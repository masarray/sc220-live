#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include "SC220PremiumTheme.h"

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
        swap(dry, other.dry); // juce::AudioBuffer has noexcept move, not swapWith.
        swap(delayedDry, other.delayedDry);
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
    // Bind signature controls to PUBLIC VST3 parameters, never private ASK-P DSP.
    static juce::AudioProcessorParameter* findMacro(juce::AudioPluginInstance* plugin,
                                                    const juce::String& label)
    {
        if (plugin == nullptr) return nullptr;
        const auto key = label.removeCharacters(" _-").toLowerCase();
        for (auto* param : plugin->getParameters())
        {
            if (param != nullptr &&
                param->getName(128).removeCharacters(" _-").toLowerCase() == key)
                return param;
        }
        return nullptr;
    }
    float signatureMacro(int lane, const juce::String& label) const
    {
        const juce::ScopedLock lock(controlLock); // UI-only; never audio callback.
        if (auto* param = findMacro(slotAt(lane, 0).instance.get(), label))
            return param->getValue() * 100.0f;
        return -1.0f; // Parameter not exposed by this VST3.
    }
    bool setSignatureMacro(int lane, const juce::String& label, float percent)
    {
        const juce::ScopedLock lock(controlLock);
        if (auto* param = findMacro(slotAt(lane, 0).instance.get(), label))
        {
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, percent / 100.0f));
            return true;
        }
        return false;
    }
    float meter(int lane, int channel) const noexcept
    {
        if (lane == 2)
            return masterPeak[static_cast<size_t>(channel)].load(std::memory_order_relaxed);
        return lanePeak[static_cast<size_t>(lane)][static_cast<size_t>(channel)]
            .load(std::memory_order_relaxed);
    }
    void setMasterGain(float gain) noexcept
    {
        masterGain.store(gain, std::memory_order_relaxed);
    }
    bool deviceAvailable() const noexcept
    {
        return sampleRate.load(std::memory_order_acquire) >= 8000.0;
    }
    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getBufferSize() const noexcept { return blockSize.load(); }
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
        for (int channel = 0; channel < 2; ++channel)
        {
            lanePeak[0][static_cast<size_t>(channel)].store(
                voiceBuffer.getMagnitude(channel, 0, n), std::memory_order_relaxed);
            lanePeak[1][static_cast<size_t>(channel)].store(
                musicBuffer.getMagnitude(channel, 0, n), std::memory_order_relaxed);
        }
        if (!streamMuted.load(std::memory_order_relaxed))
        {
            for (int ch = 0; ch < juce::jmin(outChannels, 2); ++ch)
            {
                if (!out[ch]) continue;
                juce::FloatVectorOperations::add(out[ch], voiceBuffer.getReadPointer(ch), n);
                juce::FloatVectorOperations::add(out[ch], musicBuffer.getReadPointer(ch), n);
                juce::FloatVectorOperations::multiply(out[ch],
                    masterGain.load(std::memory_order_relaxed), n);
                // Emergency fixed ceiling; not represented as a brickwall TP limiter.
                juce::FloatVectorOperations::clip(out[ch], out[ch], -0.98f, 0.98f, n);
                masterPeak[static_cast<size_t>(ch)].store(
                    juce::FloatVectorOperations::findMaximum(out[ch], n),
                    std::memory_order_relaxed);
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
    std::array<std::array<std::atomic<float>, 2>, 2> lanePeak {};
    std::array<std::atomic<float>, 2> masterPeak {};
    std::atomic<float> masterGain { 1.0f };
    std::atomic<bool> streamMuted { true };
    std::atomic<int> misses { 0 };
    std::atomic<bool> rackSwapPause { false };
    std::atomic<int> activeCallbacks { 0 };
};

// Non-modal DAW-style plugin editor. Own the editor window explicitly, so
// CLEAR/REPLACE destroys the UI before the corresponding VST3 processor.
class FxEditorWindow final : public juce::DocumentWindow
{
public:
    explicit FxEditorWindow(juce::AudioProcessor& processor)
        : juce::DocumentWindow(processor.getName(), juce::Colour(0xff202833),
                               juce::DocumentWindow::closeButton, true)
    {
        auto* editor = processor.createEditorIfNeeded();
        if (editor == nullptr)
            editor = new juce::GenericAudioProcessorEditor(processor);
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        setResizable(true, true);
        centreWithSize(juce::jmax(360, editor->getWidth()),
                       juce::jmax(300, editor->getHeight() + 32));
        setVisible(true);
    }
    void closeButtonPressed() override { setVisible(false); }
};

class Console final : public juce::Component, private juce::Timer
{
public:
    Console()
    {
        setOpaque(true);
        setSize(1376, 816);
        setLookAndFeel(&theme);
        brandLogo = visual::image("recording_tech_logo_png");
        sc220Artwork = visual::image("sc220_mkii_device_alpha_png");

        addAndMakeVisible(setup);
        setup.setButtonText("AUDIO SETUP");
        setup.setTooltip("Select ASIO, WASAPI, active input/output channels and buffer size");
        setup.onClick = [this]
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                engine.devices(), 1, 4, 1, 2, false, false, true, false);
            selector->setSize(670, 470);
            juce::DialogWindow::LaunchOptions options;
            options.content.setOwned(selector);
            options.dialogTitle = "SC220 | Audio Device / ASIO";
            options.dialogBackgroundColour = visual::panel;
            options.launchAsync();
        };
        for (int lane = 0; lane < Engine::channelCount; ++lane)
        {
            auto& fader = channelFaders[lane];
            addAndMakeVisible(fader);
            fader.setSliderStyle(juce::Slider::LinearVertical);
            fader.setRange(-60.0, 6.0, 0.1);
            fader.setValue(0.0, juce::dontSendNotification);
            fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            fader.setDoubleClickReturnValue(true, 0.0);
            fader.setTooltip("Channel fader in dB. Double-click returns to unity.");
            fader.onValueChange = [this, lane]
            {
                engine.setGain(lane, juce::Decibels::decibelsToGain(
                    static_cast<float>(channelFaders[lane].getValue())));
            };
            addAndMakeVisible(levelMeters[lane]);
            addAndMakeVisible(faderLabels[lane]);
            faderLabels[lane].setJustificationType(juce::Justification::centred);
            faderLabels[lane].setColour(juce::Label::textColourId, visual::cyan);
            fader.onValueChange(); // Initialise numeric fader setting.
            addAndMakeVisible(muteButtons[lane]);
            muteButtons[lane].setButtonText("MUTE");
            muteButtons[lane].setClickingTogglesState(true);
            muteButtons[lane].onClick = [this,lane]
            {
                engine.setGain(lane, muteButtons[lane].getToggleState()
                    ? 0.0f : juce::Decibels::decibelsToGain(
                        static_cast<float>(channelFaders[lane].getValue())));
            };
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
                createFxSlot(lane, slot);
        }

        addAndMakeVisible(masterFader);
        masterFader.setSliderStyle(juce::Slider::LinearVertical);
        masterFader.setRange(-60.0, 6.0, 0.1);
        masterFader.setValue(0.0, juce::dontSendNotification);
        masterFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        masterFader.setDoubleClickReturnValue(true, 0.0);
        masterFader.onValueChange = [this]
        {
            engine.setMasterGain(juce::Decibels::decibelsToGain(
                static_cast<float>(masterFader.getValue())));
        };
        addAndMakeVisible(levelMeters[2]);
        addAndMakeVisible(masterLabel);
        masterLabel.setJustificationType(juce::Justification::centred);
        masterLabel.setColour(juce::Label::textColourId, visual::cyan);
        addAndMakeVisible(goLive);
        goLive.setButtonText("GO LIVE");
        goLive.setClickingTogglesState(true);
        goLive.onClick = [this]
        {
            engine.setStreamMuted(!goLive.getToggleState());
            goLive.setButtonText(goLive.getToggleState() ? "ON AIR" : "GO LIVE");
        };
        addAndMakeVisible(notification);
        notification.setColour(juce::Label::textColourId, visual::amber);
        notification.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(signatureVoice);
        addAndMakeVisible(signatureMusic);
        signatureVoice.setButtonText("VOICE");
        signatureMusic.setButtonText("MUSIC");
        signatureVoice.setClickingTogglesState(true);
        signatureMusic.setClickingTogglesState(true);
        signatureVoice.setRadioGroupId(2026);
        signatureMusic.setRadioGroupId(2026);
        signatureVoice.onClick=[this]{signatureLane=0;refreshSignatureKnobs();};
        signatureMusic.onClick=[this]{signatureLane=1;refreshSignatureKnobs();};
        signatureVoice.setToggleState(true,juce::dontSendNotification);
        for(int index=0;index<5;++index)
        {
            auto& knob=signatureKnobs[static_cast<size_t>(index)];
            addAndMakeVisible(knob);
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setRange(0.0,100.0,0.1);
            knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            knob.setDoubleClickReturnValue(false,0.0);
            knob.setMouseDragSensitivity(160);
            knob.setTooltip(macroNames[static_cast<size_t>(index)]+" - hosted ArSonKuPik VST3");
            knob.onValueChange=[this,index]
            {
                engine.setSignatureMacro(signatureLane,
                    macroNames[static_cast<size_t>(index)],
                    static_cast<float>(signatureKnobs[static_cast<size_t>(index)].getValue()));
            };
        }
        autoLoadBundledPlugins();
        updateSlotNames();
        resized();
        startTimerHz(20);
    }
    ~Console() override
    {
        stopTimer();
        for (int lane = 0; lane < Engine::channelCount; ++lane)
            for (int slot = 0; slot < Engine::slotsPerChannel; ++slot)
                editors[lane][slot].reset();
        setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics& g) override
    {
        using namespace visual;
        g.fillAll(canvas);
        g.setGradientFill(juce::ColourGradient(header, 0, 0,
                                                juce::Colour(0xff182029),0,64,false));
        g.fillRect(0,0,getWidth(),64);
        g.setColour(border);
        g.drawHorizontalLine(63,0.0f,static_cast<float>(getWidth()));
        if (brandLogo.isValid())
            g.drawImageWithin(brandLogo, 18, 10, 136, 43,
                              juce::RectanglePlacement::centred, false);
        else
            caption(g,"RECORDING TECH",{19,11,137,40},13.0f,text);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawVerticalLine(169,13.0f,51.0f);
        caption(g,"SC220",{182,14,77,34},19.0f,text);
        caption(g,"LIVE",{257,14,70,34},19.0f,orange);
        g.drawVerticalLine(349,13.0f,51.0f);
        caption(g,"LIVE MIXER  /  VST3 ENGINE",{366,10,330,24},12.5f,text);
        caption(g,"INDEPENDENT DSP  /  NATIVE ASIO",{366,34,328,19},10.2f,muted);
        g.setColour(engine.deviceAvailable()?green:amber);
        g.fillEllipse(getWidth()-381.0f,28.0f,8.0f,8.0f);
        caption(g,engine.deviceAvailable()?"AUDIO ACTIVE":"DEVICE SETUP",
            {getWidth()-367,16,118,30},10.5f,
            engine.deviceAvailable()?green:amber);
        caption(g,juce::String(engine.getSampleRate()/1000.0,1)+" kHz",
            {getWidth()-261,17,74,28},11.0f,text);

        panelSurface(g,leftPanel.toFloat(),7.0f);
        panelSurface(g,centerPanel.toFloat(),7.0f);
        panelSurface(g,rightPanel.toFloat(),7.0f);
        caption(g,"INPUT MIX",leftPanel.withHeight(46).reduced(17,0),15.0f,text);
        caption(g,"FX INSERT RACK",centerPanel.withHeight(47).reduced(17,0),15.0f,text);
        caption(g,"STREAM MASTER",rightPanel.withHeight(46).reduced(15,0),14.0f,text);
        caption(g,"VOICE + MUSIC  /  SERIAL VST3", {centerPanel.getRight()-299,
            centerPanel.getY()+15,283,22},10.0f,muted);
        drawInput(g);
        drawRack(g);
        drawMaster(g);

        g.setColour(panelHi);
        g.fillRect(0,footerTop,getWidth(),getHeight()-footerTop);
        g.setColour(border);
        g.drawHorizontalLine(footerTop,0.0f,static_cast<float>(getWidth()));
        caption(g,"ENGINE / "+engine.currentBackend().toUpperCase(),
                {18,footerTop+7,193,22},10.5f,cyan);
        caption(g,juce::String(engine.getSampleRate()/1000.0,1)+" kHz  /  "
                  +juce::String(engine.getBufferSize())+" SAMPLES",
                {210,footerTop+7,211,22},10.4f,text);
        caption(g,"VOICE FX "+juce::String(engine.pluginLatencySamples(0))+" smp",
                {430,footerTop+7,140,22},10.0f,muted);
        caption(g,"MUSIC FX "+juce::String(engine.pluginLatencySamples(1))+" smp",
                {566,footerTop+7,138,22},10.0f,muted);
        caption(g,"CALLBACK DEADLINES  "+juce::String(engine.deadlineMisses()),
                {725,footerTop+7,223,22},10.0f,muted);
        caption(g,"LOW-LATENCY LIVE AUDIO  /  PREVIEW",
                {getWidth()-336,footerTop+7,324,22},10.0f,amber,
                true,juce::Justification::centredRight);
    }
    void resized() override
    {
        const int bodyY = 77;
        footerTop = getHeight()-47;
        const int bodyH = footerTop-bodyY-12;
        leftPanel = {12, bodyY, 313, bodyH};
        rightPanel = {getWidth()-249, bodyY, 237, bodyH};
        centerPanel = {leftPanel.getRight()+11,bodyY,
                       rightPanel.getX()-leftPanel.getRight()-22,bodyH};

        setup.setBounds(getWidth()-150,17,133,31);
        const int stripY = leftPanel.getY()+169;
        for(int lane=0;lane<2;++lane)
        {
            auto r = juce::Rectangle<int>(leftPanel.getX()+10+lane*147,stripY,142,
                                           leftPanel.getHeight()-183);
            channelFaders[lane].setBounds(r.getX()+19,r.getY()+105,65,
                                          juce::jmax(180,r.getHeight()-197));
            levelMeters[lane].setBounds(r.getX()+99,r.getY()+108,34,
                                        juce::jmax(180,r.getHeight()-202));
            faderLabels[lane].setBounds(r.getX()+7,r.getBottom()-80,126,25);
            muteButtons[lane].setBounds(r.getX()+13,r.getBottom()-45,116,29);
        }
        const int usable = centerPanel.getWidth()-39;
        const int cardW = (usable-20)/3;
        for(int lane=0;lane<2;++lane)
        for(int slot=0;slot<3;++slot)
        {
            auto r = juce::Rectangle<int>(
                centerPanel.getX()+13+slot*(cardW+10),
                centerPanel.getY()+99+lane*230,cardW,175);
            auto& c=fx[lane][slot];
            c.name.setBounds(r.getX()+11,r.getY()+49,r.getWidth()-22,29);
            const int w = (r.getWidth()-30)/2;
            c.load.setBounds(r.getX()+10,r.getY()+105,w,29);
            c.edit.setBounds(r.getX()+20+w,r.getY()+105,w,29);
            c.bypass.setBounds(r.getX()+10,r.getY()+143,w,29);
            c.remove.setBounds(r.getX()+20+w,r.getY()+143,w,29);
        }
        signatureVoice.setBounds(centerPanel.getRight()-155,centerPanel.getY()+515,66,28);
        signatureMusic.setBounds(centerPanel.getRight()-83,centerPanel.getY()+515,67,28);
        const int macroWidth=(centerPanel.getWidth()-37)/5;
        for(int index=0;index<5;++index)
            signatureKnobs[static_cast<size_t>(index)].setBounds(
                centerPanel.getX()+18+index*macroWidth,
                centerPanel.getY()+546,macroWidth-5,80);
        notification.setBounds(centerPanel.getX()+20,
            centerPanel.getY()+648,centerPanel.getWidth()-40,24);

        masterFader.setBounds(rightPanel.getX()+42,rightPanel.getY()+179,
                              87,juce::jmax(204,rightPanel.getHeight()-355));
        levelMeters[2].setBounds(rightPanel.getX()+159,rightPanel.getY()+192,
                                 42,juce::jmax(204,rightPanel.getHeight()-372));
        masterLabel.setBounds(rightPanel.getX()+22,rightPanel.getBottom()-137,
                              rightPanel.getWidth()-44,29);
        goLive.setBounds(rightPanel.getX()+25,rightPanel.getBottom()-83,
                          rightPanel.getWidth()-50,41);
    }
