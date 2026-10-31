#include "LiveAudioEngine.h"

#include <chrono>
#include <cmath>

namespace askp::live {
namespace {
float dbFromPeak (float peak) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
}

int sc220InputPreference (const juce::String& name) noexcept
{
    const auto normalised = name.trim().toUpperCase();

    // Windows exposes the SC220 used in the field as "Microphone (MIXER-01)".
    // Keep product-specific aliases ahead of the legacy generic MIXER fallback
    // so another mixer endpoint cannot win when the real SC220 is present.
    if (normalised.contains ("MIXER-01"))
        return 100;
    if (normalised.contains ("SC220"))
        return 90;
    if (normalised.contains ("RECORDING TECH"))
        return 80;
    if (normalised == "MIXER")
        return 60;
    if (normalised.startsWith ("MIXER ("))
        return 50;

    return 0;
}

bool looksLikeSc220InputName (const juce::String& name) noexcept
{
    return sc220InputPreference (name) > 0;
}
} // namespace

LiveAudioEngine::LiveAudioEngine()
{
    deviceManager.addChangeListener (this);
    voiceEnhancerBridge.setVisualTapEnabled (false);
    musicEnhancerBridge.setVisualTapEnabled (false);
    programmeAnalyzer.setEnabled (true);

    updateInputTrimRouting();
    updateOutputTrimRouting();
}

LiveAudioEngine::~LiveAudioEngine()
{
    shutdown();
    deviceManager.removeChangeListener (this);
}

bool LiveAudioEngine::initialise (const juce::XmlElement* savedDeviceState)
{
    auto error = deviceManager.initialise (2, 2, savedDeviceState, true);
    if (error.isNotEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError = error;
    }

    // Bundled, SHA-256 pinned official ArSonKuPik VST3 is shipped beside the EXE.
    // Both instances are created on the message thread before the callback
    // starts, without any source or compiler dependency on the ASK-P repository.
    // Avoid automatic plugin discovery: the known bundle path is the authority.
    const auto bundle = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
        .getParentDirectory().getChildFile ("Plugins").getChildFile ("ArSonKuPik.vst3");
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto rate = device->getCurrentSampleRate();
        const auto block = juce::jmax (64, device->getCurrentBufferSizeSamples());
        const auto voiceError = voiceEnhancerBridge.loadBundled (bundle, rate, block);
        const auto musicError = musicEnhancerBridge.loadBundled (bundle, rate, block);
        if (voiceError.isNotEmpty() || musicError.isNotEmpty())
        {
            const juce::ScopedLock lock (statusLock);
            lastError = "Bundled ArSonKuPik VST3 could not initialise. "
                        "SC220 will run safe dry signal until the plugin is repaired. "
                        "Voice: " + voiceError + "; Music: " + musicError;
        }
    }

    if (! callbackRegistered.exchange (true))
        deviceManager.addAudioCallback (this);
    refreshDeviceStatus();

    if (savedDeviceState == nullptr)
    {
        const auto outputs = getOutputDeviceNames();
        for (const auto& name : outputs)
        {
            if (looksLikeVirtualCable (name) && name.containsIgnoreCase ("input"))
            {
                selectOutputDevice (name);
                break;
            }
        }
    }

    // P0: SC220 Live is a dedicated hardware companion. Do not depend on the
    // Windows default recording endpoint or on a stale saved device state.
    // If an SC220 endpoint is present but another input is active, pick the
    // highest-confidence SC220 alias automatically.
    const auto activeInput = deviceManager.getAudioDeviceSetup().inputDeviceName;
    if (! looksLikeSc220InputName (activeInput))
    {
        const auto inputs = getInputDeviceNames();
        juce::String preferredSc220Input;
        int preferredScore = 0;

        for (const auto& name : inputs)
        {
            const int score = sc220InputPreference (name);
            if (score > preferredScore)
            {
                preferredScore = score;
                preferredSc220Input = name;
            }
        }

        if (preferredSc220Input.isNotEmpty())
        {
            const auto selectionError = selectInputDevice (preferredSc220Input);
            if (selectionError.isNotEmpty())
            {
                const juce::ScopedLock lock (statusLock);
                lastError = "SC220 detected as '" + preferredSc220Input
                          + "' but it could not be opened.\n\n" + selectionError;
            }
        }
    }

    return error.isEmpty();
}

bool LiveAudioEngine::quiesceForPluginSwap() noexcept
{
    // This gate is checked before any mutable plugin or DSP member is touched.
    // Unlike removeAudioCallback(), it does NOT release/prepare the entire graph.
    vstSwapSuspended.store (true, std::memory_order_release);
    for (int attempt = 0; attempt < 500; ++attempt)
    {
        if (audioCallbacksInFlight.load (std::memory_order_acquire) == 0)
            return true;
        juce::Thread::sleep (1); // Message thread only, never the audio callback.
    }
    vstSwapSuspended.store (false, std::memory_order_release);
    return false; // Refuse to destroy a processor while its callback may be stuck.
}

