#include "MonitorOutputClient.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #include <windows.h>
 #include <audioclient.h>
 #include <functiondiscoverykeys_devpkey.h>
 #include <mmdeviceapi.h>
 #include <propvarutil.h>
 #include <wrl/client.h>
#endif

namespace askp::live {

namespace {

constexpr auto systemDefaultMonitorName = "SYSTEM DEFAULT";

#if JUCE_WINDOWS
using Microsoft::WRL::ComPtr;

struct ThreadComApartment final
{
    ThreadComApartment()
    {
        result = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        balance = SUCCEEDED (result);
        available = balance || result == RPC_E_CHANGED_MODE;
    }

    ~ThreadComApartment()
    {
        if (balance)
            CoUninitialize();
    }

    HRESULT result = E_FAIL;
    bool balance = false;
    bool available = false;
};

bool ensureComForCurrentThread() noexcept
{
    thread_local ThreadComApartment apartment;
    return apartment.available;
}

juce::String hresultText (HRESULT result)
{
    const auto value = static_cast<juce::int64> (
        static_cast<unsigned long> (result));
    return "0x" + juce::String::toHexString (value)
                         .paddedLeft ('0', 8)
                         .toUpperCase();
}

juce::String endpointFriendlyName (IMMDevice* device)
{
    if (device == nullptr)
        return {};

    ComPtr<IPropertyStore> properties;
    if (FAILED (device->OpenPropertyStore (STGM_READ, &properties)))
        return {};

    PROPVARIANT value;
    PropVariantInit (&value);
    const auto result = properties->GetValue (PKEY_Device_FriendlyName, &value);
    juce::String name;
    if (SUCCEEDED (result) && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
        name = juce::String (value.pwszVal).trim();
    PropVariantClear (&value);
    return name;
}

std::wstring endpointId (IMMDevice* device)
{
    if (device == nullptr)
        return {};

    LPWSTR rawId = nullptr;
    if (FAILED (device->GetId (&rawId)) || rawId == nullptr)
        return {};

    std::wstring result (rawId);
    CoTaskMemFree (rawId);
    return result;
}

struct EndpointInfo
{
    juce::String label;
    juce::String friendlyName;
    std::wstring id;
};

bool createEnumerator (ComPtr<IMMDeviceEnumerator>& enumerator)
{
    if (! ensureComForCurrentThread())
        return false;

    return SUCCEEDED (CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr,
                                        CLSCTX_ALL, IID_PPV_ARGS (&enumerator)));
}

bool getDefaultEndpointInfo (EndpointInfo& info)
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (! createEnumerator (enumerator))
        return false;

    ComPtr<IMMDevice> device;
    if (FAILED (enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &device)))
        return false;

    info.friendlyName = endpointFriendlyName (device.Get());
    info.id = endpointId (device.Get());
    info.label = systemDefaultMonitorName;
    return info.friendlyName.isNotEmpty() && ! info.id.empty();
}

std::vector<EndpointInfo> enumerateActiveEndpoints()
{
    std::vector<EndpointInfo> result;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (! createEnumerator (enumerator))
        return result;

    ComPtr<IMMDeviceCollection> collection;
    if (FAILED (enumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE,
                                                &collection)))
        return result;

    UINT count = 0;
    if (FAILED (collection->GetCount (&count)))
        return result;

    for (UINT index = 0; index < count; ++index)
    {
        ComPtr<IMMDevice> device;
        if (FAILED (collection->Item (index, &device)))
            continue;

        EndpointInfo endpoint;
        endpoint.friendlyName = endpointFriendlyName (device.Get());
        endpoint.id = endpointId (device.Get());
        if (endpoint.friendlyName.isEmpty() || endpoint.id.empty()
            || MonitorOutputClient::isUnsafeVirtualOutput (endpoint.friendlyName))
            continue;

        endpoint.label = endpoint.friendlyName;
        int suffix = 2;
        while (std::any_of (result.begin(), result.end(),
                            [&endpoint] (const EndpointInfo& existing)
                            {
                                return existing.label.equalsIgnoreCase (endpoint.label);
                            }))
        {
            endpoint.label = endpoint.friendlyName + " [" + juce::String (suffix++) + "]";
        }

        result.push_back (std::move (endpoint));
    }

    return result;
}

