#include "MonitorOutputClient.h"

#include <cmath>
#include <limits>

namespace askp::live {

MonitorOutputClient::MonitorOutputClient()
{
    ringBuffer.setSize (2, fifoCapacitySamples, false, true, false);
    ringBuffer.clear();
    resampleInput.setSize (2, resampleScratchCapacity, false, true, false);
    resampleOutput.setSize (2, resampleScratchCapacity, false, true, false);
    resampleInput.clear();
    resampleOutput.clear();
}

MonitorOutputClient::~MonitorOutputClient()
{
    shutdown();
}

bool MonitorOutputClient::initialise()
{
    const auto error = deviceManager.initialise (0, 0, nullptr, true);
    if (! callbackRegistered.exchange (true))
        deviceManager.addAudioCallback (this);

    if (error.isNotEmpty())
    {
        setError (error);
        return false;
    }

    setError ({});
    return true;
}

void MonitorOutputClient::shutdown()
{
    running.store (false);
    deviceOpen.store (false);
    primed.store (false);
    if (callbackRegistered.exchange (false))
        deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    flush();
}

void MonitorOutputClient::prepareSource (double sourceSampleRate,
                                         int sourceMaximumBlockSize)
{
    const double safeRate = juce::jmax (1.0, sourceSampleRate);
    const int safeBlock = juce::jmax (1, sourceMaximumBlockSize);
    sourceRate.store (safeRate);
    sourceBlockSize.store (safeBlock);
    primeTargetSamples.store (juce::jlimit (
        safeBlock * 3,
        fifoCapacitySamples / 2,
        juce::roundToInt (safeRate * 0.060)));
    flush();
}

void MonitorOutputClient::flush() noexcept
{
    fifo.reset();
    ringBuffer.clear();
    resampleInput.clear();
    resampleOutput.clear();
    leftInterpolator.reset();
    rightInterpolator.reset();
    currentDriftCorrectionPpm.store (0.0);
    rateConversionActive.store (false);
    primed.store (false);
}

juce::StringArray MonitorOutputClient::getOutputDeviceNames()
{
    juce::StringArray result;
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames (false))
            if (! isUnsafeVirtualOutput (name))
                result.add (name);
    }
    return result;
}

juce::String MonitorOutputClient::selectOutputDevice (const juce::String& name,
                                                       double preferredSampleRate)
{
    const auto requestedName = name.trim();
    if (requestedName.isEmpty())
    {
        deviceManager.closeAudioDevice();
        deviceOpen.store (false);
        running.store (false);
        {
            const juce::ScopedLock lock (statusLock);
            currentDeviceName.clear();
            errorMessage.clear();
        }
        flush();
        return {};
    }

    if (isUnsafeVirtualOutput (requestedName))
        return "Monitor output must be a physical speaker or headphone endpoint, not VB-CABLE.";

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        type->scanForDevices();

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName.clear();
    setup.outputDeviceName = requestedName;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    setup.bufferSize = 0;

    const double preferred = preferredSampleRate > 0.0
        ? preferredSampleRate : sourceRate.load();

    auto tryOpen = [this, &setup] (double rate)
    {
        setup.sampleRate = rate;
        return deviceManager.setAudioDeviceSetup (setup, true);
    };

    auto error = tryOpen (preferred);
    if (error.isNotEmpty())
    {
        // Many consumer Bluetooth/USB endpoints expose only their Windows mix
        // rate. Let JUCE open that native rate; the monitor resampler handles it.
        deviceManager.closeAudioDevice();
        error = tryOpen (0.0);
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    if (error.isEmpty() && device != nullptr
        && device->getCurrentSampleRate() <= 1.0)
    {
        const auto rates = device->getAvailableSampleRates();
        double nearest = 0.0;
        double nearestDistance = std::numeric_limits<double>::max();
        for (const auto rate : rates)
        {
            const double distance = std::abs (rate - preferred);
            if (rate > 1.0 && distance < nearestDistance)
            {
                nearest = rate;
                nearestDistance = distance;
            }
        }

        if (nearest > 1.0)
        {
            deviceManager.closeAudioDevice();
            error = tryOpen (nearest);
            device = deviceManager.getCurrentAudioDevice();
        }
    }

    if (error.isNotEmpty())
    {
        const juce::String message =
            "Unable to open monitor endpoint at its supported Windows sample rate: "
            + error;
        setError (message);
        return message;
    }

    device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr || device->getCurrentSampleRate() <= 1.0)
    {
        const juce::String message =
            "The selected monitor endpoint did not expose a usable playback sample rate.";
        deviceManager.closeAudioDevice();
        deviceOpen.store (false);
        running.store (false);
        setError (message);
        return message;
    }

    {
        const juce::ScopedLock lock (statusLock);
        currentDeviceName = device->getName();
        errorMessage.clear();
    }
    flush();
    return {};
}

