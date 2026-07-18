#include "LiveAudioEngine.h"

#include <chrono>
#include <cmath>

namespace askp::live {
LiveAudioEngine::LiveAudioEngine()
{
    deviceManager.addChangeListener (this);

    if (! arsonkupik::factory_presets().empty())
    {
        const auto& macro = arsonkupik::default_preset().macro;
        enhance.store (static_cast<float> (macro.enhance));
        smartBass.store (static_cast<float> (macro.smart_bass));
        smartTreble.store (static_cast<float> (macro.smart_treble));
        vocalBody.store (static_cast<float> (macro.vocal_body));
        stereoMagic.store (static_cast<float> (macro.stereo_magic));
    }

    for (int slot = 0; slot < engineSlotCount; ++slot)
    {
        enhancerSlots[static_cast<size_t> (slot)] = std::make_unique<arsonkupik::ArSonKuPikEngine>();
        engineSlotStates[static_cast<size_t> (slot)].store (slot == 0 ? slotActive : slotFree);
    }
    engineBuilder.startThread();
}

LiveAudioEngine::~LiveAudioEngine()
{
    engineBuilder.signalThreadShouldExit();
    engineBuilder.notify();
    engineBuilder.stopThread (2000);
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

    // Beginner-friendly best effort: if no stored output exists, prefer VB-CABLE.
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
    const auto& presets = arsonkupik::factory_presets();
    if (presets.empty())
        return;

    index = juce::jlimit (0, static_cast<int> (presets.size()) - 1, index);
    presetIndex.store (index);
    const auto& preset = presets[static_cast<size_t> (index)];
    enhance.store (static_cast<float> (preset.macro.enhance));
    smartBass.store (static_cast<float> (preset.macro.smart_bass));
    smartTreble.store (static_cast<float> (preset.macro.smart_treble));
    vocalBody.store (static_cast<float> (preset.macro.vocal_body));
    stereoMagic.store (static_cast<float> (preset.macro.stereo_magic));
    requestEngineRebuild();
}

juce::StringArray LiveAudioEngine::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& preset : arsonkupik::factory_presets())
        names.add (preset.name);
    return names;
}

void LiveAudioEngine::setEnhance (float value) noexcept
{
    enhance.store (juce::jlimit (0.0f, 100.0f, value));
    requestEngineRebuild();
}

void LiveAudioEngine::setSmartBass (float value) noexcept
{
    smartBass.store (juce::jlimit (0.0f, 100.0f, value));
    requestEngineRebuild();
}

void LiveAudioEngine::setSmartTreble (float value) noexcept
{
    smartTreble.store (juce::jlimit (0.0f, 100.0f, value));
    requestEngineRebuild();
}

void LiveAudioEngine::setVocalBody (float value) noexcept
{
    vocalBody.store (juce::jlimit (0.0f, 100.0f, value));
    requestEngineRebuild();
}

void LiveAudioEngine::setStereoMagic (float value) noexcept
{
    stereoMagic.store (juce::jlimit (0.0f, 100.0f, value));
    requestEngineRebuild();
}

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
    status.visualDrops = visualTap.getDroppedBlocks();
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

    const auto preGainDb = inputTrimDb.load();
    if (std::abs (preGainDb) > 0.001f)
        processBuffer.applyGain (juce::Decibels::decibelsToGain (preGainDb));

    // Noise Clean is an independent rack module. Even when disabled it keeps
    // deterministic FFT latency by outputting a delayed dry signal.
    noiseClean.process (processBuffer);

    // Expander detector is the cleaned input before enhancement, preventing
    // compressor/makeup motion from falsely holding the expander open.
    for (int channel = 0; channel < processingChannels; ++channel)
        detectorBuffer.copyFrom (channel, 0, processBuffer, channel, 0, numSamples);

    activatePendingEngineIfReady (bypassed.load());
    if (! bypassed.load())
        processEnhancerWithTransition (processingChannels, numSamples);

    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (processBuffer, detectorBuffer);

    // Output trim belongs to the post-enhancer rack but is kept before the
    // non-bypassable safety limiter, so positive trim can never break ceiling.
    const auto postGainDb = outputTrimDb.load();
    if (std::abs (postGainDb) > 0.001f)
        processBuffer.applyGain (juce::Decibels::decibelsToGain (postGainDb));

    // Safety limiter is deliberately non-bypassable and remains the final stage.
    safetyLimiter.process (processBuffer);

    // Visual-only copy is lossy and lock-free. A full visual queue drops data
    // rather than extending the audio callback.
    if (processingChannels > 0)
        visualTap.push (processBuffer.getReadPointer (0),
                        processingChannels > 1 ? processBuffer.getReadPointer (1) : nullptr,
                        numSamples);

    updateMeters (detectorBuffer, processBuffer);

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
    callbackPeakPercent.store (blockCpu > previousPeak ? blockCpu
                                                       : previousPeak * 0.9975f,
                               std::memory_order_relaxed);
}

void LiveAudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = juce::jmax (64, device->getCurrentBufferSizeSamples());
    const int channels = 2;

    processBuffer.setSize (channels, blockSize, false, true, false);
    detectorBuffer.setSize (channels, blockSize, false, true, false);
    transitionBuffer.setSize (channels, blockSize, false, true, false);
    processBuffer.clear();
    detectorBuffer.clear();
    transitionBuffer.clear();

    noiseClean.prepare (sampleRate, blockSize, channels);
    expander.prepare (sampleRate);
    safetyLimiter.prepare (sampleRate);

    {
        // Audio callbacks are not running while aboutToStart executes. Serialise
        // with the background builder, reset the fixed slots, and prepare one
        // immediately active engine for a click-free first block.
        const juce::ScopedLock buildGuard (engineBuildLock);
        pendingEngineSlot.store (-1);
        transitionOldSlot = -1;
        transitionSamplesRemaining = 0;
        activeEngineSlot.store (0);
        for (int slot = 0; slot < engineSlotCount; ++slot)
            engineSlotStates[static_cast<size_t> (slot)].store (slot == 0 ? slotActive : slotFree);

        enhancerSlots[0] = std::make_unique<arsonkupik::ArSonKuPikEngine>();
        enhancerSlots[0]->prepare (sampleRate, static_cast<size_t> (channels));
        enhancerSlots[0]->set_runtime_params (makeRuntimeParamsSnapshot());
        enhancerSlots[0]->reset();
        builtRevision.store (parameterRevision.load());
    }

    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    totalLatencySamples.store (noiseClean.getLatencySamples()
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

void LiveAudioEngine::EngineBuilderThread::run()
{
    while (! threadShouldExit())
    {
        wait (-1);
        if (threadShouldExit())
            break;

        // Coalesce dense mouse-drag notifications. A complete prepared engine is
        // heavier than the VST's in-place coefficient retune, so Live limits full
        // rebuilds to roughly 35 Hz while preserving smooth visible control drag.
        auto observed = owner.parameterRevision.load (std::memory_order_acquire);
        for (;;)
        {
            wait (28);
            const auto latest = owner.parameterRevision.load (std::memory_order_acquire);
            if (latest == observed || threadShouldExit())
                break;
            observed = latest;
        }

        while (! threadShouldExit())
        {
            const auto wanted = owner.parameterRevision.load (std::memory_order_acquire);
            const auto built = owner.builtRevision.load (std::memory_order_acquire);
            if (wanted == built)
                break;
            if (! owner.deviceOpen.load (std::memory_order_acquire))
                break;
            if (! owner.buildLatestEngineOffAudioThread())
            {
                wait (16);
                continue;
            }
        }
    }
}

void LiveAudioEngine::requestEngineRebuild() noexcept
{
    parameterRevision.fetch_add (1, std::memory_order_acq_rel);
    engineBuilder.notify();
}

arsonkupik::RuntimeParams LiveAudioEngine::makeRuntimeParamsSnapshot() const
{
    arsonkupik::RuntimeParams params;
    const auto& presets = arsonkupik::factory_presets();
    if (presets.empty())
        return params;

    const int index = juce::jlimit (0, static_cast<int> (presets.size()) - 1, presetIndex.load());
    const auto& preset = presets[static_cast<size_t> (index)];
    // Match the current VST wrapper contract exactly. The numeric preset index
    // avoids temporary string work, while the app owns bypass and final safety.
    params.preset_index = index;
    params.enhance = enhance.load();
    params.smart_bass = smartBass.load();
    params.smart_treble = smartTreble.load();
    params.vocal_body = vocalBody.load();
    params.stereo_magic = stereoMagic.load();
    params.output_trim_db = preset.output.output_gain_db;
    params.smart_protect = preset.macro.smart_protect;
    params.bypass = false;
    params.external_bypass = true;
    params.external_peak_limiter = true;
    params.advanced_override = true;
    return params;
}

bool LiveAudioEngine::buildLatestEngineOffAudioThread()
{
    const auto sampleRate = currentSampleRate.load();
    const auto channels = 2;
    if (sampleRate <= 0.0 || ! deviceOpen.load())
        return false;

    const juce::ScopedLock buildGuard (engineBuildLock);

    int freeSlot = -1;
    for (int slot = 0; slot < engineSlotCount; ++slot)
    {
        int expected = slotFree;
        if (engineSlotStates[static_cast<size_t> (slot)].compare_exchange_strong (
                expected, slotBuilding, std::memory_order_acq_rel))
        {
            freeSlot = slot;
            break;
        }
    }

    if (freeSlot < 0)
        return false;

    const auto revision = parameterRevision.load (std::memory_order_acquire);
    auto replacement = std::make_unique<arsonkupik::ArSonKuPikEngine>();
    replacement->prepare (sampleRate, static_cast<size_t> (channels));
    replacement->set_runtime_params (makeRuntimeParamsSnapshot());
    replacement->reset();
    enhancerSlots[static_cast<size_t> (freeSlot)] = std::move (replacement);
    engineSlotStates[static_cast<size_t> (freeSlot)].store (slotPending, std::memory_order_release);

    const int replacedPending = pendingEngineSlot.exchange (freeSlot, std::memory_order_acq_rel);
    if (replacedPending >= 0 && replacedPending != freeSlot)
        engineSlotStates[static_cast<size_t> (replacedPending)].store (slotFree, std::memory_order_release);

    builtRevision.store (revision, std::memory_order_release);
    return true;
}

void LiveAudioEngine::activatePendingEngineIfReady (bool instant) noexcept
{
    // Enhancer bypass makes the old/new crossfade inaudible. Release the old
    // transition slot immediately so a rapid bypass during a parameter change
    // cannot strand one of the three prepared-engine slots.
    if (instant && transitionOldSlot >= 0)
    {
        engineSlotStates[static_cast<size_t> (transitionOldSlot)].store (slotFree,
                                                                        std::memory_order_release);
        transitionOldSlot = -1;
        transitionSamplesRemaining = 0;
        engineBuilder.notify();
    }

    if (transitionOldSlot >= 0)
        return;

    const int incoming = pendingEngineSlot.exchange (-1, std::memory_order_acq_rel);
    if (incoming < 0)
        return;

    const int outgoing = activeEngineSlot.exchange (incoming, std::memory_order_acq_rel);
    engineSlotStates[static_cast<size_t> (incoming)].store (slotActive, std::memory_order_release);

    if (instant || outgoing < 0)
    {
        if (outgoing >= 0)
            engineSlotStates[static_cast<size_t> (outgoing)].store (slotFree, std::memory_order_release);
        transitionOldSlot = -1;
        transitionSamplesRemaining = 0;
        engineBuilder.notify();
        return;
    }

    transitionOldSlot = outgoing;
    engineSlotStates[static_cast<size_t> (outgoing)].store (slotTransition, std::memory_order_release);
    transitionTotalSamples = juce::jmax (1, static_cast<int> (std::round (currentSampleRate.load() * 0.005)));
    transitionSamplesRemaining = transitionTotalSamples;
}

void LiveAudioEngine::processEnhancerWithTransition (int channels, int numSamples) noexcept
{
    const int active = activeEngineSlot.load (std::memory_order_acquire);
    if (active < 0 || enhancerSlots[static_cast<size_t> (active)] == nullptr)
        return;

    if (transitionOldSlot >= 0)
        transitionBuffer.makeCopyOf (processBuffer, true);

    std::array<float*, arsonkupik::kMaxChannels> activePlanes {};
    for (int channel = 0; channel < channels; ++channel)
        activePlanes[static_cast<size_t> (channel)] = processBuffer.getWritePointer (channel);
    enhancerSlots[static_cast<size_t> (active)]->process (
        activePlanes.data(), static_cast<size_t> (channels), static_cast<size_t> (numSamples));

    if (transitionOldSlot < 0)
        return;

    auto* oldEngine = enhancerSlots[static_cast<size_t> (transitionOldSlot)].get();
    if (oldEngine == nullptr)
    {
        transitionOldSlot = -1;
        transitionSamplesRemaining = 0;
        return;
    }

    std::array<float*, arsonkupik::kMaxChannels> oldPlanes {};
    for (int channel = 0; channel < channels; ++channel)
        oldPlanes[static_cast<size_t> (channel)] = transitionBuffer.getWritePointer (channel);
    oldEngine->process (oldPlanes.data(), static_cast<size_t> (channels), static_cast<size_t> (numSamples));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float progress = 1.0f - static_cast<float> (transitionSamplesRemaining)
                                     / static_cast<float> (transitionTotalSamples);
        const float newGain = juce::jlimit (0.0f, 1.0f, progress);
        const float oldGain = 1.0f - newGain; // unity-preserving for correlated paths
        for (int channel = 0; channel < channels; ++channel)
        {
            const float oldSample = transitionBuffer.getSample (channel, sample);
            const float newSample = processBuffer.getSample (channel, sample);
            processBuffer.setSample (channel, sample, oldSample * oldGain + newSample * newGain);
        }
        if (transitionSamplesRemaining > 0)
            --transitionSamplesRemaining;
    }

    if (transitionSamplesRemaining <= 0)
    {
        engineSlotStates[static_cast<size_t> (transitionOldSlot)].store (slotFree, std::memory_order_release);
        transitionOldSlot = -1;
        engineBuilder.notify();
    }
}