bool resolveEndpoint (const juce::String& requested, EndpointInfo& endpoint,
                      bool& useDefault)
{
    useDefault = requested.equalsIgnoreCase (systemDefaultMonitorName);
    if (useDefault)
        return getDefaultEndpointInfo (endpoint);

    const auto endpoints = enumerateActiveEndpoints();
    const auto found = std::find_if (endpoints.begin(), endpoints.end(),
        [&requested] (const EndpointInfo& candidate)
        {
            return candidate.label.equalsIgnoreCase (requested)
                || candidate.friendlyName.equalsIgnoreCase (requested);
        });

    if (found == endpoints.end())
        return false;

    endpoint = *found;
    return true;
}
#endif

} // namespace

struct MonitorOutputClient::Impl
{
    static constexpr int fifoCapacitySamples = 384000;
    static constexpr int resampleScratchCapacity = 65536;
    static constexpr int interpolationGuardSamples = 12;

    enum class SampleEncoding
    {
        unsupported,
        float32,
        pcm16,
        pcm24,
        pcm32
    };

    Impl()
    {
        ringBuffer.setSize (2, fifoCapacitySamples, false, true, false);
        resampleInput.setSize (2, resampleScratchCapacity, false, true, false);
        resampleOutput.setSize (2, resampleScratchCapacity, false, true, false);
        ringBuffer.clear();
        resampleInput.clear();
        resampleOutput.clear();
    }

    void setError (juce::String message)
    {
        const juce::ScopedLock lock (statusLock);
        errorMessage = std::move (message);
    }

    void clearError()
    {
        const juce::ScopedLock lock (statusLock);
        errorMessage.clear();
    }

    juce::String getError() const
    {
        const juce::ScopedLock lock (statusLock);
        return errorMessage;
    }

    void resetTransportLocked() noexcept
    {
        fifo.reset();
        ringBuffer.clear();
        resampleInput.clear();
        resampleOutput.clear();
        leftInterpolator.reset();
        rightInterpolator.reset();
        currentDriftCorrectionPpm.store (0.0, std::memory_order_relaxed);
        endpointPaddingFrames.store (0, std::memory_order_relaxed);
        primed.store (false, std::memory_order_release);
        endpointStartedFlag.store (false, std::memory_order_release);
#if JUCE_WINDOWS
        endpointStarted = false;
#endif
    }

    void enqueueLocked (const juce::AudioBuffer<float>& source,
                        int numSamples) noexcept
    {
        const int requested = juce::jmin (numSamples, source.getNumSamples());
        if (requested <= 0 || source.getNumChannels() <= 0)
            return;

        const int freeSpace = fifo.getFreeSpace();
        if (freeSpace < requested)
        {
            const int discard = requested - freeSpace;
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead (discard, start1, size1, start2, size2);
            fifo.finishedRead (size1 + size2);
            overflowCount.fetch_add (1, std::memory_order_relaxed);
        }

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (requested, start1, size1, start2, size2);
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

        fifo.finishedWrite (size1 + size2);
    }

#if JUCE_WINDOWS
    bool configureNativeFormat (const WAVEFORMATEX& format)
    {
        outputChannels = static_cast<int> (format.nChannels);
        outputBlockAlign = static_cast<int> (format.nBlockAlign);
        outputBitsPerSample = static_cast<int> (format.wBitsPerSample);
        outputValidBits = outputBitsPerSample;

        WORD formatTag = format.wFormatTag;
        if (formatTag == WAVE_FORMAT_EXTENSIBLE
            && format.cbSize >= sizeof (WAVEFORMATEXTENSIBLE) - sizeof (WAVEFORMATEX))
        {
            const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*> (&format);
            formatTag = static_cast<WORD> (extensible->SubFormat.Data1);
            outputValidBits = static_cast<int> (extensible->Samples.wValidBitsPerSample);
        }

        if (formatTag == WAVE_FORMAT_IEEE_FLOAT && outputBitsPerSample == 32)
            outputEncoding = SampleEncoding::float32;
        else if (formatTag == WAVE_FORMAT_PCM && outputBitsPerSample == 16)
            outputEncoding = SampleEncoding::pcm16;
        else if (formatTag == WAVE_FORMAT_PCM && outputBitsPerSample == 24)
            outputEncoding = SampleEncoding::pcm24;
        else if (formatTag == WAVE_FORMAT_PCM && outputBitsPerSample == 32)
            outputEncoding = SampleEncoding::pcm32;
        else
            outputEncoding = SampleEncoding::unsupported;

        return outputChannels > 0
            && outputBlockAlign >= outputChannels * juce::jmax (1, outputBitsPerSample / 8)
            && outputEncoding != SampleEncoding::unsupported;
    }