void MonitorOutputClient::push (const juce::AudioBuffer<float>& source,
                                int numSamples) noexcept
{
    if (! running.load (std::memory_order_acquire)
        || source.getNumChannels() <= 0
        || numSamples <= 0)
        return;

    const int requested = juce::jmin (numSamples, source.getNumSamples());
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (requested, start1, size1, start2, size2);
    const int written = size1 + size2;
    if (written <= 0)
    {
        overflowCount.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const int sourceRight = juce::jmin (1, source.getNumChannels() - 1);
    if (size1 > 0)
    {
        ringBuffer.copyFrom (0, start1, source, 0, 0, size1);
        ringBuffer.copyFrom (1, start1, source, sourceRight, 0, size1);
    }
    if (size2 > 0)
    {
        ringBuffer.copyFrom (0, start2, source, 0, size1, size2);
        ringBuffer.copyFrom (1, start2, source, sourceRight, size1, size2);
    }
    fifo.finishedWrite (written);

    if (written < requested)
        overflowCount.fetch_add (1, std::memory_order_relaxed);
}

MonitorOutputClient::Status MonitorOutputClient::getStatus() const
{
    Status status;
    status.deviceOpen = deviceOpen.load();
    status.running = running.load();
    status.primed = primed.load();
    status.resampling = rateConversionActive.load();
    status.sourceSampleRate = sourceRate.load();
    status.sampleRate = outputRate.load();
    status.driftCorrectionPpm = currentDriftCorrectionPpm.load();
    status.bufferSize = outputBlockSize.load();
    status.bufferedSamples = fifo.getNumReady();
    status.underflows = underflowCount.load();
    status.overflows = overflowCount.load();
    const juce::ScopedLock lock (statusLock);
    status.deviceName = currentDeviceName;
    status.lastError = errorMessage;
    return status;
}

bool MonitorOutputClient::isUnsafeVirtualOutput (const juce::String& name) noexcept
{
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input")
        || name.containsIgnoreCase ("CABLE Output");
}

void MonitorOutputClient::audioDeviceIOCallbackWithContext (
    const float* const*, int,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

    if (! running.load (std::memory_order_acquire)
        || numOutputChannels <= 0
        || numSamples <= 0
        || numSamples > resampleScratchCapacity)
        return;

    const int targetFill = juce::jmax (1, primeTargetSamples.load());
    const int readyBeforeRead = fifo.getNumReady();
    if (! primed.load (std::memory_order_acquire))
    {
        if (readyBeforeRead < targetFill)
            return;
        primed.store (true, std::memory_order_release);
    }

    const double sourceSampleRate = juce::jmax (1.0, sourceRate.load());
    const double monitorSampleRate = juce::jmax (1.0, outputRate.load());
    const double baseRatio = sourceSampleRate / monitorSampleRate;
    const double fillError = static_cast<double> (readyBeforeRead - targetFill)
                           / static_cast<double> (targetFill);
    const double correctionPpm = juce::jlimit (
        -2500.0, 2500.0, fillError * 1400.0);
    const double adaptiveRatio = baseRatio * (1.0 + correctionPpm * 1.0e-6);
    currentDriftCorrectionPpm.store (correctionPpm, std::memory_order_relaxed);
    rateConversionActive.store (
        std::abs (baseRatio - 1.0) > 1.0e-6 || std::abs (correctionPpm) > 1.0,
        std::memory_order_relaxed);

    const int requiredInput = juce::jmin (
        resampleScratchCapacity,
        juce::jmax (8, static_cast<int> (std::ceil (
            static_cast<double> (numSamples) * adaptiveRatio)) + 8));

    if (readyBeforeRead < requiredInput)
    {
        underflowCount.fetch_add (1, std::memory_order_relaxed);
        primed.store (false, std::memory_order_release);
        return;
    }

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (requiredInput, start1, size1, start2, size2);
    const int staged = size1 + size2;
    if (staged < requiredInput)
    {
        fifo.finishedRead (staged);
        underflowCount.fetch_add (1, std::memory_order_relaxed);
        primed.store (false, std::memory_order_release);
        return;
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        if (size1 > 0)
            resampleInput.copyFrom (channel, 0, ringBuffer, channel, start1, size1);
        if (size2 > 0)
            resampleInput.copyFrom (channel, size1, ringBuffer, channel, start2, size2);
    }

    const int consumedLeft = leftInterpolator.process (
        adaptiveRatio, resampleInput.getReadPointer (0),
        resampleOutput.getWritePointer (0), numSamples);
    const int consumedRight = rightInterpolator.process (
        adaptiveRatio, resampleInput.getReadPointer (1),
        resampleOutput.getWritePointer (1), numSamples);
    const int consumed = juce::jlimit (
        0, staged, juce::jmin (consumedLeft, consumedRight));
    fifo.finishedRead (consumed);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] == nullptr)
            continue;
        const int sourceChannel = juce::jmin (channel, 1);
        juce::FloatVectorOperations::copy (
            outputChannelData[channel],
            resampleOutput.getReadPointer (sourceChannel), numSamples);
    }

    const bool silent = muted.load (std::memory_order_acquire);
    gainLinear.setTargetValue (juce::Decibels::decibelsToGain (gainDb.load()));
    const float gainStart = gainLinear.getCurrentValue();
    const float gainEnd = gainLinear.skip (numSamples);
    const float gainStep = (gainEnd - gainStart)
                         / static_cast<float> (juce::jmax (1, numSamples));

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        auto* output = outputChannelData[channel];
        if (output == nullptr)
            continue;
        if (silent)
        {
            juce::FloatVectorOperations::clear (output, numSamples);
            continue;
        }

        float gain = gainStart;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            output[sample] *= gain;
            gain += gainStep;
        }
    }
}

