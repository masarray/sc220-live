#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>

namespace askp::live {

// Independent physical playback client for confidence monitoring. The primary
// AudioDeviceManager remains dedicated to SC220 input -> VB-CABLE stream output.
// This client owns a second output-only device and consumes a bounded stereo
// FIFO written by the main broadcast callback.
class MonitorOutputClient final : private juce::AudioIODeviceCallback
{
public:
    struct Status
    {
        bool deviceOpen = false;
        bool running = false;
        bool primed = false;
        double sampleRate = 0.0;
        int bufferSize = 0;
        int bufferedSamples = 0;
        int underflows = 0;
        int overflows = 0;
        juce::String deviceName;
        juce::String lastError;
    };

    MonitorOutputClient();
    ~MonitorOutputClient() override;

    bool initialise();
    void shutdown();
    void prepareSource (double sourceSampleRate, int sourceMaximumBlockSize);
    void flush() noexcept;

    juce::StringArray getOutputDeviceNames();
    juce::String selectOutputDevice (const juce::String& name,
                                     double preferredSampleRate);

    void push (const juce::AudioBuffer<float>& source, int numSamples) noexcept;

    void setGainDb (float value) noexcept
    {
        gainDb.store (juce::jlimit (-60.0f, 12.0f, value));
    }
    float getGainDb() const noexcept { return gainDb.load(); }

    void setMuted (bool value) noexcept { muted.store (value); }
    bool isMuted() const noexcept { return muted.load(); }

    Status getStatus() const;
    static bool isUnsafeVirtualOutput (const juce::String& name) noexcept;

private:
    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;
    void setError (juce::String message);

    static constexpr int fifoCapacitySamples = 384000;

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> ringBuffer;
    juce::AbstractFifo fifo { fifoCapacitySamples };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainLinear;

    std::atomic<double> sourceRate { 48000.0 };
    std::atomic<int> sourceBlockSize { 128 };
    std::atomic<double> outputRate { 0.0 };
    std::atomic<int> outputBlockSize { 0 };
    std::atomic<int> primeTargetSamples { 1440 };
    std::atomic<bool> deviceOpen { false };
    std::atomic<bool> running { false };
    std::atomic<bool> primed { false };
    std::atomic<bool> callbackRegistered { false };
    std::atomic<float> gainDb { 0.0f };
    std::atomic<bool> muted { true };
    std::atomic<int> underflowCount { 0 };
    std::atomic<int> overflowCount { 0 };

    mutable juce::CriticalSection statusLock;
    juce::String currentDeviceName;
    juce::String errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorOutputClient)
};

} // namespace askp::live