    void closeEndpointLocked (bool clearSelection) noexcept
    {
        if (audioClient != nullptr && endpointStarted)
            audioClient->Stop();

        renderClient.Reset();
        audioClient.Reset();
        endpointBufferFrameCount = 0;
        outputChannels = 0;
        outputBlockAlign = 0;
        outputBitsPerSample = 0;
        outputValidBits = 0;
        outputEncoding = SampleEncoding::unsupported;
        deviceOpen.store (false, std::memory_order_release);
        running.store (false, std::memory_order_release);
        outputRate.store (0.0, std::memory_order_release);
        outputBlockSize.store (0, std::memory_order_release);
        rateConversionActive.store (false, std::memory_order_release);
        resetTransportLocked();

        if (clearSelection)
        {
            selectedDeviceId.clear();
            selectedDisplayName.clear();
            selectedDefault = false;
            const juce::ScopedLock lock (statusLock);
            currentDeviceName.clear();
            errorMessage.clear();
        }
    }

    bool openSelectedEndpointLocked (float initialGainDb, bool reconnect)
    {
        if (selectedDisplayName.isEmpty() || ! ensureComForCurrentThread())
            return false;

        closeEndpointLocked (false);

        ComPtr<IMMDeviceEnumerator> enumerator;
        if (! createEnumerator (enumerator))
        {
            setError ("WASAPI monitor could not create MMDeviceEnumerator.");
            return false;
        }

        ComPtr<IMMDevice> device;
        HRESULT result = selectedDefault
            ? enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &device)
            : enumerator->GetDevice (selectedDeviceId.c_str(), &device);
        if (FAILED (result))
        {
            setError ("WASAPI monitor could not resolve the selected endpoint ("
                      + hresultText (result) + ").");
            return false;
        }

        const auto actualFriendlyName = endpointFriendlyName (device.Get());
        if (MonitorOutputClient::isUnsafeVirtualOutput (actualFriendlyName))
        {
            setError ("Monitor output must be a physical speaker or headphone endpoint, not VB-CABLE.");
            return false;
        }