void LiveAudioEngine::publishLatencyEstimate() noexcept
{
    const int voiceLatency = noiseClean.getLatencySamples() + voiceVst3.getLatencySamples()
                           + voiceEnhancerBridge.latencySamples();
    const int musicLatency = musicEnhancerBridge.latencySamples();
    voiceLatencySamples.store (voiceLatency, std::memory_order_relaxed);
    musicDspLatencySamples.store (musicLatency, std::memory_order_relaxed);
    if (auto* device = deviceManager.getCurrentAudioDevice())
        totalLatencySamples.store (juce::jmax (voiceLatency, musicLatency)
                                   + device->getInputLatencyInSamples()
                                   + device->getOutputLatencyInSamples(),
                                   std::memory_order_relaxed);
}

juce::String LiveAudioEngine::loadVoiceVst3 (const juce::File& pluginFile)
{
    const double sampleRate = currentSampleRate.load (std::memory_order_acquire);
    const int blockSize = currentBufferSize.load (std::memory_order_acquire);
    InputVst3Rack::PreparedInstance candidate;
    const auto error = InputVst3Rack::createPrepared (pluginFile, sampleRate, blockSize, candidate);
    if (error.isNotEmpty())
        return error;

    if (sampleRate != currentSampleRate.load() || blockSize != currentBufferSize.load())
    {
        candidate->releaseResources();
        return "The audio device changed while preparing the VST3. Try loading again.";
    }

    if (! quiesceForPluginSwap())
    {
        candidate->releaseResources();
        return "The audio callback did not become idle; VST3 was not replaced.";
    }
    voiceVst3.install (std::move (candidate), sampleRate, blockSize);
    publishLatencyEstimate();
    vstSwapSuspended.store (false, std::memory_order_release);
    return {};
}

void LiveAudioEngine::removeVoiceVst3()
{
    if (! quiesceForPluginSwap())
        return; // Never destroy a processor still in use.
    voiceVst3.clear();
    publishLatencyEstimate();
    vstSwapSuspended.store (false, std::memory_order_release);
}

void LiveAudioEngine::shutdown()
{
    pcLoopback.stop();
    if (callbackRegistered.exchange (false))
        deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    voiceEnhancerBridge.release();
    musicEnhancerBridge.release();
    running.store (false);
    deviceOpen.store (false);
}

juce::StringArray LiveAudioEngine::getInputDeviceNames()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (true);
    }
    return {};
}

juce::StringArray LiveAudioEngine::getOutputDeviceNames()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (false);
    }
    return {};
}

juce::Array<double> LiveAudioEngine::getAvailableSampleRates() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableSampleRates();
    return { 44100.0, 48000.0, 96000.0 };
}

juce::Array<int> LiveAudioEngine::getAvailableBufferSizes() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();
    return { 64, 128, 256, 512 };
}

juce::String LiveAudioEngine::selectInputDevice (const juce::String& name)
{
    return applyDeviceSelection (&name, nullptr);
}

juce::String LiveAudioEngine::selectOutputDevice (const juce::String& name)
{
    return applyDeviceSelection (nullptr, &name);
}

juce::String LiveAudioEngine::applyDeviceSelection (const juce::String* inputName,
                                                     const juce::String* outputName)
{
    if (inputName == nullptr && outputName == nullptr)
        return {};

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        type->scanForDevices();

    const auto previous = deviceManager.getAudioDeviceSetup();
    auto requested = previous;
    if (inputName != nullptr)
        requested.inputDeviceName = inputName->trim();
    if (outputName != nullptr)
        requested.outputDeviceName = outputName->trim();

    requested.useDefaultInputChannels = true;
    requested.useDefaultOutputChannels = true;

    auto selectionApplied = [this, inputName, outputName]()
    {
        const auto applied = deviceManager.getAudioDeviceSetup();
        const bool inputOk = inputName == nullptr
            || applied.inputDeviceName.equalsIgnoreCase (inputName->trim());
        const bool outputOk = outputName == nullptr
            || applied.outputDeviceName.equalsIgnoreCase (outputName->trim());
        return inputOk && outputOk && deviceManager.getCurrentAudioDevice() != nullptr;
    };

    auto trySetup = [this, &selectionApplied] (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
    {
        const auto error = deviceManager.setAudioDeviceSetup (setup, true);
        refreshDeviceStatus();
        if (error.isEmpty() && selectionApplied())
            return juce::String();
        if (error.isNotEmpty())
            return error;
        return juce::String ("The audio backend did not keep the requested device selection.");
    };

    auto firstError = trySetup (requested);
    if (firstError.isEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError.clear();
        return {};
    }

    auto automatic = requested;
    automatic.sampleRate = 0.0;
    automatic.bufferSize = 0;
    auto automaticError = trySetup (automatic);
    if (automaticError.isEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError.clear();
        return {};
    }

    deviceManager.closeAudioDevice();
    auto reopenError = trySetup (automatic);
    if (reopenError.isEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError.clear();
        return {};
    }

    const auto rollbackError = deviceManager.setAudioDeviceSetup (previous, true);
    refreshDeviceStatus();

    juce::String message = "Unable to open the selected audio device in a compatible input/output pair.";
    message << "\n\nInitial attempt: " << firstError;
    if (automaticError != firstError)
        message << "\nAutomatic sample-rate/buffer attempt: " << automaticError;
    if (reopenError != automaticError)
        message << "\nClean reopen attempt: " << reopenError;
    if (rollbackError.isNotEmpty())
        message << "\nPrevious device restore: " << rollbackError;
    message << "\n\nTry Refresh Device Lists, then select the output first and the input second.";

    {
        const juce::ScopedLock lock (statusLock);
        lastError = message;
    }
    return message;
}

juce::String LiveAudioEngine::setSampleRate (double value)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.sampleRate = value;
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    refreshDeviceStatus();
    return error;
}

