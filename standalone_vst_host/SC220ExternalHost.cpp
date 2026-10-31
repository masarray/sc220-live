#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>

// Independent shipping boundary: no private ASK-P headers, generators or SDK.
// Each voice/music effect is a normal externally installed VST3 bundle.
namespace sc220
{
struct StereoPlugin
{
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String displayName;
    std::atomic<bool> bypass { false };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wetMix;
    juce::AudioBuffer<float> dry;
    juce::AudioBuffer<float> delayedDry;
    int writeIndex = 0;
    int latency = 0;
    bool ready = false;

    void prepare(double rate, int block)
    {
        if (instance == nullptr) return;
        instance->setRateAndBufferSizeDetails(rate, block);
        instance->prepareToPlay(rate, block);
        instance->reset();
        latency = juce::jlimit(0, 8192, instance->getLatencySamples());
        dry.setSize(2, block, false, true, false);
        delayedDry.setSize(2, latency + 1, false, true, false);
        delayedDry.clear();
        writeIndex = 0;
        wetMix.reset(rate, 0.012);
        wetMix.setCurrentAndTargetValue(bypass.load() ? 0.0f : 1.0f);
        ready = true;
    }
    void swapPrepared(StereoPlugin& other)
    {
        // Called on UI thread with audio callbacks quiesced. No DSP scratch
        // is allocated, and old instance retirement happens after the gate.
        using std::swap;
        instance.swap(other.instance);
        displayName.swapWith(other.displayName);
        dry.swapWith(other.dry);
        delayedDry.swapWith(other.delayedDry);
        swap(writeIndex, other.writeIndex);
        swap(latency, other.latency);
        swap(ready, other.ready);
        swap(wetMix, other.wetMix);
        const bool oldBypass = bypass.exchange(other.bypass.load(std::memory_order_relaxed));
        other.bypass.store(oldBypass, std::memory_order_relaxed);
    }
    void release()
    {
        if (instance && ready) instance->releaseResources();
        ready = false;
    }
    void clear()
    {
        release();
        instance.reset();
        displayName.clear();
    }
    void process(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) noexcept
    {
        if (!ready || !instance || audio.getNumChannels() != 2
            || audio.getNumSamples() > dry.getNumSamples()) return;
        const int n = audio.getNumSamples();
        for (int ch = 0; ch < 2; ++ch) dry.copyFrom(ch, 0, audio, ch, 0, n);
        midi.clear();
        instance->processBlock(audio, midi);
        wetMix.setTargetValue(bypass.load(std::memory_order_relaxed) ? 0.0f : 1.0f);
        const int len = delayedDry.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float wet = wetMix.getNextValue();
            int read = writeIndex - latency;
            if (read < 0) read += len; // Bounded ring read; avoid per-sample division.
            for (int ch = 0; ch < 2; ++ch)
            {
                delayedDry.setSample(ch, writeIndex, dry.getSample(ch, i));
                const auto d = delayedDry.getSample(ch, read);
                const auto w = audio.getSample(ch, i);
                audio.setSample(ch, i, (std::isfinite(w) ? w : 0.0f) * wet + d * (1.0f - wet));
            }
            if (++writeIndex == len) writeIndex = 0;
        }
    }
};