void MonitorOutputClient::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const double actualOutputRate = device->getCurrentSampleRate();
    outputRate.store (actualOutputRate);
    outputBlockSize.store (device->getCurrentBufferSizeSamples());
    gainLinear.reset (actualOutputRate, 0.020);
    gainLinear.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainDb.load()));
    primeTargetSamples.store (juce::jmax (
        sourceBlockSize.load() * 3,
        juce::roundToInt (sourceRate.load() * 0.060)));
    leftInterpolator.reset();
    rightInterpolator.reset();
    currentDriftCorrectionPpm.store (0.0);
    rateConversionActive.store (std::abs (sourceRate.load() - actualOutputRate) > 1.0);
    underflowCount.store (0);
    overflowCount.store (0);
    primed.store (false);
    deviceOpen.store (actualOutputRate > 1.0);
    running.store (actualOutputRate > 1.0);

    const juce::ScopedLock lock (statusLock);
    currentDeviceName = device->getName();
    errorMessage.clear();
}

void MonitorOutputClient::audioDeviceStopped()
{
    running.store (false);
    deviceOpen.store (false);
    primed.store (false);
    rateConversionActive.store (false);
    currentDriftCorrectionPpm.store (0.0);
    outputRate.store (0.0);
    outputBlockSize.store (0);
}

void MonitorOutputClient::audioDeviceError (const juce::String& errorMessage)
{
    setError (errorMessage);
    underflowCount.fetch_add (1, std::memory_order_relaxed);
}

void MonitorOutputClient::setError (juce::String message)
{
    const juce::ScopedLock lock (statusLock);
    errorMessage = std::move (message);
}

} // namespace askp::live