juce::String LiveAudioEngine::setBufferSize (int value)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.bufferSize = value;
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    refreshDeviceStatus();
    return error;
}

void LiveAudioEngine::setInputTrimDb (float value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    inputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value));
    updateInputTrimRouting();
}

void LiveAudioEngine::setSc220FaderDb (float value) noexcept
{
    sc220FaderDb.store (juce::jlimit (-60.0f, 12.0f, value));
}

void LiveAudioEngine::setOutputTrimDb (float value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    outputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value));
    updateOutputTrimRouting();
}

void LiveAudioEngine::setInputTrimBypassed (bool value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    inputTrimBypassed.store (value);
    updateInputTrimRouting();
}

void LiveAudioEngine::setOutputTrimBypassed (bool value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    outputTrimBypassed.store (value);
    updateOutputTrimRouting();
}

void LiveAudioEngine::setPcAudioGainDb (float value) noexcept
{
    pcAudioGainDb.store (juce::jlimit (-60.0f, 12.0f, value));
}

void LiveAudioEngine::setDuckingDepthDb (float value) noexcept
{
    duckingDepthDb.store (juce::jlimit (-24.0f, 0.0f, value));
}

void LiveAudioEngine::setBypassed (bool value) noexcept
{
    setVoiceEnhancerBypassed (value);
    setMusicEnhancerBypassed (value);
}

void LiveAudioEngine::setVoiceEnhancerBypassed (bool value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    voiceEnhancerBypassed.store (value);
    voiceEnhancerBridge.setBypassed (value);
}

void LiveAudioEngine::setMusicEnhancerBypassed (bool value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    musicEnhancerBypassed.store (value);
    musicEnhancerBridge.setBypassed (value);
}

void LiveAudioEngine::restartAudio()
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    deviceManager.closeAudioDevice();
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError = error;
    }
    refreshDeviceStatus();
}

void LiveAudioEngine::applyVstReferenceConditions() noexcept
{
    noiseClean.setEnabled (false);
    expander.setEnabled (false);
    setInputChannelMode (2);
    setInputTrimDb (0.0f);
    setOutputTrimDb (0.0f);
    setInputTrimBypassed (false);
    setOutputTrimBypassed (false);
    setEngineEnabled (true);
    setBypassed (false);
    voiceEnhancerBridge.setGainMatchEnabled (false);
    musicEnhancerBridge.setGainMatchEnabled (false);
    setOutputMuted (false);
}

void LiveAudioEngine::setPresetIndex (int index)                 { voiceEnhancerBridge.setPresetIndex (index); }
int LiveAudioEngine::getPresetIndex() const noexcept             { return voiceEnhancerBridge.presetIndex(); }
juce::StringArray LiveAudioEngine::getPresetNames() const        { return voiceEnhancerBridge.presetNames(); }
void LiveAudioEngine::setEnhance (float value) noexcept          { voiceEnhancerBridge.setEnhance (value); }
void LiveAudioEngine::setSmartBass (float value) noexcept        { voiceEnhancerBridge.setSmartBass (value); }
void LiveAudioEngine::setSmartTreble (float value) noexcept      { voiceEnhancerBridge.setSmartTreble (value); }
void LiveAudioEngine::setVocalBody (float value) noexcept        { voiceEnhancerBridge.setVocalBody (value); }
void LiveAudioEngine::setStereoMagic (float value) noexcept      { voiceEnhancerBridge.setStereoMagic (value); }
float LiveAudioEngine::getEnhance() const noexcept               { return voiceEnhancerBridge.enhance(); }
float LiveAudioEngine::getSmartBass() const noexcept             { return voiceEnhancerBridge.smartBass(); }
float LiveAudioEngine::getSmartTreble() const noexcept           { return voiceEnhancerBridge.smartTreble(); }
float LiveAudioEngine::getVocalBody() const noexcept             { return voiceEnhancerBridge.vocalBody(); }
float LiveAudioEngine::getStereoMagic() const noexcept           { return voiceEnhancerBridge.stereoMagic(); }

