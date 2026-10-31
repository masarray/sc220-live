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
            const int read = (writeIndex + len - latency) % len;
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
        voice.clear();
        music.clear();
    }
    juce::AudioDeviceManager& devices() noexcept { return manager; }
    juce::String pluginName(int lane) const
    {
        const juce::ScopedLock lock(controlLock);
        return lane == 0 ? voice.displayName : music.displayName;
    }
    bool hasPlugin(int lane) const
    {
        const juce::ScopedLock lock(controlLock);
        return lane == 0 ? voice.instance != nullptr : music.instance != nullptr;
    }
    void setBypass(int lane, bool enabled) noexcept
    {
        (lane == 0 ? voice : music).bypass.store(enabled, std::memory_order_relaxed);
    }
    bool isBypassed(int lane) const noexcept
    {
        return (lane == 0 ? voice : music).bypass.load(std::memory_order_relaxed);
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
    juce::String loadPlugin(int lane, const juce::File& file)
    {
        if (!file.exists() || !file.hasFileExtension(".vst3"))
            return "Please choose an installed .vst3 bundle.";
        const double rate = sampleRate.load();
        const int block = blockSize.load();
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
        const auto name = next->getName();
        // JUCE's removeAudioCallback waits for in-flight callbacks. No VST3
        // scan or destruction takes place in the realtime callback.
        manager.removeAudioCallback(this);
        {
            const juce::ScopedLock lock(controlLock);
            auto& slot = lane == 0 ? voice : music;
            slot.clear();
            slot.instance = std::move(next);
            slot.displayName = name;
            slot.bypass.store(false);
        }
        manager.addAudioCallback(this);
        return {};
    }
    void removePlugin(int lane)
    {
        manager.removeAudioCallback(this);
        {
            const juce::ScopedLock lock(controlLock);
            (lane == 0 ? voice : music).clear();
        }
        manager.addAudioCallback(this);
    }
    juce::AudioPluginInstance* processor(int lane)
    {
        const juce::ScopedLock lock(controlLock);
        return (lane == 0 ? voice : music).instance.get();
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        const auto rate = device->getCurrentSampleRate();
        const auto block = juce::jmax(64, device->getCurrentBufferSizeSamples());
        sampleRate.store(rate);
        blockSize.store(block);
        voiceBuffer.setSize(2, block, false, true, false);
        musicBuffer.setSize(2, block, false, true, false);
        voice.prepare(rate, block);
        music.prepare(rate, block);
        misses.store(0);
    }
    void audioDeviceStopped() override
    {
        voice.release();
        music.release();
        sampleRate.store(0.0);
    }
    void audioDeviceIOCallbackWithContext(const float* const* in, int inChannels,
        float* const* out, int outChannels, int n,
        const juce::AudioIODeviceCallbackContext&) override
    {
        const auto begin = juce::Time::getHighResolutionTicks();
        for (int ch = 0; ch < outChannels; ++ch)
            if (out[ch]) juce::FloatVectorOperations::clear(out[ch], n);
        if (n > voiceBuffer.getNumSamples() || n <= 0) return;
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
        voice.process(voiceBuffer, midi);
        music.process(musicBuffer, midi);
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
    StereoPlugin voice, music;
    juce::AudioBuffer<float> voiceBuffer, musicBuffer;
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<float> voiceGain { 1.0f }, musicGain { 1.0f };
    std::atomic<float> voicePeak { 0.0f }, musicPeak { 0.0f };
    std::atomic<bool> streamMuted { true };
    std::atomic<int> misses { 0 };
};

class Console final : public juce::Component, private juce::Timer
{
public:
    Console()
    {
        setSize(920, 590);
        addAndMakeVisible(header);
        header.setText("SC220  /  LIVE CONSOLE", juce::dontSendNotification);
        header.setFont(juce::Font(juce::FontOptions(20.0f)));
        header.setColour(juce::Label::textColourId, juce::Colour(0xffdee7f6));
        addAndMakeVisible(subhead);
        subhead.setText("INDEPENDENT VST3 HOST     ·     ASIO / WASAPI", juce::dontSendNotification);
        subhead.setColour(juce::Label::textColourId, juce::Colour(0xff8293aa));
        addAndMakeVisible(setup);
        setup.setButtonText("AUDIO SETUP");
        setup.onClick = [this]
        {
            auto* selector = new juce::AudioDeviceSelectorComponent(
                engine.devices(), 1, 4, 1, 2, false, false, true, false);
            selector->setSize(650, 440);
            juce::DialogWindow::LaunchOptions opt;
            opt.content.setOwned(selector);
            opt.dialogTitle = "SC220 Audio Device / ASIO";
            opt.dialogBackgroundColour = juce::Colour(0xff202833);
            opt.launchAsync();
        };
        initialiseLane(0, "VOICE  /  INPUT 1–2");
        initialiseLane(1, "MUSIC  /  INPUT 3–4");
        addAndMakeVisible(stream);
        stream.setButtonText("GO LIVE");
        stream.setClickingTogglesState(true);
        stream.onClick = [this]
        {
            engine.setStreamMuted(!stream.getToggleState());
            stream.setButtonText(stream.getToggleState() ? "ON AIR" : "GO LIVE");
        };
        addAndMakeVisible(health);
        health.setColour(juce::Label::textColourId, juce::Colour(0xff9cadbf));
        startTimerHz(12);
    }
    ~Console() override
    {
        stopTimer();
        // All editors are owned by this GUI and closed before the engine.
        editors = {};
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff141920));
        g.setColour(juce::Colour(0xff263444));
        g.drawLine(24.0f, 97.0f, static_cast<float>(getWidth() - 24), 97.0f);
        g.setColour(juce::Colour(0xff1b2531));
        g.fillRoundedRectangle(24.0f, 120.0f, static_cast<float>(getWidth() - 48), 335.0f, 12.0f);
        g.setColour(juce::Colour(0xff2b3849));
        g.drawRoundedRectangle(24.0f, 120.0f, static_cast<float>(getWidth() - 48), 335.0f, 12.0f, 1.0f);
        for (int i = 0; i < 2; ++i)
        {
            auto r = juce::Rectangle<float>(40.0f + i * 440.0f, 138.0f, 410.0f, 295.0f);
            g.setColour(juce::Colour(0xff202d3b));
            g.fillRoundedRectangle(r, 9.0f);
        }
    }
    void resized() override
    {
        header.setBounds(28, 18, 520, 32);
        subhead.setBounds(30, 56, 520, 22);
        setup.setBounds(getWidth() - 175, 37, 145, 34);
        for (int i = 0; i < 2; ++i)
        {
            const int x = 56 + i * 440;
            title[i].setBounds(x, 152, 265, 30);
            load[i].setBounds(x, 200, 155, 34);
            bypass[i].setBounds(x + 165, 200, 105, 34);
            remove[i].setBounds(x + 280, 200, 98, 34);
            plugin[i].setBounds(x, 247, 378, 40);
            gain[i].setBounds(x, 314, 378, 38);
            meter[i].setBounds(x, 380, 376, 22);
        }
        stream.setBounds(getWidth() - 210, 492, 180, 52);
        health.setBounds(30, 490, getWidth() - 260, 56);
    }