        ComPtr<IAudioClient> nextClient;
        result = device->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**> (nextClient.GetAddressOf()));
        if (FAILED (result))
        {
            setError ("WASAPI monitor failed to activate IAudioClient ("
                      + hresultText (result) + ").");
            return false;
        }

        WAVEFORMATEX* mixFormat = nullptr;
        result = nextClient->GetMixFormat (&mixFormat);
        if (FAILED (result) || mixFormat == nullptr)
        {
            setError ("WASAPI monitor failed to read the endpoint mix format ("
                      + hresultText (result) + ").");
            return false;
        }

        const double nativeRate = static_cast<double> (mixFormat->nSamplesPerSec);
        const bool supportedFormat = configureNativeFormat (*mixFormat);
        if (! supportedFormat || nativeRate <= 1.0)
        {
            CoTaskMemFree (mixFormat);
            setError ("WASAPI monitor endpoint exposes an unsupported native mix format.");
            return false;
        }

        // OBS Studio uses the endpoint's own mix format in shared mode and a
        // generous one-second client buffer. Actual confidence-monitor latency
        // is controlled by how much audio we prefill, not by this capacity.
        constexpr REFERENCE_TIME requestedBufferDuration100ns = 10000000;
        result = nextClient->Initialize (AUDCLNT_SHAREMODE_SHARED, 0,
                                         requestedBufferDuration100ns, 0,
                                         mixFormat, nullptr);
        CoTaskMemFree (mixFormat);
        if (FAILED (result))
        {
            setError ("WASAPI shared-mode monitor initialization failed ("
                      + hresultText (result) + ").");
            return false;
        }

        UINT32 nextBufferFrames = 0;
        result = nextClient->GetBufferSize (&nextBufferFrames);
        if (FAILED (result) || nextBufferFrames == 0)
        {
            setError ("WASAPI monitor failed to obtain endpoint buffer size ("
                      + hresultText (result) + ").");
            return false;
        }

        ComPtr<IAudioRenderClient> nextRender;
        result = nextClient->GetService (__uuidof (IAudioRenderClient),
                                         reinterpret_cast<void**> (nextRender.GetAddressOf()));
        if (FAILED (result))
        {
            setError ("WASAPI monitor failed to obtain IAudioRenderClient ("
                      + hresultText (result) + ").");
            return false;
        }

        audioClient = std::move (nextClient);
        renderClient = std::move (nextRender);
        endpointBufferFrameCount = nextBufferFrames;
        outputRate.store (nativeRate, std::memory_order_release);
        outputBlockSize.store (static_cast<int> (nextBufferFrames),
                               std::memory_order_release);
        const double safeSourceRate = juce::jmax (1.0, sourceRate.load());
        primeTargetSamples.store (juce::jlimit (
            sourceBlockSize.load() * 3,
            fifoCapacitySamples / 4,
            juce::roundToInt (safeSourceRate * 0.030)),
            std::memory_order_release);
        gainLinear.reset (nativeRate, 0.020);
        gainLinear.setCurrentAndTargetValue (
            juce::Decibels::decibelsToGain (initialGainDb));
        resetTransportLocked();
        deviceOpen.store (true, std::memory_order_release);
        running.store (true, std::memory_order_release);
        rateConversionActive.store (
            std::abs (safeSourceRate - nativeRate) > 1.0
                || outputChannels != 2
                || outputEncoding != SampleEncoding::float32,
            std::memory_order_release);
        reconnectPending = false;
        lastReconnectAttemptMs = 0.0;
        if (reconnect)
            reconnectCount.fetch_add (1, std::memory_order_relaxed);

        {
            const juce::ScopedLock lock (statusLock);
            currentDeviceName = selectedDisplayName;
            errorMessage.clear();
        }
        return true;
    }

    void markForReconnectLocked (const juce::String& operation,
                                 HRESULT result) noexcept
    {
        if (audioClient != nullptr && endpointStarted)
            audioClient->Stop();
        renderClient.Reset();
        audioClient.Reset();
        endpointBufferFrameCount = 0;
        endpointStarted = false;
        endpointStartedFlag.store (false, std::memory_order_release);
        primed.store (false, std::memory_order_release);
        deviceOpen.store (false, std::memory_order_release);
        running.store (false, std::memory_order_release);
        endpointPaddingFrames.store (0, std::memory_order_relaxed);
        reconnectPending = true;
        resetTransportLocked();
        setError (operation + " failed; monitor will reconnect automatically ("
                  + hresultText (result) + ").");
    }

    static float clampAudioSample (float sample) noexcept
    {
        return juce::jlimit (-1.0f, 1.0f, sample);
    }

    void writeOneSample (BYTE* destination, float sample) const noexcept
    {
        sample = clampAudioSample (sample);
        switch (outputEncoding)
        {
            case SampleEncoding::float32:
            {
                std::memcpy (destination, &sample, sizeof (sample));
                break;
            }
            case SampleEncoding::pcm16:
            {
                const auto value = static_cast<int16_t> (
                    std::lrint (sample * 32767.0f));
                std::memcpy (destination, &value, sizeof (value));
                break;
            }
            case SampleEncoding::pcm24:
            {
                const auto value = static_cast<int32_t> (
                    std::lrint (sample * 8388607.0f));
                destination[0] = static_cast<BYTE> (value & 0xff);
                destination[1] = static_cast<BYTE> ((value >> 8) & 0xff);
                destination[2] = static_cast<BYTE> ((value >> 16) & 0xff);
                break;
            }
            case SampleEncoding::pcm32:
            {
                const double scale = outputValidBits > 0 && outputValidBits < 32
                    ? static_cast<double> ((uint64_t { 1 } << (outputValidBits - 1)) - 1)
                    : 2147483647.0;
                int64_t value64 = static_cast<int64_t> (std::llround (sample * scale));
                if (outputValidBits > 0 && outputValidBits < 32)
                    value64 <<= (32 - outputValidBits);
                const auto value = static_cast<int32_t> (value64);
                std::memcpy (destination, &value, sizeof (value));
                break;
            }
            case SampleEncoding::unsupported:
                break;
        }
    }

    void writeInterleaved (BYTE* destination, int frames,
                           float gainStart, float gainEnd) const noexcept
    {
        if (destination == nullptr || frames <= 0)
            return;

        const int bytesPerSample = juce::jmax (1, outputBitsPerSample / 8);
        const float gainStep = (gainEnd - gainStart)
                             / static_cast<float> (juce::jmax (1, frames));
        float gain = gainStart;
        const auto* left = resampleOutput.getReadPointer (0);
        const auto* right = resampleOutput.getReadPointer (1);

        for (int frame = 0; frame < frames; ++frame)
        {
            auto* frameDestination = destination + frame * outputBlockAlign;
            const float leftSample = left[frame] * gain;
            const float rightSample = right[frame] * gain;

            for (int channel = 0; channel < outputChannels; ++channel)
            {
                float sample = 0.0f;
                if (outputChannels == 1)
                    sample = 0.5f * (leftSample + rightSample);
                else if (channel == 0)
                    sample = leftSample;
                else if (channel == 1)
                    sample = rightSample;

                writeOneSample (frameDestination + channel * bytesPerSample, sample);
            }
            gain += gainStep;
        }
    }

    void renderAvailableLocked (float requestedGainDb, bool muted) noexcept
    {
        if (audioClient == nullptr || renderClient == nullptr
            || endpointBufferFrameCount == 0)
            return;

        UINT32 padding = 0;
        auto result = audioClient->GetCurrentPadding (&padding);
        if (FAILED (result))
        {
            markForReconnectLocked ("WASAPI GetCurrentPadding", result);
            return;
        }
        endpointPaddingFrames.store (static_cast<int> (padding),
                                     std::memory_order_relaxed);

        const int ready = fifo.getNumReady();
        const int primeTarget = juce::jmax (1, primeTargetSamples.load());
        if (! endpointStarted && ready < primeTarget)
            return;

        const UINT32 availableFrames = endpointBufferFrameCount > padding
            ? endpointBufferFrameCount - padding : 0;
        if (availableFrames == 0)
            return;

        const double sourceSampleRate = juce::jmax (1.0, sourceRate.load());
        const double monitorSampleRate = juce::jmax (1.0, outputRate.load());
        const double baseRatio = sourceSampleRate / monitorSampleRate;
        const double fillError = static_cast<double> (ready - primeTarget)
                               / static_cast<double> (primeTarget);
        const double correctionPpm = juce::jlimit (-1200.0, 1200.0,
                                                    fillError * 450.0);
        const double adaptiveRatio = baseRatio * (1.0 + correctionPpm * 1.0e-6);
        currentDriftCorrectionPpm.store (correctionPpm, std::memory_order_relaxed);
        rateConversionActive.store (
            std::abs (baseRatio - 1.0) > 1.0e-6
                || std::abs (correctionPpm) > 1.0
                || outputChannels != 2
                || outputEncoding != SampleEncoding::float32,
            std::memory_order_relaxed);

        const int usableInput = juce::jmax (0, ready - interpolationGuardSamples);
        const int outputFromInput = static_cast<int> (
            std::floor (static_cast<double> (usableInput) / adaptiveRatio));
        const int framesToWrite = juce::jmin (
            static_cast<int> (availableFrames),
            juce::jmin (outputFromInput, resampleScratchCapacity));
        if (framesToWrite <= 0)
        {
            if (endpointStarted && padding == 0)
                underflowCount.fetch_add (1, std::memory_order_relaxed);
            return;
        }

        const int requiredInput = juce::jmin (
            resampleScratchCapacity,
            juce::jmax (interpolationGuardSamples,
                        static_cast<int> (std::ceil (
                            static_cast<double> (framesToWrite) * adaptiveRatio))
                            + interpolationGuardSamples));
        if (ready < requiredInput)
            return;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (requiredInput, start1, size1, start2, size2);
        const int staged = size1 + size2;
        if (staged < requiredInput)
        {
            fifo.finishedRead (staged);
            underflowCount.fetch_add (1, std::memory_order_relaxed);
            return;
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            if (size1 > 0)
                resampleInput.copyFrom (channel, 0, ringBuffer,
                                        channel, start1, size1);
            if (size2 > 0)
                resampleInput.copyFrom (channel, size1, ringBuffer,
                                        channel, start2, size2);
        }

        const int consumedLeft = leftInterpolator.process (
            adaptiveRatio, resampleInput.getReadPointer (0),
            resampleOutput.getWritePointer (0), framesToWrite);
        const int consumedRight = rightInterpolator.process (
            adaptiveRatio, resampleInput.getReadPointer (1),
            resampleOutput.getWritePointer (1), framesToWrite);
        const int consumed = juce::jlimit (
            0, staged, juce::jmin (consumedLeft, consumedRight));
        if (consumed <= 0)
            return;

        BYTE* endpointBuffer = nullptr;
        result = renderClient->GetBuffer (static_cast<UINT32> (framesToWrite),
                                          &endpointBuffer);
        if (FAILED (result))
        {
            markForReconnectLocked ("WASAPI IAudioRenderClient::GetBuffer", result);
            return;
        }

        gainLinear.setTargetValue (
            juce::Decibels::decibelsToGain (requestedGainDb));
        const float gainStart = gainLinear.getCurrentValue();
        const float gainEnd = gainLinear.skip (framesToWrite);
        DWORD releaseFlags = 0;
        if (muted)
            releaseFlags = AUDCLNT_BUFFERFLAGS_SILENT;
        else
            writeInterleaved (endpointBuffer, framesToWrite, gainStart, gainEnd);

        result = renderClient->ReleaseBuffer (static_cast<UINT32> (framesToWrite),
                                              releaseFlags);
        if (FAILED (result))
        {
            markForReconnectLocked ("WASAPI IAudioRenderClient::ReleaseBuffer", result);
            return;
        }

        fifo.finishedRead (consumed);
        endpointPaddingFrames.store (static_cast<int> (padding)
                                         + framesToWrite,
                                     std::memory_order_relaxed);

        if (! endpointStarted)
        {
            result = audioClient->Start();
            if (FAILED (result))
            {
                markForReconnectLocked ("WASAPI IAudioClient::Start", result);
                return;
            }
            endpointStarted = true;
            endpointStartedFlag.store (true, std::memory_order_release);
            primed.store (true, std::memory_order_release);
        }
    }

    SRWLOCK playbackLock = SRWLOCK_INIT;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioRenderClient> renderClient;
    std::wstring selectedDeviceId;
    juce::String selectedDisplayName;
    bool selectedDefault = false;
    bool endpointStarted = false;
    bool reconnectPending = false;
    double lastReconnectAttemptMs = 0.0;
    UINT32 endpointBufferFrameCount = 0;
    int outputChannels = 0;
    int outputBlockAlign = 0;
    int outputBitsPerSample = 0;
    int outputValidBits = 0;
    SampleEncoding outputEncoding = SampleEncoding::unsupported;