class Engine final : public juce::AudioIODeviceCallback
{
public:
    Engine()
    {
        formats.addFormat(new juce::VST3PluginFormat());
        manager.initialise(2, 2, nullptr, true);
        manager.addAudioCallback(this);
    }
    ~Engine() override
    {
        manager.removeAudioCallback(this);
        manager.closeAudioDevice();
        for (auto& channel : racks)
            for (auto& effect : channel) effect.clear();
    }
    juce::AudioDeviceManager& devices() noexcept { return manager; }
    static constexpr int slotsPerChannel = 3;
    static constexpr int channelCount = 2;
    StereoPlugin& slotAt(int lane, int slot) noexcept
    {
        return racks[static_cast<size_t>(lane)][static_cast<size_t>(slot)];
    }
    const StereoPlugin& slotAt(int lane, int slot) const noexcept
    {
        return racks[static_cast<size_t>(lane)][static_cast<size_t>(slot)];
    }
    juce::String pluginName(int lane, int slot) const
    {
        const juce::ScopedLock lock(controlLock);
        return slotAt(lane, slot).displayName;
    }
    bool hasPlugin(int lane, int slot) const
    {
        const juce::ScopedLock lock(controlLock);
        return slotAt(lane, slot).instance != nullptr;
    }
    void setBypass(int lane, int slot, bool enabled) noexcept
    {
        slotAt(lane, slot).bypass.store(enabled, std::memory_order_relaxed);
    }
    bool isBypassed(int lane, int slot) const noexcept
    {
        return slotAt(lane, slot).bypass.load(std::memory_order_relaxed);
    }
    float meter(int lane) const noexcept
    {
        return (lane == 0 ? voicePeak : musicPeak).load(std::memory_order_relaxed);
    }
    void setGain(int lane, float gain) noexcept
    {
        (lane == 0 ? voiceGain : musicGain).store(gain, std::memory_order_relaxed);
    }
    void setStreamMuted(bool muted) noexcept { streamMuted.store(muted); }
    bool isStreamMuted() const noexcept { return streamMuted.load(); }
    int deadlineMisses() const noexcept { return misses.load(); }
    juce::String currentBackend() const { return manager.getCurrentAudioDeviceType(); }
    double bufferPeriodMs() const noexcept
    {
        const auto rate = sampleRate.load(std::memory_order_relaxed);
        return rate > 0.0 ? 1000.0 * blockSize.load(std::memory_order_relaxed) / rate : 0.0;
    }
    int pluginLatencySamples(int lane) const
    {
        const juce::ScopedLock lock(controlLock);
        int total = 0;
        for (const auto& slot : racks[static_cast<size_t>(lane)])
            total += slot.latency;
        return total;
    }
    bool quiesceCallbacks() noexcept
    {
        rackSwapPause.store(true, std::memory_order_seq_cst);
        for (int attempt = 0; attempt < 150; ++attempt)
        {
            if (activeCallbacks.load(std::memory_order_seq_cst) == 0)
                return true;
            juce::Thread::sleep(1); // Only GUI thread; never realtime.
        }
        rackSwapPause.store(false, std::memory_order_seq_cst);
        return false;
    }
    juce::String loadPlugin(int lane, int slotIndex, const juce::File& file)
    {
        if (!file.exists() || !file.hasFileExtension(".vst3"))
            return "Please choose an installed .vst3 bundle.";
        const double rate = sampleRate.load(std::memory_order_acquire);
        const int block = blockSize.load(std::memory_order_acquire);
        if (rate < 8000.0 || block < 1) return "Open an audio device first.";
        juce::VST3PluginFormat vst;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        vst.findAllTypesForFile(descriptions, file.getFullPathName());
        juce::String failure;
        std::unique_ptr<juce::AudioPluginInstance> next;
        for (auto* description : descriptions)
        {
            if (description == nullptr || description->isInstrument) continue;
            next = formats.createPluginInstance(*description, rate, block, failure);
            if (next) break;
        }
        if (!next) return failure.isEmpty() ? "No compatible VST3 effect was found." : failure;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        if (!next->setBusesLayout(layout) || next->getTotalNumInputChannels() != 2
            || next->getTotalNumOutputChannels() != 2)
            return "This VST3 must support stereo input and output.";

        StereoPlugin candidate;
        candidate.displayName = next->getName();
        candidate.instance = std::move(next);
        candidate.prepare(rate, block); // Host preparation outside audio callback.
        if (candidate.latency > 8192)
            return "The VST3 declares more than 8192 samples of plugin latency.";
        if (rate != sampleRate.load(std::memory_order_acquire)
            || block != blockSize.load(std::memory_order_acquire))
            return "Audio device changed while loading VST3; try again.";
        if (!quiesceCallbacks())
            return "Audio callback would not quiesce; active FX slot was preserved.";
        {
            const juce::ScopedLock lock(controlLock);
            slotAt(lane, slotIndex).swapPrepared(candidate);
        }
        rackSwapPause.store(false, std::memory_order_seq_cst);
        candidate.clear(); // Old plugin retired on UI thread after resuming audio.
        return {};
    }
    void removePlugin(int lane, int slotIndex)
    {
        if (!quiesceCallbacks()) return;
        StereoPlugin retired;
        {
            const juce::ScopedLock lock(controlLock);
            slotAt(lane, slotIndex).swapPrepared(retired);
        }
        rackSwapPause.store(false, std::memory_order_seq_cst);
        retired.clear();
    }
    juce::AudioPluginInstance* processor(int lane, int slotIndex)
    {
        const juce::ScopedLock lock(controlLock);
        return slotAt(lane, slotIndex).instance.get();
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const auto rate = device->getCurrentSampleRate();
        const auto block = juce::jmax(64, device->getCurrentBufferSizeSamples());
        sampleRate.store(rate);
        blockSize.store(block);
        voiceBuffer.setSize(2, block, false, true, false);
        musicBuffer.setSize(2, block, false, true, false);
        for (auto& channel : racks)
            for (auto& effect : channel) effect.prepare(rate, block);
        misses.store(0);
    }
    void audioDeviceStopped() override
    {
        for (auto& channel : racks)
            for (auto& effect : channel) effect.release();
        sampleRate.store(0.0);
    }
    void audioDeviceIOCallbackWithContext(const float* const* in, int inChannels,
        float* const* out, int outChannels, int n,
        const juce::AudioIODeviceCallbackContext&) override
    {
        struct ScopedCallback
        {
            explicit ScopedCallback(std::atomic<int>& count) : callbacks(count)
            { callbacks.fetch_add(1, std::memory_order_seq_cst); }
            ~ScopedCallback() { callbacks.fetch_sub(1, std::memory_order_seq_cst); }
            std::atomic<int>& callbacks;
        } callbackGuard(activeCallbacks);
        const auto begin = juce::Time::getHighResolutionTicks();
        for (int ch = 0; ch < outChannels; ++ch)
            if (out[ch]) juce::FloatVectorOperations::clear(out[ch], n);
        if (rackSwapPause.load(std::memory_order_seq_cst)
            || n > voiceBuffer.getNumSamples() || n <= 0) return;
        voiceBuffer.clear();
        musicBuffer.clear();
        // Default routing: hardware mic = input 1/2, music = input 3/4 if
        // available on a multichannel ASIO interface. Never duplicate
        // microphone samples into the Music lane.
        for (int ch = 0; ch < 2; ++ch)
        {
            if (ch < inChannels && in[ch])
                voiceBuffer.copyFrom(ch, 0, in[ch], n);
            if (ch + 2 < inChannels && in[ch + 2])
                musicBuffer.copyFrom(ch, 0, in[ch + 2], n);
        }
        juce::MidiBuffer midi;
        for (auto& effect : racks[0]) effect.process(voiceBuffer, midi);
        for (auto& effect : racks[1]) effect.process(musicBuffer, midi);
        const auto vg = voiceGain.load(std::memory_order_relaxed);
        const auto mg = musicGain.load(std::memory_order_relaxed);
        voiceBuffer.applyGain(vg);
        musicBuffer.applyGain(mg);
        voicePeak.store(juce::jmax(voiceBuffer.getMagnitude(0, n), 0.0f));
        musicPeak.store(juce::jmax(musicBuffer.getMagnitude(0, n), 0.0f));
        if (!streamMuted.load(std::memory_order_relaxed))
        {
            for (int ch = 0; ch < juce::jmin(outChannels, 2); ++ch)
            {
                if (!out[ch]) continue;
                juce::FloatVectorOperations::add(out[ch], voiceBuffer.getReadPointer(ch), n);
                juce::FloatVectorOperations::add(out[ch], musicBuffer.getReadPointer(ch), n);
                // Conservative emergency output clip protection.
                juce::FloatVectorOperations::clip(out[ch], out[ch], -0.98f, 0.98f, n);
            }
        }
        const auto seconds = juce::Time::highResolutionTicksToSeconds(
            juce::Time::getHighResolutionTicks() - begin);
        const double rate = sampleRate.load(std::memory_order_relaxed);
        if (rate > 0 && seconds > static_cast<double>(n) / rate)
            misses.fetch_add(1, std::memory_order_relaxed);
    }
private:
    juce::AudioDeviceManager manager;
    juce::AudioPluginFormatManager formats;
    mutable juce::CriticalSection controlLock;
    std::array<std::array<StereoPlugin, slotsPerChannel>, channelCount> racks;
    juce::AudioBuffer<float> voiceBuffer, musicBuffer;
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<float> voiceGain { 1.0f }, musicGain { 1.0f };
    std::atomic<float> voicePeak { 0.0f }, musicPeak { 0.0f };
    std::atomic<bool> streamMuted { true };
    std::atomic<int> misses { 0 };
    std::atomic<bool> rackSwapPause { false };
    std::atomic<int> activeCallbacks { 0 };
};