void LiveAudioEngine::setMusicPresetIndex (int index)            { musicEnhancerBridge.setPresetIndex (index); }
int LiveAudioEngine::getMusicPresetIndex() const noexcept        { return musicEnhancerBridge.presetIndex(); }
juce::StringArray LiveAudioEngine::getMusicPresetNames() const   { return musicEnhancerBridge.presetNames(); }
void LiveAudioEngine::setMusicEnhance (float value) noexcept     { musicEnhancerBridge.setEnhance (value); }
void LiveAudioEngine::setMusicSmartBass (float value) noexcept   { musicEnhancerBridge.setSmartBass (value); }
void LiveAudioEngine::setMusicSmartTreble (float value) noexcept { musicEnhancerBridge.setSmartTreble (value); }
void LiveAudioEngine::setMusicVocalBody (float value) noexcept   { musicEnhancerBridge.setVocalBody (value); }
void LiveAudioEngine::setMusicStereoMagic (float value) noexcept { musicEnhancerBridge.setStereoMagic (value); }
float LiveAudioEngine::getMusicEnhance() const noexcept          { return musicEnhancerBridge.enhance(); }
float LiveAudioEngine::getMusicSmartBass() const noexcept        { return musicEnhancerBridge.smartBass(); }
float LiveAudioEngine::getMusicSmartTreble() const noexcept      { return musicEnhancerBridge.smartTreble(); }
float LiveAudioEngine::getMusicVocalBody() const noexcept        { return musicEnhancerBridge.vocalBody(); }
float LiveAudioEngine::getMusicStereoMagic() const noexcept      { return musicEnhancerBridge.stereoMagic(); }

LiveAudioEngine::Status LiveAudioEngine::getStatus() const
{
    Status status;
    status.running = running.load();
    status.deviceOpen = deviceOpen.load();
    status.outputIsVirtualCable = outputCable.load();
    status.sampleRate = currentSampleRate.load();
    status.bufferSize = currentBufferSize.load();
    status.cpuPercent = cpuPercent.load();
    status.callbackPeakPercent = callbackPeakPercent.load();
    status.callbackDeadlineMisses = callbackDeadlineMisses.load (std::memory_order_relaxed);
    status.inputPeakDbL = inputPeakL.load();
    status.inputPeakDbR = inputPeakR.load();
    status.pcPeakDbL = pcLoopback.peakDbL();
    status.pcPeakDbR = pcLoopback.peakDbR();
    status.outputPeakDbL = outputPeakL.load();
    status.outputPeakDbR = outputPeakR.load();
    status.gainReductionDb = gainReduction.load();
    status.voiceGainReductionDb = voiceGainReduction.load();
    status.musicGainReductionDb = musicGainReduction.load();
    status.masterGainReductionDb = masterGainReduction.load();
    const auto pleasantSnapshot = pleasantPresence.snapshot();
    status.pleasantMidHighReductionDb = pleasantSnapshot.midHighReductionDb;
    status.deEssReductionDb = pleasantSnapshot.deEssReductionDb;
    status.expanderReductionDb = expander.getGainReductionDb();
    status.duckGainDb = duckGainDb.load();
    status.loudnessLufs = loudnessLufs.load();
    status.truePeakDbtp = truePeakDbtp.load();
    status.loudnessElapsedSeconds = loudnessElapsedSeconds.load();
    status.loudnessReady = loudnessReady.load();
    status.pcLoopbackRunning = pcLoopback.isRunning();
    status.pcUnderflows = pcLoopback.underflows();
    status.pcOverflows = pcLoopback.overflows();
    status.dropouts = dropoutCount.load();
    status.totalLatencySamples = totalLatencySamples.load();
    status.voiceLatencySamples = voiceLatencySamples.load();
    status.musicDspLatencySamples = musicDspLatencySamples.load();

    const juce::ScopedLock lock (statusLock);
    status.inputDevice = currentInputName;
    status.outputDevice = currentOutputName;
    status.lastError = lastError;
    return status;
}

std::unique_ptr<juce::XmlElement> LiveAudioEngine::createDeviceState() const
{
    return deviceManager.createStateXml();
}