#endif

    juce::AudioBuffer<float> ringBuffer;
    juce::AbstractFifo fifo { fifoCapacitySamples };
    juce::AudioBuffer<float> resampleInput;
    juce::AudioBuffer<float> resampleOutput;
    juce::LagrangeInterpolator leftInterpolator;
    juce::LagrangeInterpolator rightInterpolator;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainLinear;

    std::atomic<double> sourceRate { 48000.0 };
    std::atomic<int> sourceBlockSize { 128 };
    std::atomic<double> outputRate { 0.0 };
    std::atomic<int> outputBlockSize { 0 };
    std::atomic<int> primeTargetSamples { 1440 };
    std::atomic<double> currentDriftCorrectionPpm { 0.0 };
    std::atomic<bool> rateConversionActive { false };
    std::atomic<bool> deviceOpen { false };
    std::atomic<bool> running { false };
    std::atomic<bool> primed { false };
    std::atomic<bool> endpointStartedFlag { false };
    std::atomic<int> endpointPaddingFrames { 0 };
    std::atomic<int> underflowCount { 0 };
    std::atomic<int> overflowCount { 0 };
    std::atomic<int> reconnectCount { 0 };

    mutable juce::CriticalSection statusLock;
    juce::String currentDeviceName;
    juce::String errorMessage;
};