class Console final : public juce::Component, private juce::Timer
{
public:
    Console()
    {
        setSize(960, 672);
        addAndMakeVisible(title);
        title.setText("SC220  /  LIVE MIXER", juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(19.0f)));
        title.setColour(juce::Label::textColourId, juce::Colour(0xffe4eaf4));
        addAndMakeVisible(subtitle);
        subtitle.setText("ASIO / WASAPI     ·     3 FX INSERTS PER CHANNEL     ·     BUNDLED VST3",
                         juce::dontSendNotification);
        subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff8596aa));
        addAndMakeVisible(setup);
        setup.setButtonText("AUDIO SETUP");
        setup.onClick = [this]
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                engine.devices(), 1, 4, 1, 2, false, false, true, false);
            selector->setSize(650, 440);
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "SC220 Audio Setup / ASIO";
            options.dialogBackgroundColour = juce::Colour(0xff202833);
            options.launchAsync();
        };
        for (int lane = 0; lane < 2; ++lane)
            initialiseLane(lane, lane == 0 ? "VOICE  ·  INPUT 1/2"
                                          : "MUSIC  ·  INPUT 3/4");

        addAndMakeVisible(stream);
        stream.setButtonText("GO LIVE");
        stream.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff287f77));
        stream.setClickingTogglesState(true);
        stream.onClick = [this]
        {
            engine.setStreamMuted(!stream.getToggleState());
            stream.setButtonText(stream.getToggleState() ? "ON AIR" : "GO LIVE");
        };
        addAndMakeVisible(health);
        health.setColour(juce::Label::textColourId, juce::Colour(0xff9cadbf));
        health.setJustificationType(juce::Justification::centredLeft);

        autoLoadBundledPlugins();
        startTimerHz(12);
    }
    ~Console() override
    {
        stopTimer();
        for (int lane = 0; lane < 2; ++lane)
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
                closeEditor(lane, slot);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff121820));
        g.setColour(juce::Colour(0xff293746));
        g.drawLine(24.0f, 98.0f, static_cast<float>(getWidth() - 24), 98.0f);
        for (int lane = 0; lane < 2; ++lane)
        {
            const auto r = juce::Rectangle<float>(22.0f + 468.0f * lane,
                                                   116.0f, 446.0f, 444.0f);
            g.setColour(juce::Colour(0xff1d2632));
            g.fillRoundedRectangle(r, 9.0f);
            g.setColour(juce::Colour(0xff304153));
            g.drawRoundedRectangle(r, 9.0f, 1.0f);
            g.setColour(juce::Colour(0xff8396ac));
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
                g.drawLine(r.getX() + 15.0f, 236.0f + slot * 83.0f,
                           r.getRight() - 15.0f, 236.0f + slot * 83.0f, 0.5f);
        }
    }
    void resized() override
    {
        title.setBounds(28, 15, 490, 34);
        subtitle.setBounds(30, 55, getWidth() - 240, 25);
        setup.setBounds(getWidth() - 185, 35, 150, 34);
        for (int lane = 0; lane < 2; ++lane)
        {
            const int x = 42 + lane * 468;
            laneTitles[lane].setBounds(x, 134, 400, 32);
            rackTitles[lane].setBounds(x, 164, 390, 23);
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
            {
                const int y = 193 + slot * 83;
                auto& fx = controls[lane][slot];
                fx.name.setBounds(x + 4, y, 394, 23);
                fx.load.setBounds(x + 4, y + 29, 97, 29);
                fx.edit.setBounds(x + 109, y + 29, 76, 29);
                fx.bypass.setBounds(x + 193, y + 29, 96, 29);
                fx.remove.setBounds(x + 297, y + 29, 90, 29);
            }
            gains[lane].setBounds(x, 475, 390, 33);
            meters[lane].setBounds(x, 519, 390, 18);
        }
        health.setBounds(30, 583, getWidth() - 235, 61);
        stream.setBounds(getWidth() - 203, 582, 166, 48);
    }
