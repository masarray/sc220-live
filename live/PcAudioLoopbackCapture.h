#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>

namespace askp::live {

// Captures the Windows default render endpoint through WASAPI loopback and
// exposes a realtime-safe stereo FIFO to the main audio graph. The COM/WASAPI
// client lives entirely on its own high-priority thread; the audio callback
// only reads already-converted float samples from the lock-free FIFO.
class PcAudioLoopbackCapture final : private juce::Thread
{
public:
    PcAudioLoopbackCapture();
    ~PcAudioLoopbackCapture() override;

    void prepare (double targetSampleRate, int fifoDurationMs = 1200);
    bool start();
    void stop();

    int read (juce::AudioBuffer<float>& destination, int numSamples) noexcept;

    bool isRunning() const noexcept { return running.load(); }
    double sourceSampleRate() const noexcept { return captureSampleRate.load(); }
    float peakDbL() const noexcept { return peakL.load(); }
    float peakDbR() const noexcept { return peakR.load(); }
    int underflows() const noexcept { return underflowCount.load(); }
    int overflows() const noexcept { return overflowCount.load(); }
    int bufferedSamples() const noexcept { return fifo.getNumReady(); }
    juce::String lastError() const;

private:
    void run() override;
    void setError (juce::String message);
    void push (const float* left, const float* right, int numSamples) noexcept;

    juce::AudioBuffer<float> ringBuffer;
    juce::AbstractFifo fifo { 1 };
    std::atomic<double> targetRate { 48000.0 };
    std::atomic<double> captureSampleRate { 0.0 };
    std::atomic<bool> running { false };
    std::atomic<float> peakL { -120.0f };
    std::atomic<float> peakR { -120.0f };
    std::atomic<int> underflowCount { 0 };
    std::atomic<int> overflowCount { 0 };

    mutable juce::CriticalSection errorLock;
    juce::String errorMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PcAudioLoopbackCapture)
};

} // namespace askp::live