MonitorOutputClient::MonitorOutputClient()
    : impl (std::make_unique<Impl>())
{
}

MonitorOutputClient::~MonitorOutputClient()
{
    shutdown();
}

bool MonitorOutputClient::initialise()
{
#if JUCE_WINDOWS
    if (! ensureComForCurrentThread())
    {
        impl->setError ("Windows COM is unavailable; WASAPI monitor cannot start.");
        return false;
    }

    impl->clearError();
    return true;
#else
    impl->setError ("Physical monitoring currently requires the Windows WASAPI backend.");
    return false;
#endif
}

void MonitorOutputClient::shutdown()
{
#if JUCE_WINDOWS
    AcquireSRWLockExclusive (&impl->playbackLock);
    impl->closeEndpointLocked (true);
    ReleaseSRWLockExclusive (&impl->playbackLock);
#else
    impl->resetTransportLocked();
#endif
}

void MonitorOutputClient::prepareSource (double sourceSampleRate,
                                         int sourceMaximumBlockSize)
{
    const double safeRate = juce::jmax (1.0, sourceSampleRate);
    const int safeBlock = juce::jmax (1, sourceMaximumBlockSize);
    impl->sourceRate.store (safeRate, std::memory_order_release);
    impl->sourceBlockSize.store (safeBlock, std::memory_order_release);
    impl->primeTargetSamples.store (juce::jlimit (
        safeBlock * 3,
        Impl::fifoCapacitySamples / 4,
        juce::roundToInt (safeRate * 0.030)),
        std::memory_order_release);
    flush();
}

