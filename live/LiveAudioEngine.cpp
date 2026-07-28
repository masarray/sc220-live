#include "LiveAudioEngine.h"

#include <chrono>
#include <cmath>

namespace askp::live {
namespace {
float dbFromPeak (float peak) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
}
} // namespace

LiveAudioEngine::LiveAudioEngine()
{
    deviceManager.addChangeListener (this);
    enhancerBridge.setVisualTapEnabled (false);

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

    return error.isEmpty();
}

void LiveAudioEngine::shutdown()
{
    if (callbackRegistered.exchange (false))
        deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    enhancerBridge.release();
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

    // Let each newly selected endpoint expose its normal channel mask. Keeping
    // a stale BigInteger from the previous device is a common reason a valid
    // Windows/WASAPI selection needs several attempts before it opens.
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

    // First preserve the user's current latency settings.
    auto firstError = trySetup (requested);
    if (firstError.isEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError.clear();
        return {};
    }

    // A new input/output pair may not support the previous pair's exact sample
    // rate or buffer size. Zero asks JUCE/WASAPI to negotiate compatible values
    // in one transaction instead of requiring repeated manual selections.
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

    // Some Windows drivers retain a busy endpoint until the old device is
    // explicitly closed. Reopen once with automatic timing, then roll back if
    // the requested pair still cannot be established.
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

void LiveAudioEngine::setBypassed (bool value) noexcept
{
    if (! fullFeaturesAllowed()) return;
    enhancerBypassed.store (value);
    enhancerBridge.setBypassed (value);
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
    // Manual A/B reference: remove every Live-only tonal/dynamics stage while
    // leaving the authoritative askp::Processor active at unity I/O gain.
    // MusicalNoiseClean still maintains its fixed latency line when disabled,
    // so comparisons must align by the latency value reported in the footer.
    noiseClean.setEnabled (false);
    expander.setEnabled (false);
    setInputChannelMode (2);
    setInputTrimDb (0.0f);
    setOutputTrimDb (0.0f);
    setInputTrimBypassed (false);
    setOutputTrimBypassed (false);
    setEngineEnabled (true);
    setBypassed (false);
    enhancerBridge.setGainMatchEnabled (false);
    setOutputMuted (false);
}

void LiveAudioEngine::setPresetIndex (int index)
{
    enhancerBridge.setPresetIndex (index);
}

int LiveAudioEngine::getPresetIndex() const noexcept
{
    return enhancerBridge.presetIndex();
}

juce::StringArray LiveAudioEngine::getPresetNames() const
{
    return enhancerBridge.presetNames();
}

void LiveAudioEngine::setEnhance (float value) noexcept     { enhancerBridge.setEnhance (value); }
void LiveAudioEngine::setSmartBass (float value) noexcept   { enhancerBridge.setSmartBass (value); }
void LiveAudioEngine::setSmartTreble (float value) noexcept { enhancerBridge.setSmartTreble (value); }
void LiveAudioEngine::setVocalBody (float value) noexcept   { enhancerBridge.setVocalBody (value); }
void LiveAudioEngine::setStereoMagic (float value) noexcept { enhancerBridge.setStereoMagic (value); }

float LiveAudioEngine::getEnhance() const noexcept      { return enhancerBridge.enhance(); }
float LiveAudioEngine::getSmartBass() const noexcept    { return enhancerBridge.smartBass(); }
float LiveAudioEngine::getSmartTreble() const noexcept  { return enhancerBridge.smartTreble(); }
float LiveAudioEngine::getVocalBody() const noexcept    { return enhancerBridge.vocalBody(); }
float LiveAudioEngine::getStereoMagic() const noexcept  { return enhancerBridge.stereoMagic(); }

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
    status.inputPeakDbL = inputPeakL.load();
    status.inputPeakDbR = inputPeakR.load();
    status.outputPeakDbL = outputPeakL.load();
    status.outputPeakDbR = outputPeakR.load();
    status.gainReductionDb = gainReduction.load();
    status.expanderReductionDb = expander.getGainReductionDb();
    status.dropouts = dropoutCount.load();
    status.totalLatencySamples = totalLatencySamples.load();

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
    const auto start = std::chrono::high_resolution_clock::now();
    clearOutputs (outputChannelData, numOutputChannels, numSamples);

    if (! running.load() || ! engineEnabled.load() || numOutputChannels <= 0)
        return;

    const int processingChannels = juce::jlimit (1, 2, juce::jmax (numOutputChannels, numInputChannels));
    if (processBuffer.getNumSamples() < numSamples || processBuffer.getNumChannels() < processingChannels)
    {
        ++dropoutCount;
        return;
    }

    const bool dualMonoInput = inputChannelMode.load() == 1;
    for (int channel = 0; channel < processingChannels; ++channel)
    {
        const int sourceChannel = dualMonoInput ? 0
                                                : juce::jmin (channel, juce::jmax (0, numInputChannels - 1));
        if (numInputChannels > 0 && inputChannelData[sourceChannel] != nullptr)
            processBuffer.copyFrom (channel, 0, inputChannelData[sourceChannel], numSamples);
        else
            processBuffer.clear (channel, 0, numSamples);
    }

    const float inputTargetDb = inputTrimBypassed.load() ? 0.0f
                                                        : juce::jmin (0.0f, inputTrimDb.load() + 12.0f);
    inputGainLinear.setTargetValue (juce::Decibels::decibelsToGain (inputTargetDb));
    const float inputStart = inputGainLinear.getCurrentValue();
    const float inputEnd = inputGainLinear.skip (numSamples);
    if (std::abs (inputStart - 1.0f) > 0.00001f || std::abs (inputEnd - 1.0f) > 0.00001f)
        processBuffer.applyGainRamp (0, numSamples, inputStart, inputEnd);

    noiseClean.process (processBuffer);

    for (int channel = 0; channel < processingChannels; ++channel)
        detectorBuffer.copyFrom (channel, 0, processBuffer, channel, 0, numSamples);

    emptyMidi.clear();
    enhancerBridge.process (processBuffer, emptyMidi);

    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (processBuffer, detectorBuffer);

    const float outputTargetDb = outputTrimBypassed.load() ? 0.0f
                                                          : juce::jmin (0.0f, outputTrimDb.load() + 12.0f);
    outputGainLinear.setTargetValue (juce::Decibels::decibelsToGain (outputTargetDb));
    const float outputStart = outputGainLinear.getCurrentValue();
    const float outputEnd = outputGainLinear.skip (numSamples);
    if (std::abs (outputStart - 1.0f) > 0.00001f || std::abs (outputEnd - 1.0f) > 0.00001f)
        processBuffer.applyGainRamp (0, numSamples, outputStart, outputEnd);

    updateMeters (processBuffer);

    if (! outputMuted.load())
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            if (outputChannelData[channel] == nullptr)
                continue;
            const int sourceChannel = juce::jmin (channel, processingChannels - 1);
            juce::FloatVectorOperations::copy (outputChannelData[channel],
                                               processBuffer.getReadPointer (sourceChannel),
                                               numSamples);
        }
    }

    const auto elapsed = std::chrono::duration<double> (
        std::chrono::high_resolution_clock::now() - start).count();
    const auto budget = static_cast<double> (numSamples)
                      / juce::jmax (1.0, currentSampleRate.load());
    const float blockCpu = budget > 0.0 ? static_cast<float> (elapsed / budget * 100.0) : 0.0f;
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
    constexpr int channels = 2;

    processBuffer.setSize (channels, blockSize, false, true, false);
    detectorBuffer.setSize (channels, blockSize, false, true, false);
    processBuffer.clear();
    detectorBuffer.clear();

    noiseClean.prepare (sampleRate, blockSize, channels);
    expander.prepare (sampleRate);

    enhancerBridge.prepare (sampleRate, blockSize);
    updateInputTrimRouting();
    updateOutputTrimRouting();
    enhancerBridge.setBypassed (enhancerBypassed.load());
    enhancerBridge.setGainMatchEnabled (false);

    inputGainLinear.reset (sampleRate, 0.020);
    outputGainLinear.reset (sampleRate, 0.020);
    inputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        inputTrimBypassed.load() ? 0.0f : juce::jmin (0.0f, inputTrimDb.load() + 12.0f)));
    outputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        outputTrimBypassed.load() ? 0.0f : juce::jmin (0.0f, outputTrimDb.load() + 12.0f)));

    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    totalLatencySamples.store (noiseClean.getLatencySamples()
                               + enhancerBridge.latencySamples()
                               + device->getInputLatencyInSamples()
                               + device->getOutputLatencyInSamples());
    running.store (true);
    deviceOpen.store (true);
    dropoutCount.store (0);
    callbackPeakPercent.store (0.0f);
    refreshDeviceStatus();
}

