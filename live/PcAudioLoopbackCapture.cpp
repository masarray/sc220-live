#include "PcAudioLoopbackCapture.h"

#include <algorithm>
#include <cmath>
#include <vector>

#if JUCE_WINDOWS
 #include <Windows.h>
 #include <audioclient.h>
 #include <ksmedia.h>
 #include <mmdeviceapi.h>
 #include <wrl/client.h>
#endif

namespace askp::live {
namespace {
float peakToDb (float value) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (value, 1.0e-9f), -120.0f);
}

#if JUCE_WINDOWS
using Microsoft::WRL::ComPtr;

bool isFloatFormat (const WAVEFORMATEX& format) noexcept
{
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;

    if (format.wFormatTag != WAVE_FORMAT_EXTENSIBLE
        || format.cbSize < sizeof (WAVEFORMATEXTENSIBLE) - sizeof (WAVEFORMATEX))
        return false;

    const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&> (format);
    return extensible.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}

float readPcmSample (const BYTE* frame, int channel, const WAVEFORMATEX& format) noexcept
{
    const int bytesPerSample = juce::jmax (1, static_cast<int> (format.wBitsPerSample) / 8);
    const BYTE* sample = frame + channel * bytesPerSample;

    if (isFloatFormat (format) && format.wBitsPerSample == 32)
        return juce::jlimit (-1.0f, 1.0f, *reinterpret_cast<const float*> (sample));

    if (format.wBitsPerSample == 16)
        return static_cast<float> (*reinterpret_cast<const int16_t*> (sample)) / 32768.0f;

    if (format.wBitsPerSample == 24)
    {
        int32_t value = static_cast<int32_t> (sample[0])
                      | (static_cast<int32_t> (sample[1]) << 8)
                      | (static_cast<int32_t> (sample[2]) << 16);
        if ((value & 0x00800000) != 0)
            value |= static_cast<int32_t> (0xff000000);
        return static_cast<float> (value) / 8388608.0f;
    }

    if (format.wBitsPerSample == 32)
        return static_cast<float> (*reinterpret_cast<const int32_t*> (sample)) / 2147483648.0f;

    return 0.0f;
}
#endif
} // namespace

PcAudioLoopbackCapture::PcAudioLoopbackCapture()
    : juce::Thread ("SC220 Live PC Audio Loopback")
{
}

PcAudioLoopbackCapture::~PcAudioLoopbackCapture()
{
    stop();
}

void PcAudioLoopbackCapture::prepare (double targetSampleRate, int fifoDurationMs)
{
    stop();

    const auto safeRate = juce::jlimit (8000.0, 384000.0, targetSampleRate);
    const auto capacity = juce::jmax (4096,
        juce::roundToInt (safeRate * juce::jlimit (250, 4000, fifoDurationMs) / 1000.0));

    targetRate.store (safeRate);
    ringBuffer.setSize (2, capacity, false, true, false);
    ringBuffer.clear();
    fifo.setTotalSize (capacity);
    fifo.reset();
    captureSampleRate.store (0.0);
    peakL.store (-120.0f);
    peakR.store (-120.0f);
    underflowCount.store (0);
    overflowCount.store (0);
}

bool PcAudioLoopbackCapture::start()
{
#if JUCE_WINDOWS
    if (isThreadRunning())
        return true;

    setError ({});
    return startThread (juce::Thread::Priority::highest);
#else
    setError ("PC Audio loopback is only available on Windows.");
    return false;
#endif
}

void PcAudioLoopbackCapture::stop()
{
    signalThreadShouldExit();
    stopThread (2500);
    running.store (false);
    captureSampleRate.store (0.0);
}

int PcAudioLoopbackCapture::read (juce::AudioBuffer<float>& destination,
                                  int numSamples) noexcept
{
    if (destination.getNumChannels() < 2 || numSamples <= 0)
        return 0;

    destination.clear (0, 0, numSamples);
    destination.clear (1, 0, numSamples);

    const int available = juce::jmin (numSamples, fifo.getNumReady());
    if (available <= 0)
    {
        ++underflowCount;
        return 0;
    }

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (available, start1, size1, start2, size2);

    for (int channel = 0; channel < 2; ++channel)
    {
        if (size1 > 0)
            destination.copyFrom (channel, 0, ringBuffer, channel, start1, size1);
        if (size2 > 0)
            destination.copyFrom (channel, size1, ringBuffer, channel, start2, size2);
    }

    fifo.finishedRead (size1 + size2);
    if (available < numSamples)
        ++underflowCount;
    return available;
}

juce::String PcAudioLoopbackCapture::lastError() const
{
    const juce::ScopedLock lock (errorLock);
    return errorMessage;
}

void PcAudioLoopbackCapture::setError (juce::String message)
{
    const juce::ScopedLock lock (errorLock);
    errorMessage = std::move (message);
}

void PcAudioLoopbackCapture::push (const float* left, const float* right,
                                   int numSamples) noexcept
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    const int writable = juce::jmin (numSamples, fifo.getFreeSpace());
    if (writable < numSamples)
        ++overflowCount;
    if (writable <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (writable, start1, size1, start2, size2);

    if (size1 > 0)
    {
        ringBuffer.copyFrom (0, start1, left, size1);
        ringBuffer.copyFrom (1, start1, right, size1);
    }
    if (size2 > 0)
    {
        ringBuffer.copyFrom (0, start2, left + size1, size2);
        ringBuffer.copyFrom (1, start2, right + size1, size2);
    }
    fifo.finishedWrite (size1 + size2);

    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int i = 0; i < writable; ++i)
    {
        leftPeak = juce::jmax (leftPeak, std::abs (left[i]));
        rightPeak = juce::jmax (rightPeak, std::abs (right[i]));
    }
    peakL.store (juce::jmax (peakToDb (leftPeak), peakL.load() - 0.8f));
    peakR.store (juce::jmax (peakToDb (rightPeak), peakR.load() - 0.8f));
}