void MonitorOutputClient::flush() noexcept
{
#if JUCE_WINDOWS
    AcquireSRWLockExclusive (&impl->playbackLock);
    if (impl->audioClient != nullptr && impl->endpointStarted)
    {
        impl->audioClient->Stop();
        impl->audioClient->Reset();
    }
    impl->resetTransportLocked();
    ReleaseSRWLockExclusive (&impl->playbackLock);
#else
    impl->resetTransportLocked();
#endif
}

juce::StringArray MonitorOutputClient::getOutputDeviceNames()
{
    juce::StringArray names;
#if JUCE_WINDOWS
    if (! ensureComForCurrentThread())
        return names;

    EndpointInfo defaultEndpoint;
    if (getDefaultEndpointInfo (defaultEndpoint)
        && ! isUnsafeVirtualOutput (defaultEndpoint.friendlyName))
        names.add (systemDefaultMonitorName);

    for (const auto& endpoint : enumerateActiveEndpoints())
        names.addIfNotAlreadyThere (endpoint.label);
#endif
    return names;
}

juce::String MonitorOutputClient::selectOutputDevice (const juce::String& name,
                                                       double preferredSampleRate)
{
    juce::ignoreUnused (preferredSampleRate);
    const auto requested = name.trim();

#if JUCE_WINDOWS
    if (! ensureComForCurrentThread())
        return "Windows COM is unavailable; WASAPI monitor cannot start.";

    AcquireSRWLockExclusive (&impl->playbackLock);
    if (requested.isEmpty())
    {
        impl->closeEndpointLocked (true);
        ReleaseSRWLockExclusive (&impl->playbackLock);
        return {};
    }

    EndpointInfo endpoint;
    bool useDefault = false;
    if (! resolveEndpoint (requested, endpoint, useDefault))
    {
        const juce::String error =
            "The selected Windows playback endpoint is no longer available.";
        impl->setError (error);
        ReleaseSRWLockExclusive (&impl->playbackLock);
        return error;
    }

    if (isUnsafeVirtualOutput (endpoint.friendlyName))
    {
        const juce::String error =
            "Monitor output must be a physical speaker or headphone endpoint, not VB-CABLE.";
        impl->setError (error);
        ReleaseSRWLockExclusive (&impl->playbackLock);
        return error;
    }

    impl->selectedDefault = useDefault;
    impl->selectedDeviceId = endpoint.id;
    impl->selectedDisplayName = useDefault ? juce::String (systemDefaultMonitorName)
                                           : endpoint.label;
    const bool opened = impl->openSelectedEndpointLocked (getGainDb(), false);
    const auto error = opened ? juce::String() : impl->getError();
    ReleaseSRWLockExclusive (&impl->playbackLock);
    return error;
#else
    juce::ignoreUnused (requested);
    return "Physical monitoring currently requires the Windows WASAPI backend.";
#endif
}