private:
    struct FxControls
    {
        juce::Label name;
        juce::TextButton load,edit,bypass,remove;
    };
    void drawInput(juce::Graphics& g)
    {
        using namespace visual;
        if(sc220Artwork.isValid())
            g.drawImageWithin(sc220Artwork,leftPanel.getCentreX()-88,
                leftPanel.getY()+45,176,92,juce::RectanglePlacement::centred,false);
        caption(g,"SC220 / EXTERNAL AUDIO INTERFACE",
                {leftPanel.getX()+20,leftPanel.getY()+139,
                 leftPanel.getWidth()-40,20},9.5f,muted);
        const int stripY = leftPanel.getY()+169;
        for(int lane=0;lane<2;++lane)
        {
            const auto r=juce::Rectangle<float>(leftPanel.getX()+10.0f+lane*147.0f,
                static_cast<float>(stripY),142.0f,
                static_cast<float>(leftPanel.getHeight()-183));
            g.setColour(inset);
            g.fillRoundedRectangle(r,5.0f);
            g.setColour(borderSoft);
            g.drawRoundedRectangle(r,5.0f,0.8f);
            caption(g,lane==0?"VOICE":"MUSIC",
                {static_cast<int>(r.getX()+10),static_cast<int>(r.getY()+11),122,21},
                12.0f,text);
            caption(g,lane==0?"INPUT 1 / 2":"INPUT 3 / 4",
                {static_cast<int>(r.getX()+10),static_cast<int>(r.getY()+35),122,17},
                9.6f,muted);
            caption(g,"FADER",
                {static_cast<int>(r.getX()+9),static_cast<int>(r.getY()+79),67,17},
                9.4f,muted);
            caption(g,"L  R", {static_cast<int>(r.getX()+94),
                static_cast<int>(r.getY()+79),43,17},9.2f,muted);
            g.setColour(cyan.withAlpha(0.17f));
            g.drawHorizontalLine(static_cast<int>(r.getY()+64),
                                 r.getX()+10,r.getRight()-10);
        }
    }
    void drawRack(juce::Graphics& g)
    {
        using namespace visual;
        for(int lane=0;lane<2;++lane)
        {
            const int y = centerPanel.getY()+55+lane*230;
            const auto name=lane==0?"VOICE / SMART SIGNATURE":"MUSIC / SIGNATURE BUS";
            caption(g,name,{centerPanel.getX()+17,y,420,26},12.0f,text);
            g.setColour(lane==0?orange:cyan);
            g.fillRoundedRectangle(static_cast<float>(centerPanel.getX()+15),
                                   static_cast<float>(y+28),37.0f,2.0f,1.0f);
            const int usable=centerPanel.getWidth()-39;
            const int cardW=(usable-20)/3;
            for(int slot=0;slot<3;++slot)
            {
                const auto r=juce::Rectangle<float>(
                    static_cast<float>(centerPanel.getX()+13+slot*(cardW+10)),
                    static_cast<float>(centerPanel.getY()+99+lane*230),
                    static_cast<float>(cardW),175.0f);
                g.setColour(inset);
                g.fillRoundedRectangle(r,6.0f);
                g.setColour(border);
                g.drawRoundedRectangle(r,6.0f,0.8f);
                g.setColour(panelHi);
                g.fillRoundedRectangle(r.withHeight(36.0f),5.0f);
                const bool loaded=engine.hasPlugin(lane,slot);
                g.setColour(loaded?green:muted);
                g.fillEllipse(r.getX()+12.0f,r.getY()+14.0f,7.0f,7.0f);
                caption(g,"FX "+juce::String(slot+1),
                    {static_cast<int>(r.getX()+29),static_cast<int>(r.getY()+4),77,29},
                    10.3f,text);
                caption(g,loaded?"LOADED":"AVAILABLE",
                    {static_cast<int>(r.getRight()-101),static_cast<int>(r.getY()+4),
                     88,29},9.2f,loaded?green:muted,true,
                    juce::Justification::centredRight);
                if (slot<2)
                    caption(g,">",{static_cast<int>(r.getRight()+1),
                        static_cast<int>(r.getCentreY()-12),10,24},11.0f,muted);
                if (!loaded)
                    caption(g,slot==0?"ENHANCER READY":"ADD VST3 EFFECT",
                        {static_cast<int>(r.getX()+12),static_cast<int>(r.getY()+78),
                         static_cast<int>(r.getWidth()-24),23},10.0f,muted,false);
            }
        }
        g.setColour(borderSoft);
        g.drawHorizontalLine(centerPanel.getY()+512,
            static_cast<float>(centerPanel.getX()+14),
            static_cast<float>(centerPanel.getRight()-14));
        caption(g,"SMART SIGNATURE / VST3 MACROS",
            {centerPanel.getX()+20,centerPanel.getY()+516,
             centerPanel.getWidth()-204,26},12.0f,text);
        static constexpr const char* macroTitles[5] = {
            "ENHANCE","SMART BASS","SMART TREBLE","VOCAL BODY","STEREO MAGIC"
        };
        const int columnWidth=(centerPanel.getWidth()-37)/5;
        for(int index=0;index<5;++index)
        {
            const auto value=signatureKnobs[static_cast<size_t>(index)].getValue();
            caption(g,macroTitles[index],
                {centerPanel.getX()+18+index*columnWidth,
                 centerPanel.getY()+629,columnWidth-5,17},
                9.2f,signatureKnobs[static_cast<size_t>(index)].isEnabled()?text:muted,
                true,juce::Justification::centred);
            caption(g,signatureKnobs[static_cast<size_t>(index)].isEnabled()
                    ?juce::String(value,0)+"%":"N/A",
                {centerPanel.getX()+18+index*columnWidth,
                 centerPanel.getY()+642,columnWidth-5,16},
                10.2f,cyan,true,juce::Justification::centred);
        }
    }
    void drawMaster(juce::Graphics& g)
    {
        using namespace visual;
        caption(g,"OUTPUT 1 / 2",
            {rightPanel.getX()+20,rightPanel.getY()+61,rightPanel.getWidth()-40,21},
            10.5f,cyan);
        caption(g,"STREAM / HARDWARE",
            {rightPanel.getX()+20,rightPanel.getY()+87,rightPanel.getWidth()-40,20},
            9.3f,muted);
        g.setColour(border);
        g.drawHorizontalLine(rightPanel.getY()+130,
            static_cast<float>(rightPanel.getX()+18),
            static_cast<float>(rightPanel.getRight()-18));
        caption(g,"MASTER FADER",
            {rightPanel.getX()+33,rightPanel.getY()+155,120,20},10.0f,text);
        caption(g,"L   R",
            {rightPanel.getX()+156,rightPanel.getY()+165,55,20},9.2f,muted);
        caption(g,"OUTPUT TRIM",
            {rightPanel.getX()+18,rightPanel.getBottom()-165,
             rightPanel.getWidth()-36,19},10.0f,muted);
        caption(g,"SAFE MUTE AT START",
            {rightPanel.getX()+20,rightPanel.getBottom()-41,
             rightPanel.getWidth()-40,20},9.3f,muted,
            true,juce::Justification::centred);
    }
    void createFxSlot(int lane,int slot)
    {
        auto& c=fx[lane][slot];
        addAndMakeVisible(c.name);
        c.name.setColour(juce::Label::textColourId,visual::text);
        c.name.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(c.load);
        c.load.setButtonText("+ LOAD");
        c.load.onClick=[this,lane,slot]
        {
            fileChooser=std::make_unique<juce::FileChooser>(
                "Load 64-bit stereo VST3 FX",juce::File(),"*.vst3");
            juce::Component::SafePointer<Console> safe(this);
            fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::canSelectDirectories,
                [safe,lane,slot](const juce::FileChooser& fc)
                {
                    if(safe==nullptr)return;
                    const auto file=fc.getResult();
                    if(file==juce::File())return;
                    safe->closeEditor(lane,slot);
                    const auto error=safe->engine.loadPlugin(lane,slot,file);
                    if(error.isNotEmpty()) safe->startupError="VST3 LOAD FAILED: "+error;
                    else safe->startupError.clear();
                    safe->updateSlotNames();
                });
        };
        addAndMakeVisible(c.edit);
        c.edit.setButtonText("EDIT");
        c.edit.onClick=[this,lane,slot]{openEditor(lane,slot);};
        addAndMakeVisible(c.bypass);
        c.bypass.setButtonText("BYPASS");
        c.bypass.setClickingTogglesState(true);
        c.bypass.onClick=[this,lane,slot]
        {
            engine.setBypass(lane,slot,fx[lane][slot].bypass.getToggleState());
        };
        addAndMakeVisible(c.remove);
        c.remove.setButtonText("CLEAR");
        c.remove.onClick=[this,lane,slot]
        {
            closeEditor(lane,slot);
            engine.removePlugin(lane,slot);
            updateSlotNames();
        };
    }
    void closeEditor(int lane,int slot) { editors[lane][slot].reset(); }
    void openEditor(int lane,int slot)
    {
        if(auto* existing=editors[lane][slot].get())
        {
            existing->setVisible(true);
            existing->toFront(true);
            return;
        }
        if(auto* processor=engine.processor(lane,slot))
            editors[lane][slot]=std::make_unique<FxEditorWindow>(*processor);
    }
    void updateSlotNames()
    {
        for(int lane=0;lane<2;++lane)
        for(int slot=0;slot<3;++slot)
        {
            const auto name=engine.pluginName(lane,slot);
            fx[lane][slot].name.setText(
                name.isEmpty()?"Empty slot":name,juce::dontSendNotification);
            fx[lane][slot].edit.setEnabled(name.isNotEmpty());
            fx[lane][slot].bypass.setEnabled(name.isNotEmpty());
            fx[lane][slot].remove.setEnabled(name.isNotEmpty());
            fx[lane][slot].bypass.setToggleState(
                engine.isBypassed(lane,slot),juce::dontSendNotification);
        }
        refreshSignatureKnobs();
        repaint(centerPanel);
    }
    void refreshSignatureKnobs()
    {
        for(int index=0;index<5;++index)
        {
            auto& knob=signatureKnobs[static_cast<size_t>(index)];
            const auto value=engine.signatureMacro(signatureLane,
                macroNames[static_cast<size_t>(index)]);
            knob.setEnabled(value>=0.0f);
            if(value>=0.0f && !knob.isMouseOverOrDragging())
                knob.setValue(value,juce::dontSendNotification);
        }
        repaint(centerPanel);
    }
    void autoLoadBundledPlugins()
    {
        const auto folder=juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory().getChildFile("plugins");
        juce::Array<juce::File> candidates;
        if(folder.isDirectory())
            folder.findChildFiles(candidates,juce::File::findFilesAndDirectories,
                                  true,"*.vst3");
        juce::File enhancer,verb;
        for(const auto& file:candidates)
        {
            const auto path=file.getFullPathName();
            if(path.containsIgnoreCase(".vst3\\") ||
               path.containsIgnoreCase(".vst3/")) continue;
            if(file.getFileName().containsIgnoreCase("ArSonKuPik"))
                enhancer=file;
            else if(file.getFileName().containsIgnoreCase("ArSonVerb"))
                verb=file;
        }
        if(!enhancer.exists())
            startupError="ArSonKuPik not found. Check plugins/ beside EXE.";
        else
        {
            for(int lane=0;lane<2;++lane)
            {
                const auto error=engine.loadPlugin(lane,0,enhancer);
                if(error.isNotEmpty())
                    startupError="ArSonKuPik FX"+juce::String(lane+1)+": "+error;
            }
        }
        if(!verb.exists())
        {
            if(startupError.isEmpty())
                startupError="ArSonVerb not found. Check plugins/ beside EXE.";
        }
        else
        {
            const auto error=engine.loadPlugin(0,1,verb);
            if(error.isNotEmpty()) startupError="ArSonVerb: "+error;
        }
    }
    void timerCallback() override
    {
        for(int lane=0;lane<2;++lane)
        {
            levelMeters[lane].setLevels(engine.meter(lane,0),engine.meter(lane,1));
            faderLabels[lane].setText(
                juce::String(channelFaders[lane].getValue(),1)+" dB",
                juce::dontSendNotification);
        }
        levelMeters[2].setLevels(engine.meter(2,0),engine.meter(2,1));
        masterLabel.setText(juce::String(masterFader.getValue(),1)+" dB",
                            juce::dontSendNotification);
        notification.setText(startupError.isEmpty()?
            "ASIO / VST3   -   FX rack active. Select input 3/4 for MUSIC.":startupError,
            juce::dontSendNotification);
        refreshSignatureKnobs();
        notification.setColour(juce::Label::textColourId,
            startupError.isEmpty()?visual::muted:visual::amber);
        repaint({0,0,getWidth(),64});
        repaint({0,footerTop,getWidth(),getHeight()-footerTop});
    }
    Engine engine;
    visual::Look theme;
    juce::Image brandLogo,sc220Artwork;
    juce::Rectangle<int> leftPanel,centerPanel,rightPanel;
    int footerTop=760;
    juce::TextButton setup,goLive;
    std::array<juce::Slider,2> channelFaders;
    std::array<juce::TextButton,2> muteButtons;
    std::array<juce::Label,2> faderLabels;
    std::array<visual::Meter,3> levelMeters;
    juce::Slider masterFader;
    juce::Label masterLabel,notification;
    std::array<std::array<FxControls,3>,2> fx;
    const std::array<juce::String,5> macroNames {
        "Enhance","Smart Bass","Smart Treble","Vocal Body","Stereo Magic"
    };
    std::array<juce::Slider,5> signatureKnobs;
    juce::TextButton signatureVoice,signatureMusic;
    int signatureLane=0;
    std::array<std::array<std::unique_ptr<FxEditorWindow>,3>,2> editors;
    std::unique_ptr<juce::FileChooser> fileChooser;
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
        setResizeLimits(1180, 816, 1920, 1200);
        centreWithSize(1376, 816);
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