private:
    void initialiseLane(int index, juce::String name)
    {
        title[index].setText(name, juce::dontSendNotification);
        title[index].setColour(juce::Label::textColourId, juce::Colour(0xffa2d6f6));
        addAndMakeVisible(title[index]);
        addAndMakeVisible(load[index]);
        load[index].setButtonText("LOAD VST3");
        load[index].onClick = [this, index]
        {
            chooser = std::make_unique<juce::FileChooser>(
                "Choose a trusted stereo VST3", juce::File(), "*.vst3");
            chooser->launchAsync(juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::canSelectDirectories,
                [safe = juce::Component::SafePointer<Console>(this), index](const juce::FileChooser& fc)
                {
                    if (safe == nullptr) return;
                    auto file = fc.getResult();
                    if (file == juce::File()) return;
                    const auto failure = safe->engine.loadPlugin(index, file);
                    if (failure.isNotEmpty())
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon, "VST3 unavailable", failure);
                    safe->updatePluginNames();
                });
        };
        addAndMakeVisible(bypass[index]);
        bypass[index].setButtonText("BYPASS");
        bypass[index].setClickingTogglesState(true);
        bypass[index].onClick = [this, index]
        {
            engine.setBypass(index, bypass[index].getToggleState());
        };
        addAndMakeVisible(remove[index]);
        remove[index].setButtonText("REMOVE");
        remove[index].onClick = [this, index]
        {
            // The host destroys a plugin only after its editor.
            editors[index].reset();
            engine.removePlugin(index);
            updatePluginNames();
        };
        addAndMakeVisible(plugin[index]);
        plugin[index].setText("No enhancer loaded  ·  Click LOAD VST3",
            juce::dontSendNotification);
        plugin[index].setColour(juce::Label::textColourId, juce::Colour(0xffd7e5f5));
        plugin[index].setInterceptsMouseClicks(true, false);
        plugin[index].onTextChange = [] {};
        addAndMakeVisible(gain[index]);
        gain[index].setRange(-60.0, 12.0, 0.1);
        gain[index].setValue(0.0);
        gain[index].setTextValueSuffix(" dB");
        gain[index].setSliderStyle(juce::Slider::LinearHorizontal);
        gain[index].onValueChange = [this, index]
        {
            engine.setGain(index, juce::Decibels::decibelsToGain(
                static_cast<float>(gain[index].getValue())));
        };
        addAndMakeVisible(meter[index]);
        meter[index].setRange(0.0, 1.0);
        meter[index].setValue(0.0);
        meter[index].setSliderStyle(juce::Slider::LinearBar);
        meter[index].setEnabled(false);
    }
    void updatePluginNames()
    {
        for (int i = 0; i < 2; ++i)
        {
            const auto name = engine.pluginName(i);
            plugin[i].setText(name.isEmpty() ? "No enhancer loaded" : name,
                juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        for (int i = 0; i < 2; ++i)
            meter[i].setValue(engine.meter(i), juce::dontSendNotification);
        health.setText("ENGINE STATUS  ·  Callback deadline misses: "
            + juce::String(engine.deadlineMisses())
            + "\nOUTPUT: " + juce::String(engine.isStreamMuted() ? "MUTED (safe)" : "LIVE"),
            juce::dontSendNotification);
    }
    Engine engine;
    juce::Label header, subhead, health;
    juce::TextButton setup, stream;
    std::array<juce::Label, 2> title, plugin;
    std::array<juce::TextButton, 2> load, bypass, remove;
    std::array<juce::Slider, 2> gain, meter;
    std::array<std::unique_ptr<juce::DocumentWindow>, 2> editors;
    std::unique_ptr<juce::FileChooser> chooser;
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
        centreWithSize(920, 590);
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