void MonitorOutputClient::push (const juce::AudioBuffer<float>& source,
                                int numSamples) noexcept
{
#if JUCE_WINDOWS
    if (source.getNumChannels() <= 0 || numSamples <= 0)
        return;

    if (! TryAcquireSRWLockExclusive (&impl->playbackLock))
    {
        impl->overflowCount.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    if (! ensureComForCurrentThread())
    {
        ReleaseSRWLockExclusive (&impl->playbackLock);
        return;
    }

    if (impl->audioClient == nullptr && impl->selectedDisplayName.isNotEmpty())
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (impl->lastReconnectAttemptMs <= 0.0
            || now - impl->lastReconnectAttemptMs >= 1000.0)
        {
            impl->lastReconnectAttemptMs = now;
            impl->openSelectedEndpointLocked (getGainDb(), true);
        }
    }

    if (impl->audioClient != nullptr)
    {
        impl->enqueueLocked (source, numSamples);
        impl->renderAvailableLocked (getGainDb(), isMuted());
    }

    ReleaseSRWLockExclusive (&impl->playbackLock);
#else
    juce::ignoreUnused (source, numSamples);
#endif
}

MonitorOutputClient::Status MonitorOutputClient::getStatus() const
{
    Status status;
    status.deviceOpen = impl->deviceOpen.load (std::memory_order_acquire);
    status.running = impl->running.load (std::memory_order_acquire);
    status.primed = impl->primed.load (std::memory_order_acquire);
    status.endpointStarted = impl->endpointStartedFlag.load (std::memory_order_acquire);
    status.resampling = impl->rateConversionActive.load (std::memory_order_acquire);
    status.sourceSampleRate = impl->sourceRate.load (std::memory_order_acquire);
    status.sampleRate = impl->outputRate.load (std::memory_order_acquire);
    status.driftCorrectionPpm = impl->currentDriftCorrectionPpm.load (std::memory_order_acquire);
    status.bufferSize = impl->outputBlockSize.load (std::memory_order_acquire);
    status.endpointPaddingFrames = impl->endpointPaddingFrames.load (std::memory_order_acquire);
    status.bufferedSamples = impl->fifo.getNumReady();
    status.underflows = impl->underflowCount.load (std::memory_order_acquire);
    status.overflows = impl->overflowCount.load (std::memory_order_acquire);
    status.reconnects = impl->reconnectCount.load (std::memory_order_acquire);
#if JUCE_WINDOWS
    status.backend = "WASAPI SHARED / OBS-STYLE";
#else
    status.backend = "UNAVAILABLE";
#endif
    const juce::ScopedLock lock (impl->statusLock);
    status.deviceName = impl->currentDeviceName;
    status.lastError = impl->errorMessage;
    return status;
}

bool MonitorOutputClient::isUnsafeVirtualOutput (const juce::String& name) noexcept
{
    return name.containsIgnoreCase ("VB-Audio")
        || name.containsIgnoreCase ("VB-CABLE")
        || name.containsIgnoreCase ("CABLE Input")
        || name.containsIgnoreCase ("CABLE Output");
}

} // namespace askp::live