void LiveAudioEngine::audioDeviceIOCallbackWithContext (
    const float* const* inputChannelData, int numInputChannels,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    struct CallbackLifetime final
    {
        explicit CallbackLifetime (std::atomic<int>& count) noexcept : inFlight (count)
        { inFlight.fetch_add (1, std::memory_order_acq_rel); }
        ~CallbackLifetime() { inFlight.fetch_sub (1, std::memory_order_release); }
        std::atomic<int>& inFlight;
    } guard (audioCallbacksInFlight);

    const auto start = std::chrono::high_resolution_clock::now();
    clearOutputs (outputChannelData, numOutputChannels, numSamples);
    if (vstSwapSuspended.load (std::memory_order_acquire))
        return;

    if (! running.load() || ! engineEnabled.load() || numOutputChannels <= 0)
        return;

    constexpr int processingChannels = 2;
    if (processBuffer.getNumSamples() < numSamples
        || detectorBuffer.getNumSamples() < numSamples
        || pcAudioBuffer.getNumSamples() < numSamples)
    {
        ++dropoutCount;
        return;
    }

    juce::AudioBuffer<float> voiceBlock (processBuffer.getArrayOfWritePointers(),
                                         processingChannels, numSamples);
    juce::AudioBuffer<float> detectorBlock (detectorBuffer.getArrayOfWritePointers(),
                                            processingChannels, numSamples);
    juce::AudioBuffer<float> musicBlock (pcAudioBuffer.getArrayOfWritePointers(),
                                         processingChannels, numSamples);

    if (loudnessResetRequested.exchange (false, std::memory_order_acq_rel))
    {
        loudnessIntegrator.reset();
        truePeakMaximumLinear = 0.0f;
        truePeakDbtp.store (-120.0f);
        loudnessLufs.store (-120.0f);
        loudnessElapsedSeconds.store (0.0f);
        loudnessReady.store (false);
        truePeakOversampler.reset();
        for (auto& filter : loudnessShelf) filter.reset();
        for (auto& filter : loudnessHighPass) filter.reset();
    }

    // Stereo keeps channels 1/2 as L/R. Dual Mono also preserves channels 1/2;
    // its independent per-channel faders/pan are a UI/transport follow-up. The
    // previous implementation incorrectly duplicated channel 1 and discarded 2.
    for (int channel = 0; channel < processingChannels; ++channel)
    {
        const int sourceChannel = juce::jmin (channel, juce::jmax (0, numInputChannels - 1));
        if (numInputChannels > 0 && inputChannelData[sourceChannel] != nullptr)
            voiceBlock.copyFrom (channel, 0, inputChannelData[sourceChannel], numSamples);
        else
            voiceBlock.clear (channel, 0, numSamples);
    }

    // SC220 trim is now entirely local to the voice lane. It can no longer
    // change the PC Music level through the shared ASK-P input parameter.
    const float inputTargetDb = inputTrimBypassed.load() ? 0.0f : inputTrimDb.load();
    inputGainLinear.setTargetValue (juce::Decibels::decibelsToGain (inputTargetDb));
    const float inputStart = inputGainLinear.getCurrentValue();
    const float inputEnd = inputGainLinear.skip (numSamples);
    voiceBlock.applyGainRamp (0, numSamples, inputStart, inputEnd);

    noiseClean.process (voiceBlock);
    for (int channel = 0; channel < processingChannels; ++channel)
        detectorBlock.copyFrom (channel, 0, voiceBlock, channel, 0, numSamples);

    float sc220PeakL = 0.0f;
    float sc220PeakR = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        sc220PeakL = juce::jmax (sc220PeakL, std::abs (detectorBlock.getSample (0, sample)));
        sc220PeakR = juce::jmax (sc220PeakR, std::abs (detectorBlock.getSample (1, sample)));
    }
    const float meterReleaseDb = static_cast<float> (26.0 * static_cast<double> (numSamples)
                                                     / juce::jmax (1.0, currentSampleRate.load()));
    inputPeakL.store (juce::jmax (dbFromPeak (sc220PeakL), inputPeakL.load() - meterReleaseDb));
    inputPeakR.store (juce::jmax (dbFromPeak (sc220PeakR), inputPeakR.load() - meterReleaseDb));

    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (voiceBlock, detectorBlock);

    voiceMidi.clear();
    voiceVst3.process (voiceBlock, voiceMidi); // Voice FX only; PC Music bypasses this insert.
    voiceEnhancerBridge.process (voiceBlock, voiceMidi);
    if (! voiceEnhancerBypassed.load (std::memory_order_relaxed))
    {
        // Coordinate protection with the amount of generated brilliance. It
        // remains restrained at low macro values and gains authority only as
        // Enhance/Smart Treble make the voice more forward.
        const float brilliance = juce::jmax (voiceEnhancerBridge.enhance(),
                                             voiceEnhancerBridge.smartTreble());
        pleasantPresence.setStrength (juce::jlimit (0.58f, 1.0f,
            0.52f + 0.0052f * brilliance));
        pleasantPresence.process (voiceBlock);
    }
    else
    {
        // Do not leave stale detector state or reduction telemetry visible
        // while the complete vocal enhancement lane is bypassed.
        pleasantPresence.reset();
    }

    sc220MixGainLinear.setTargetValue (juce::Decibels::decibelsToGain (sc220FaderDb.load()));
    const float sc220MuteGain = sc220Muted.load() ? 0.0f : 1.0f;
    const float sc220GainStart = sc220MixGainLinear.getCurrentValue() * sc220MuteGain;
    const float sc220GainEnd = sc220MixGainLinear.skip (numSamples) * sc220MuteGain;
    voiceBlock.applyGainRamp (0, numSamples, sc220GainStart, sc220GainEnd);

    pcLoopback.read (musicBlock, numSamples);
    if (! pcAudioEnabled.load() || pcAudioMuted.load())
    {
        musicBlock.clear();
        duckGainLinear = 1.0f;
        duckGainDb.store (0.0f);
    }
    else
    {
        // Program audio receives its own ASK-P Signature pass before ducking,
        // so its bass/detail/stereo character remains stable while voice talks.
        musicMidi.clear();
        musicEnhancerBridge.process (musicBlock, musicMidi);

        const float keyPeak = juce::jmax (sc220PeakL, sc220PeakR);
        const bool voicePresent = dbFromPeak (keyPeak) > -36.0f;
        const float targetDuck = duckingEnabled.load() && voicePresent
                               ? juce::Decibels::decibelsToGain (duckingDepthDb.load())
                               : 1.0f;
        const double seconds = targetDuck < duckGainLinear ? 0.020 : 0.400;
        const float coefficient = static_cast<float> (std::exp (
            -static_cast<double> (numSamples)
            / (juce::jmax (1.0, currentSampleRate.load()) * seconds)));
        const float nextDuck = targetDuck + coefficient * (duckGainLinear - targetDuck);

        pcGainLinear.setTargetValue (juce::Decibels::decibelsToGain (pcAudioGainDb.load()));
        const float pcGainStart = pcGainLinear.getCurrentValue() * duckGainLinear;
        const float pcGainEnd = pcGainLinear.skip (numSamples) * nextDuck;
        musicBlock.applyGainRamp (0, numSamples, pcGainStart, pcGainEnd);
        duckGainLinear = nextDuck;
        duckGainDb.store (juce::Decibels::gainToDecibels (juce::jmax (nextDuck, 1.0e-6f), -120.0f));
    }

    for (int channel = 0; channel < processingChannels; ++channel)
        voiceBlock.addFrom (channel, 0, musicBlock, channel, 0, numSamples);

    // Output trim is a master-bus control. The protected mix is then caught by
    // a final -1 dB limiter rather than sending two independently safe lanes to
    // VB-CABLE without sum protection.
    const float outputTargetDb = outputTrimBypassed.load() ? 0.0f : outputTrimDb.load();
    outputGainLinear.setTargetValue (juce::Decibels::decibelsToGain (outputTargetDb));
    const float outputStart = outputGainLinear.getCurrentValue();
    const float outputEnd = outputGainLinear.skip (numSamples);
    voiceBlock.applyGainRamp (0, numSamples, outputStart, outputEnd);

    const float preLimiterPeakDb = peakDb (voiceBlock);
    auto masterBlock = juce::dsp::AudioBlock<float> (voiceBlock);
    juce::dsp::ProcessContextReplacing<float> masterContext (masterBlock);
    masterLimiter.process (masterContext);

    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);

    const auto& truePeakBuffer = static_cast<const juce::AudioBuffer<float>&> (voiceBlock);
    const auto truePeakInput = juce::dsp::AudioBlock<const float> (truePeakBuffer);
    const auto oversampled = truePeakOversampler.processSamplesUp (truePeakInput);
    float blockTruePeak = 0.0f;
    for (int channel = 0; channel < static_cast<int> (oversampled.getNumChannels()); ++channel)
        for (int sample = 0; sample < static_cast<int> (oversampled.getNumSamples()); ++sample)
            blockTruePeak = juce::jmax (blockTruePeak,
                std::abs (oversampled.getSample (channel, sample)));
    truePeakMaximumLinear = juce::jmax (truePeakMaximumLinear, blockTruePeak);
    truePeakDbtp.store (dbFromPeak (truePeakMaximumLinear));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        double weightedFrameEnergy = 0.0;
        for (int channel = 0; channel < processingChannels; ++channel)
        {
            float value = voiceBlock.getSample (channel, sample);
            value = loudnessShelf[static_cast<size_t> (channel)].processSample (value);
            value = loudnessHighPass[static_cast<size_t> (channel)].processSample (value);
            weightedFrameEnergy += static_cast<double> (value) * static_cast<double> (value);
        }
        loudnessIntegrator.processFrame (weightedFrameEnergy);
    }
    loudnessLufs.store (loudnessIntegrator.integratedLufs());
    loudnessElapsedSeconds.store (static_cast<float> (loudnessIntegrator.elapsedSeconds()));
    loudnessReady.store (loudnessIntegrator.ready());

    updateMeters (voiceBlock, preLimiterPeakDb);

    if (! outputMuted.load())
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            if (outputChannelData[channel] == nullptr)
                continue;
            const int sourceChannel = juce::jmin (channel, processingChannels - 1);
            juce::FloatVectorOperations::copy (outputChannelData[channel],
                                               voiceBlock.getReadPointer (sourceChannel),
                                               numSamples);
        }
    }

    const auto elapsed = std::chrono::duration<double> (
        std::chrono::high_resolution_clock::now() - start).count();
    const auto budget = static_cast<double> (numSamples)
                      / juce::jmax (1.0, currentSampleRate.load());
    const float blockCpu = budget > 0.0 ? static_cast<float> (elapsed / budget * 100.0) : 0.0f;
    if (blockCpu >= 100.0f)
        callbackDeadlineMisses.fetch_add (1, std::memory_order_relaxed);
    cpuPercent.store (cpuPercent.load() * 0.92f + blockCpu * 0.08f);
    const float previousPeak = callbackPeakPercent.load (std::memory_order_relaxed);
    callbackPeakPercent.store (blockCpu > previousPeak ? blockCpu : previousPeak * 0.9975f,
                               std::memory_order_relaxed);
}

void LiveAudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = juce::jmax (64, device->getCurrentBufferSizeSamples());
    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    constexpr int channels = 2;

    processBuffer.setSize (channels, blockSize, false, true, false);
    detectorBuffer.setSize (channels, blockSize, false, true, false);
    pcAudioBuffer.setSize (channels, blockSize, false, true, false);
    processBuffer.clear();
    detectorBuffer.clear();
    pcAudioBuffer.clear();

    // Clear the per-engine P6B filter state on every real device start.
    p6bLow1L = p6bLow2L = p6bLow1R = p6bLow2R = 0.0f;
    p6bHighEnvelope = p6bBodyEnvelope = 0.0f;
    p6bHissGain = 1.0f;
    noiseClean.prepare (sampleRate, blockSize, channels);
    expander.prepare (sampleRate);

    voiceEnhancerBridge.prepare (sampleRate, blockSize);
    musicEnhancerBridge.prepare (sampleRate, blockSize);
    voiceVst3.prepare (sampleRate, blockSize);
    pleasantPresence.prepare (sampleRate, blockSize, channels);
    updateInputTrimRouting();
    updateOutputTrimRouting();
    voiceEnhancerBridge.setBypassed (voiceEnhancerBypassed.load());
    musicEnhancerBridge.setBypassed (musicEnhancerBypassed.load());
    voiceEnhancerBridge.setGainMatchEnabled (false);
    musicEnhancerBridge.setGainMatchEnabled (false);

    juce::dsp::ProcessSpec masterSpec;
    masterSpec.sampleRate = sampleRate;
    masterSpec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
    masterSpec.numChannels = channels;
    masterLimiter.prepare (masterSpec);
    masterLimiter.setThreshold (-1.0f);
    masterLimiter.setRelease (80.0f);
    masterLimiter.reset();

    inputGainLinear.reset (sampleRate, 0.020);
    sc220MixGainLinear.reset (sampleRate, 0.020);
    outputGainLinear.reset (sampleRate, 0.020);
    pcGainLinear.reset (sampleRate, 0.020);
    inputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        inputTrimBypassed.load() ? 0.0f : inputTrimDb.load()));
    sc220MixGainLinear.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (sc220FaderDb.load()));
    outputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        outputTrimBypassed.load() ? 0.0f : outputTrimDb.load()));
    pcGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pcAudioGainDb.load()));
    duckGainLinear = 1.0f;
    duckGainDb.store (0.0f);
    loudnessIntegrator.prepare (sampleRate);
    loudnessLufs.store (-120.0f);
    loudnessElapsedSeconds.store (0.0f);
    loudnessReady.store (false);
    truePeakMaximumLinear = 0.0f;
    truePeakDbtp.store (-120.0f);
    truePeakOversampler.initProcessing (static_cast<std::size_t> (blockSize));
    truePeakOversampler.reset();

    const auto shelfCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        static_cast<float> (sampleRate), 1681.9745f, 0.70717525f,
        juce::Decibels::decibelsToGain (4.0f));
    const auto highPassCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        static_cast<float> (sampleRate), 38.13547f, 0.50032705f);
    for (int channel = 0; channel < channels; ++channel)
    {
        loudnessShelf[static_cast<size_t> (channel)].coefficients = shelfCoefficients;
        loudnessHighPass[static_cast<size_t> (channel)].coefficients = highPassCoefficients;
        loudnessShelf[static_cast<size_t> (channel)].reset();
        loudnessHighPass[static_cast<size_t> (channel)].reset();
    }

    pcLoopback.prepare (sampleRate);
    pcLoopback.start();

    publishLatencyEstimate();
    running.store (true);
    deviceOpen.store (true);
    dropoutCount.store (0);
    callbackPeakPercent.store (0.0f);
    callbackDeadlineMisses.store (0, std::memory_order_relaxed);
    refreshDeviceStatus();
}