void PcAudioLoopbackCapture::run()
{
#if JUCE_WINDOWS
    const HRESULT comResult = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialise = SUCCEEDED (comResult);
    if (FAILED (comResult) && comResult != RPC_E_CHANGED_MODE)
    {
        setError ("Unable to initialise Windows audio COM services.");
        return;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> endpoint;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioCaptureClient> capture;
    WAVEFORMATEX* format = nullptr;
    HANDLE eventHandle = nullptr;

    auto fail = [this] (const juce::String& action, HRESULT result)
    {
        setError (action + " (HRESULT 0x" + juce::String::toHexString (static_cast<int> (result)) + ")");
    };

    HRESULT hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS (enumerator.GetAddressOf()));
    if (SUCCEEDED (hr))
        hr = enumerator->GetDefaultAudioEndpoint (eRender, eConsole, endpoint.GetAddressOf());
    if (SUCCEEDED (hr))
        hr = endpoint->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**> (client.GetAddressOf()));
    if (SUCCEEDED (hr))
        hr = client->GetMixFormat (&format);

    if (FAILED (hr) || format == nullptr)
    {
        fail ("Unable to open the Windows System Mix endpoint", hr);
        if (format != nullptr) CoTaskMemFree (format);
        if (shouldUninitialise) CoUninitialize();
        return;
    }

    const DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK
                      | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                      | AUDCLNT_STREAMFLAGS_NOPERSIST;
    hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, format, nullptr);
    if (SUCCEEDED (hr))
    {
        eventHandle = CreateEventW (nullptr, FALSE, FALSE, nullptr);
        if (eventHandle == nullptr)
            hr = HRESULT_FROM_WIN32 (GetLastError());
    }
    if (SUCCEEDED (hr))
        hr = client->SetEventHandle (eventHandle);
    if (SUCCEEDED (hr))
        hr = client->GetService (IID_PPV_ARGS (capture.GetAddressOf()));
    if (SUCCEEDED (hr))
        hr = client->Start();

    if (FAILED (hr))
    {
        fail ("Unable to start WASAPI loopback capture", hr);
        if (eventHandle != nullptr) CloseHandle (eventHandle);
        CoTaskMemFree (format);
        if (shouldUninitialise) CoUninitialize();
        return;
    }

    const int channels = juce::jmax (1, static_cast<int> (format->nChannels));
    const int frameBytes = juce::jmax (1, static_cast<int> (format->nBlockAlign));
    const double sourceRate = static_cast<double> (format->nSamplesPerSec);
    const double destinationRate = targetRate.load();
    captureSampleRate.store (sourceRate);
    running.store (true);
    setError ({});

    juce::LagrangeInterpolator leftResampler;
    juce::LagrangeInterpolator rightResampler;
    std::vector<float> inputLeft;
    std::vector<float> inputRight;
    std::vector<float> outputLeft;
    std::vector<float> outputRight;

    while (! threadShouldExit())
    {
        const DWORD waitResult = WaitForSingleObject (eventHandle, 100);
        if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_TIMEOUT)
            break;

        UINT32 packetFrames = 0;
        while (! threadShouldExit()
               && SUCCEEDED (capture->GetNextPacketSize (&packetFrames))
               && packetFrames > 0)
        {
            BYTE* data = nullptr;
            DWORD packetFlags = 0;
            UINT32 frames = 0;
            hr = capture->GetBuffer (&data, &frames, &packetFlags, nullptr, nullptr);
            if (FAILED (hr))
                break;

            inputLeft.resize (frames);
            inputRight.resize (frames);
            const bool silent = (packetFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr;
            for (UINT32 frame = 0; frame < frames; ++frame)
            {
                if (silent)
                {
                    inputLeft[frame] = 0.0f;
                    inputRight[frame] = 0.0f;
                    continue;
                }

                const BYTE* frameData = data + static_cast<size_t> (frame) * frameBytes;
                inputLeft[frame] = readPcmSample (frameData, 0, *format);
                inputRight[frame] = readPcmSample (frameData, channels > 1 ? 1 : 0, *format);
            }

            if (std::abs (sourceRate - destinationRate) < 0.5)
            {
                push (inputLeft.data(), inputRight.data(), static_cast<int> (frames));
            }
            else
            {
                const double speedRatio = sourceRate / destinationRate;
                const int outputFrames = juce::jmax (1,
                    static_cast<int> (std::floor (static_cast<double> (frames) / speedRatio)));
                outputLeft.resize (static_cast<size_t> (outputFrames));
                outputRight.resize (static_cast<size_t> (outputFrames));
                leftResampler.process (speedRatio, inputLeft.data(), outputLeft.data(), outputFrames);
                rightResampler.process (speedRatio, inputRight.data(), outputRight.data(), outputFrames);
                push (outputLeft.data(), outputRight.data(), outputFrames);
            }

            capture->ReleaseBuffer (frames);
            packetFrames = 0;
        }
    }

    client->Stop();
    running.store (false);
    if (eventHandle != nullptr) CloseHandle (eventHandle);
    CoTaskMemFree (format);
    if (shouldUninitialise) CoUninitialize();
#endif
}

} // namespace askp::live
