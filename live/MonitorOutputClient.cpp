#include "MonitorOutputClient.h"

#include <cmath>

namespace askp::live {

MonitorOutputClient::MonitorOutputClient()
{
    ringBuffer.setSize (2, fifoCapacitySamples, false, true, false);
    ringBuffer.clear();
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
    sourceRate.store (juce::jmax (1.0, sourceSampleRate));
    sourceBlockSize.store (juce::jmax (1, sourceMaximumBlockSize));
    primeTargetSamples.store (juce::jlimit (
        sourceMaximumBlockSize * 2,
        fifoCapacitySamples / 2,
        juce::roundToInt (sourceSampleRate * 0.030)));
    flush();
}

void MonitorOutputClient::flush() noexcept
{
    fifo.reset();
    ringBuffer.clear();
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
    setup.sampleRate = preferredSampleRate > 0.0 ? preferredSampleRate : sourceRate.load();
    setup.bufferSize = 0;

    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
    {
        setError (error);
        return error;
    }

    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        const juce::String message = "The selected monitor endpoint did not open.";
        setError (message);
        return message;
    }

    const auto actualRate = device->getCurrentSampleRate();
    const auto expectedRate = sourceRate.load();
    if (expectedRate > 0.0 && std::abs (actualRate - expectedRate) > 1.0)
    {
        const juce::String message = "Monitor endpoint must support the stream sample rate ("
            + juce::String (expectedRate, 0) + " Hz). Opened at "
            + juce::String (actualRate, 0) + " Hz instead.";
        deviceManager.closeAudioDevice();
        deviceOpen.store (false);
        running.store (false);
        setError (message);
        return message;
    }

    {
        const juce::ScopedLock lock (statusLock);
        currentDeviceName = requestedName;
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
    status.sampleRate = outputRate.load();
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
        || numSamples <= 0)
        return;

    if (! primed.load (std::memory_order_acquire))
    {
        if (fifo.getNumReady() < primeTargetSamples.load())
            return;
        primed.store (true, std::memory_order_release);
    }

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (numSamples, start1, size1, start2, size2);
    const int read = size1 + size2;

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] == nullptr)
            continue;
        const int sourceChannel = juce::jmin (channel, 1);
        if (size1 > 0)
            juce::FloatVectorOperations::copy (
                outputChannelData[channel],
                ringBuffer.getReadPointer (sourceChannel, start1), size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy (
                outputChannelData[channel] + size1,
                ringBuffer.getReadPointer (sourceChannel, start2), size2);
    }
    fifo.finishedRead (read);

    if (read < numSamples)
    {
        underflowCount.fetch_add (1, std::memory_order_relaxed);
        primed.store (false, std::memory_order_release);
    }

    const bool silent = muted.load (std::memory_order_acquire);
    gainLinear.setTargetValue (juce::Decibels::decibelsToGain (gainDb.load()));
    const float gainStart = gainLinear.getCurrentValue();
    const float gainEnd = gainLinear.skip (numSamples);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] == nullptr)
            continue;
        if (silent)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);
        else
            juce::FloatVectorOperations::multiplyWithMultiply (
                outputChannelData[channel], gainStart,
                (gainEnd - gainStart) / static_cast<float> (juce::jmax (1, numSamples)),
                numSamples);
    }
}

void MonitorOutputClient::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    outputRate.store (device->getCurrentSampleRate());
    outputBlockSize.store (device->getCurrentBufferSizeSamples());
    gainLinear.reset (device->getCurrentSampleRate(), 0.020);
    gainLinear.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainDb.load()));
    primeTargetSamples.store (juce::jmax (
        device->getCurrentBufferSizeSamples() * 2,
        juce::roundToInt (device->getCurrentSampleRate() * 0.030)));
    underflowCount.store (0);
    overflowCount.store (0);
    primed.store (false);
    deviceOpen.store (true);
    running.store (true);

    const juce::ScopedLock lock (statusLock);
    currentDeviceName = device->getName();
    errorMessage.clear();
}

void MonitorOutputClient::audioDeviceStopped()
{
    running.store (false);
    deviceOpen.store (false);
    primed.store (false);
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