void LiveAudioEngine::clearOutputs (float* const* outputChannelData,
                                    int channels, int samples) noexcept
{
    for (int channel = 0; channel < channels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], samples);
}

void LiveAudioEngine::updateMeters (const juce::AudioBuffer<float>& input,
                                    const juce::AudioBuffer<float>& output) noexcept
{
    const int samples = juce::jmin (input.getNumSamples(), output.getNumSamples());
    const int inputRight = juce::jmin (1, input.getNumChannels() - 1);
    const int outputRight = juce::jmin (1, output.getNumChannels() - 1);
    const float* inputL = input.getReadPointer (0);
    const float* inputR = input.getReadPointer (inputRight);
    const float* outputL = output.getReadPointer (0);
    const float* outputR = output.getReadPointer (outputRight);
    float inPeakL = 0.0f, inPeakR = 0.0f, outPeakL = 0.0f, outPeakR = 0.0f;
    for (int sample = 0; sample < samples; ++sample)
    {
        inPeakL = juce::jmax (inPeakL, std::abs (inputL[sample]));
        inPeakR = juce::jmax (inPeakR, std::abs (inputR[sample]));
        outPeakL = juce::jmax (outPeakL, std::abs (outputL[sample]));
        outPeakR = juce::jmax (outPeakR, std::abs (outputR[sample]));
    }

    const float releaseDb = static_cast<float> (26.0 * static_cast<double> (samples)
                                                / juce::jmax (1.0, currentSampleRate.load()));
    const auto toDb = [] (float peak) noexcept
    {
        return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
    };
    inputPeakL.store (juce::jmax (toDb (inPeakL), inputPeakL.load() - releaseDb));
    inputPeakR.store (juce::jmax (toDb (inPeakR), inputPeakR.load() - releaseDb));
    outputPeakL.store (juce::jmax (toDb (outPeakL), outputPeakL.load() - releaseDb));
    outputPeakR.store (juce::jmax (toDb (outPeakR), outputPeakR.load() - releaseDb));

    const int active = activeEngineSlot.load (std::memory_order_acquire);
    const float enhancerReduction = active >= 0 && enhancerSlots[static_cast<size_t> (active)] != nullptr
                                  ? static_cast<float> (enhancerSlots[static_cast<size_t> (active)]->meters().gain_reduction_db)
                                  : 0.0f;
    gainReduction.store (enhancerReduction + safetyLimiter.getGainReductionDb());
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

bool LiveAudioEngine::looksLikeVirtualCable (const juce::String& name)
{
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input");
}

} // namespace askp::live