void LiveAudioEngine::audioDeviceStopped()
{
    enhancerBridge.release();
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

void LiveAudioEngine::updateMeters (const juce::AudioBuffer<float>& finalOutput) noexcept
{
    const auto engineMeters = enhancerBridge.meters();
    inputPeakL.store (engineMeters.inputPeakDbL);
    inputPeakR.store (engineMeters.inputPeakDbR);

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
    gainReduction.store (engineMeters.gainReductionDb);
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
    const float requested = inputTrimBypassed.load() ? 0.0f : inputTrimDb.load();
    // The authoritative VST input faucet covers -12..+12 dB. Only attenuation
    // below -12 dB is completed by the outer rack gain, and multiplication is
    // still pre-engine, so the enhancer sees the requested total input level.
    enhancerBridge.setInputGainDb (requested);
}

void LiveAudioEngine::updateOutputTrimRouting() noexcept
{
    const float requested = outputTrimBypassed.load() ? 0.0f : outputTrimDb.load();
    // Positive and normal attenuation remain inside the exact VST lookahead
    // safety path. Only attenuation below -12 dB is completed after processing.
    enhancerBridge.setFinalOutputGainDb (requested);
}

bool LiveAudioEngine::looksLikeVirtualCable (const juce::String& name)
{
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input");
}

} // namespace askp::live
