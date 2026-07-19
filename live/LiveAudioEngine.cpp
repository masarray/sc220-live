#include "LiveAudioEngine.h"

#include <chrono>
#include <cmath>

namespace askp::live {
namespace {
constexpr auto presetId = "preset";
constexpr auto inputGainId = "input_gain_db";
constexpr auto finalOutputGainId = "final_output_gain_db";
constexpr auto enhanceId = "enhance";
constexpr auto smartBassId = "smart_bass";
constexpr auto smartTrebleId = "smart_treble";
constexpr auto vocalBodyId = "vocal_body";
constexpr auto stereoMagicId = "stereo_magic";
constexpr auto bypassId = "bypass";

float dbFromPeak (float peak) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
}
} // namespace

LiveAudioEngine::LiveAudioEngine()
{
    deviceManager.addChangeListener (this);
    enhancerProcessor.scopeTap.setEnabled (false);

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
    enhancerProcessor.releaseResources();
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
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = name;
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError = error;
    }
    refreshDeviceStatus();
    return error;
}

juce::String LiveAudioEngine::selectOutputDevice (const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = name;
    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
    {
        const juce::ScopedLock lock (statusLock);
        lastError = error;
    }
    refreshDeviceStatus();
    return error;
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
    inputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value));
    updateInputTrimRouting();
}

void LiveAudioEngine::setOutputTrimDb (float value) noexcept
{
    outputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value));
    updateOutputTrimRouting();
}

void LiveAudioEngine::setInputTrimBypassed (bool value) noexcept
{
    inputTrimBypassed.store (value);
    updateInputTrimRouting();
}

void LiveAudioEngine::setOutputTrimBypassed (bool value) noexcept
{
    outputTrimBypassed.store (value);
    updateOutputTrimRouting();
}

void LiveAudioEngine::setBypassed (bool value) noexcept
{
    enhancerBypassed.store (value);
    setAskpParameter (bypassId, value ? 1.0f : 0.0f);
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

void LiveAudioEngine::setPresetIndex (int index)
{
    const auto count = enhancerProcessor.presetNames().size();
    if (count <= 0)
        return;

    index = juce::jlimit (0, count - 1, index);
    setAskpParameter (presetId, static_cast<float> (index));

    // The VST normally performs this visible macro handoff on its 15 Hz message
    // timer. In the standalone rack we can call the same public authority method
    // immediately, so preset controls and audible transition begin together.
    enhancerProcessor.applyPresetMacroToParameters (index);
}

int LiveAudioEngine::getPresetIndex() const noexcept
{
    return juce::jlimit (0, juce::jmax (0, enhancerProcessor.presetNames().size() - 1),
                         static_cast<int> (std::round (getAskpParameter (presetId))));
}

juce::StringArray LiveAudioEngine::getPresetNames() const
{
    return enhancerProcessor.presetNames();
}

void LiveAudioEngine::setEnhance (float value) noexcept
{
    setAskpParameter (enhanceId, juce::jlimit (0.0f, 100.0f, value));
}

void LiveAudioEngine::setSmartBass (float value) noexcept
{
    setAskpParameter (smartBassId, juce::jlimit (0.0f, 100.0f, value));
}

void LiveAudioEngine::setSmartTreble (float value) noexcept
{
    setAskpParameter (smartTrebleId, juce::jlimit (0.0f, 100.0f, value));
}

void LiveAudioEngine::setVocalBody (float value) noexcept
{
    setAskpParameter (vocalBodyId, juce::jlimit (0.0f, 100.0f, value));
}

void LiveAudioEngine::setStereoMagic (float value) noexcept
{
    setAskpParameter (stereoMagicId, juce::jlimit (0.0f, 100.0f, value));
}

float LiveAudioEngine::getEnhance() const noexcept      { return getAskpParameter (enhanceId, 65.0f); }
float LiveAudioEngine::getSmartBass() const noexcept    { return getAskpParameter (smartBassId, 55.0f); }
float LiveAudioEngine::getSmartTreble() const noexcept  { return getAskpParameter (smartTrebleId, 70.0f); }
float LiveAudioEngine::getVocalBody() const noexcept    { return getAskpParameter (vocalBodyId, 65.0f); }
float LiveAudioEngine::getStereoMagic() const noexcept  { return getAskpParameter (stereoMagicId, 85.0f); }

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
    enhancerProcessor.processBlock (processBuffer, emptyMidi);

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

    enhancerProcessor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    enhancerProcessor.prepareToPlay (sampleRate, blockSize);
    updateInputTrimRouting();
    updateOutputTrimRouting();
    setAskpParameter (bypassId, enhancerBypassed.load() ? 1.0f : 0.0f);

    inputGainLinear.reset (sampleRate, 0.020);
    outputGainLinear.reset (sampleRate, 0.020);
    inputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        inputTrimBypassed.load() ? 0.0f : juce::jmin (0.0f, inputTrimDb.load() + 12.0f)));
    outputGainLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        outputTrimBypassed.load() ? 0.0f : juce::jmin (0.0f, outputTrimDb.load() + 12.0f)));

    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    totalLatencySamples.store (noiseClean.getLatencySamples()
                               + enhancerProcessor.getLatencySamples()
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
    enhancerProcessor.releaseResources();
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
    inputPeakL.store (enhancerProcessor.meters.inputPeakDbL.load (std::memory_order_relaxed));
    inputPeakR.store (enhancerProcessor.meters.inputPeakDbR.load (std::memory_order_relaxed));

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
    gainReduction.store (enhancerProcessor.meters.gainReductionDb.load (std::memory_order_relaxed));
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

void LiveAudioEngine::setAskpParameter (const char* parameterId, float plainValue) noexcept
{
    if (auto* parameter = enhancerProcessor.apvts.getParameter (parameterId))
    {
        const float normalised = parameter->convertTo0to1 (plainValue);
        if (std::abs (parameter->getValue() - normalised) > 0.000001f)
            parameter->setValueNotifyingHost (normalised);
    }
}

float LiveAudioEngine::getAskpParameter (const char* parameterId, float fallback) const noexcept
{
    if (const auto* value = enhancerProcessor.apvts.getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);
    return fallback;
}

void LiveAudioEngine::updateInputTrimRouting() noexcept
{
    const float requested = inputTrimBypassed.load() ? 0.0f : inputTrimDb.load();
    // The authoritative VST input faucet covers -12..+12 dB. Only attenuation
    // below -12 dB is completed by the outer rack gain, and multiplication is
    // still pre-engine, so the enhancer sees the requested total input level.
    setAskpParameter (inputGainId, juce::jlimit (-12.0f, 12.0f, requested));
}

void LiveAudioEngine::updateOutputTrimRouting() noexcept
{
    const float requested = outputTrimBypassed.load() ? 0.0f : outputTrimDb.load();
    // Positive and normal attenuation remain inside the exact VST lookahead
    // safety path. Only attenuation below -12 dB is completed after processing.
    setAskpParameter (finalOutputGainId, juce::jlimit (-12.0f, 12.0f, requested));
}

bool LiveAudioEngine::looksLikeVirtualCable (const juce::String& name)
{
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input");
}

} // namespace askp::live