private:
    struct FxControls
    {
        juce::Label name;
        juce::TextButton load, edit, bypass, remove;
    };
    void closeEditor(int lane, int slot)
    {
        if (auto* window = editors[lane][slot].getComponent())
            delete window; // The editor must die before its AudioPluginInstance.
        editors[lane][slot] = nullptr;
    }
    void openEditor(int lane, int slot)
    {
        if (auto* existing = editors[lane][slot].getComponent())
        {
            existing->toFront(true);
            return;
        }
        auto* processor = engine.processor(lane, slot);
        if (processor == nullptr) return;
        auto* editor = processor->createEditorIfNeeded();
        if (editor == nullptr)
            editor = new juce::GenericAudioProcessorEditor(*processor);
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = engine.pluginName(lane, slot);
        options.dialogBackgroundColour = juce::Colour(0xff202833);
        options.content.setOwned(editor);
        options.resizable = true;
        editors[lane][slot] = options.launchAsync();
    }
    void initialiseLane(int lane, const juce::String& heading)
    {
        laneTitles[lane].setText(heading, juce::dontSendNotification);
        laneTitles[lane].setColour(juce::Label::textColourId, juce::Colour(0xffa5d4f2));
        addAndMakeVisible(laneTitles[lane]);
        rackTitles[lane].setText("FX INSERT RACK  /  3 SLOTS  /  SERIAL",
                                 juce::dontSendNotification);
        rackTitles[lane].setColour(juce::Label::textColourId, juce::Colour(0xff91a5ba));
        addAndMakeVisible(rackTitles[lane]);

        for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
        {
            auto& fx = controls[lane][slot];
            addAndMakeVisible(fx.name);
            fx.name.setText(juce::String(slot + 1) + "   EMPTY SLOT", juce::dontSendNotification);
            fx.name.setColour(juce::Label::textColourId, juce::Colour(0xffd8e2f1));
            addAndMakeVisible(fx.load);
            fx.load.setButtonText("+ LOAD");
            fx.load.onClick = [this, lane, slot]
            {
                chooser = std::make_unique<juce::FileChooser>(
                    "Select a trusted 64-bit VST3 effect", juce::File(), "*.vst3");
                juce::Component::SafePointer<Console> safe(this);
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::canSelectDirectories,
                    [safe, lane, slot](const juce::FileChooser& selection)
                    {
                        if (safe == nullptr) return;
                        const auto file = selection.getResult();
                        if (file == juce::File()) return;
                        safe->closeEditor(lane, slot);
                        const auto error = safe->engine.loadPlugin(lane, slot, file);
                        if (error.isNotEmpty())
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "VST3 load failed", error);
                        safe->updateSlotNames();
                    });
            };
            addAndMakeVisible(fx.edit);
            fx.edit.setButtonText("EDIT");
            fx.edit.onClick = [this, lane, slot] { openEditor(lane, slot); };
            addAndMakeVisible(fx.bypass);
            fx.bypass.setButtonText("BYPASS");
            fx.bypass.setClickingTogglesState(true);
            fx.bypass.onClick = [this, lane, slot]
            {
                engine.setBypass(lane, slot, controls[lane][slot].bypass.getToggleState());
            };
            addAndMakeVisible(fx.remove);
            fx.remove.setButtonText("CLEAR");
            fx.remove.onClick = [this, lane, slot]
            {
                closeEditor(lane, slot);
                engine.removePlugin(lane, slot);
                controls[lane][slot].bypass.setToggleState(false, juce::dontSendNotification);
                updateSlotNames();
            };
        }
        addAndMakeVisible(gains[lane]);
        gains[lane].setRange(-60.0, 6.0, 0.1);
        gains[lane].setValue(0.0);
        gains[lane].setTextValueSuffix(" dB  /  CHANNEL");
        gains[lane].setSliderStyle(juce::Slider::LinearHorizontal);
        gains[lane].onValueChange = [this, lane]
        {
            engine.setGain(lane, juce::Decibels::decibelsToGain(
                static_cast<float>(gains[lane].getValue())));
        };
        addAndMakeVisible(meters[lane]);
        meters[lane].setRange(0.0, 1.0);
        meters[lane].setValue(0.0);
        meters[lane].setSliderStyle(juce::Slider::LinearBar);
        meters[lane].setEnabled(false);
    }
    void updateSlotNames()
    {
        for (int lane = 0; lane < 2; ++lane)
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
            {
                const auto name = engine.pluginName(lane, slot);
                controls[lane][slot].name.setText(
                    juce::String(slot + 1) + "   " +
                        (name.isEmpty() ? juce::String("EMPTY SLOT") : name),
                    juce::dontSendNotification);
                controls[lane][slot].bypass.setToggleState(
                    engine.isBypassed(lane, slot), juce::dontSendNotification);
            }
    }
    void autoLoadBundledPlugins()
    {
        const auto folder = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory().getChildFile("plugins");
        juce::Array<juce::File> bundles;
        if (folder.isDirectory())
            folder.findChildFiles(bundles, juce::File::findFilesAndDirectories, true, "*.vst3");
        juce::File enhancer, reverb;
        for (const auto& file : bundles)
        {
            if (file.getFileName().containsIgnoreCase("arsonkupik"))
                enhancer = file;
            if (file.getFileName().containsIgnoreCase("arsonverb"))
                reverb = file;
        }
        if (enhancer.exists())
        {
            for (int lane = 0; lane < 2; ++lane)
            {
                const auto error = engine.loadPlugin(lane, 0, enhancer);
                if (error.isNotEmpty())
                    startupError = "ArSonKuPik: " + error;
            }
        }
        if (reverb.exists())
        {
            const auto error = engine.loadPlugin(0, 1, reverb);
            if (error.isNotEmpty())
                startupError = "ArSonVerb: " + error;
        }
        updateSlotNames();
    }
    void timerCallback() override
    {
        for (int lane = 0; lane < 2; ++lane)
            meters[lane].setValue(engine.meter(lane), juce::dontSendNotification);
        health.setText("DSP callback deadline misses: " + juce::String(engine.deadlineMisses())
            + "     ·     Output: "
            + juce::String(engine.isStreamMuted() ? "MUTED (safe)" : "LIVE")
            + (startupError.isEmpty() ? juce::String()
                : juce::String("\nBundled plugin error: ") + startupError),
            juce::dontSendNotification);
    }
    Engine engine;
    juce::Label title, subtitle, health;
    juce::TextButton setup, stream;
    std::array<juce::Label, 2> laneTitles, rackTitles;
    std::array<std::array<FxControls, Engine::slotsPerChannel>, 2> controls;
    std::array<juce::Slider, 2> gains, meters;
    std::array<std::array<juce::Component::SafePointer<juce::DialogWindow>,
                          Engine::slotsPerChannel>, 2> editors;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String startupError;
};

class Window final : public juce::DocumentWindow
{
public:
    Window() : juce::DocumentWindow("SC220 Live", juce::Colour(0xff141920),
        juce::DocumentWindow::allButtons, true)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new Console(), true);
        setResizable(true, true);
        centreWithSize(960, 672);
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SC220 Live"; }
    const juce::String getApplicationVersion() override { return "0.1.2"; }
    void initialise(const juce::String&) override { window = std::make_unique<Window>(); }
    void shutdown() override { window.reset(); }
private:
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(sc220::Application)