void LiveAudioEngine::audioDeviceStopped()
{
    pcLoopback.stop();
    voiceEnhancerBridge.release();
    musicEnhancerBridge.release();
    voiceVst3.release();
    pleasantPresence.reset();
    masterLimiter.reset();
    running.store (false);
    deviceOpen.store (false);
}

void LiveAudioEngine::audioDeviceError (const juce::String& errorMessage)
{
    const juce::ScopedLock lock (statusLock);
    lastError = errorMessage;
    ++dropoutCount;
}

void LiveAudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshDeviceStatus();
}

void LiveAudioEngine::clearOutputs (float* const* outputChannelData,
                                    int channels, int samples) noexcept
{
    for (int channel = 0; channel < channels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], samples);
}

float LiveAudioEngine::peakDb (const juce::AudioBuffer<float>& buffer) noexcept
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = juce::jmax (peak, std::abs (data[sample]));
    }
    return dbFromPeak (peak);
}

void LiveAudioEngine::updateMeters (const juce::AudioBuffer<float>& finalOutput,
                                    float preLimiterPeakDb) noexcept
{
    const auto voiceMeters = voiceEnhancerBridge.meters();
    const auto musicMeters = musicEnhancerBridge.meters();

    const int samples = finalOutput.getNumSamples();
    const int right = juce::jmin (1, finalOutput.getNumChannels() - 1);
    const auto* leftData = finalOutput.getReadPointer (0);
    const auto* rightData = finalOutput.getReadPointer (right);
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int sample = 0; sample < samples; ++sample)
    {
        leftPeak = juce::jmax (leftPeak, std::abs (leftData[sample]));
        rightPeak = juce::jmax (rightPeak, std::abs (rightData[sample]));
    }

    const float releaseDb = static_cast<float> (26.0 * static_cast<double> (samples)
                                                / juce::jmax (1.0, currentSampleRate.load()));
    outputPeakL.store (juce::jmax (dbFromPeak (leftPeak), outputPeakL.load() - releaseDb));
    outputPeakR.store (juce::jmax (dbFromPeak (rightPeak), outputPeakR.load() - releaseDb));

    const float postLimiterPeakDb = dbFromPeak (juce::jmax (leftPeak, rightPeak));
    const float masterReduction = juce::jmax (0.0f, preLimiterPeakDb - postLimiterPeakDb);
    voiceGainReduction.store (voiceMeters.gainReductionDb);
    musicGainReduction.store (musicMeters.gainReductionDb);
    masterGainReduction.store (masterReduction);
    gainReduction.store (juce::jmax (masterReduction,
                          juce::jmax (voiceMeters.gainReductionDb, musicMeters.gainReductionDb)));
}

void LiveAudioEngine::refreshDeviceStatus()
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    {
        const juce::ScopedLock lock (statusLock);
        currentInputName = setup.inputDeviceName;
        currentOutputName = setup.outputDeviceName;
    }
    outputCable.store (looksLikeVirtualCable (setup.outputDeviceName));
    deviceOpen.store (deviceManager.getCurrentAudioDevice() != nullptr);
}

void LiveAudioEngine::updateInputTrimRouting() noexcept
{
    // SC220 trim is implemented in the dedicated voice lane. Keep the embedded
    // processor's input faucet at unity so it cannot alter PC Music.
    voiceEnhancerBridge.setInputGainDb (0.0f);
}

void LiveAudioEngine::updateOutputTrimRouting() noexcept
{
    // Each source processor stays at its profile output level. Final trim is a
    // master-bus gain directly before the final -1 dB safety limiter.
    voiceEnhancerBridge.setFinalOutputGainDb (0.0f);
    musicEnhancerBridge.setFinalOutputGainDb (0.0f);
}

bool LiveAudioEngine::looksLikeVirtualCable (const juce::String& name)
{
    // Identify explicit virtual routing outputs without assuming that every
    // JACK, CoreAudio or PipeWire endpoint is a safe stream destination.
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input")
        || name.containsIgnoreCase ("BlackHole")
        || name.containsIgnoreCase ("Soundflower")
        || name.containsIgnoreCase ("Loopback Audio");
}

} // namespace askp::live
