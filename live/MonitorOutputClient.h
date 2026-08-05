#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #include <Windows.h>
 #include <propkeydef.h>
 #include <propsys.h>
#endif

#include <atomic>
#include <memory>

namespace askp::live {

// Independent confidence-monitor output. On Windows this is deliberately not a
// second JUCE AudioDeviceManager: it follows OBS Studio's proven monitoring
// architecture and opens the selected render endpoint directly through WASAPI
// shared mode, uses that endpoint's native mix format, resamples the protected
// programme/cue bus, and submits frames to IAudioRenderClient.
class MonitorOutputClient final
{
public:
    struct Status
    {
        bool deviceOpen = false;
        bool running = false;
        bool primed = false;
        bool endpointStarted = false;
        bool resampling = false;
        double sourceSampleRate = 0.0;
        double sampleRate = 0.0;
        double driftCorrectionPpm = 0.0;
        int bufferSize = 0;
        int endpointPaddingFrames = 0;
        int bufferedSamples = 0;
        int underflows = 0;
        int overflows = 0;
        int reconnects = 0;
        juce::String backend;
        juce::String deviceName;
        juce::String lastError;
    };

    MonitorOutputClient();
    ~MonitorOutputClient();

    bool initialise();
    void shutdown();
    void prepareSource (double sourceSampleRate, int sourceMaximumBlockSize);
    void flush() noexcept;

    juce::StringArray getOutputDeviceNames();
    juce::String selectOutputDevice (const juce::String& name,
                                     double preferredSampleRate);

    // Called by the main broadcast callback after DSP. The implementation is
    // non-blocking on the steady-state path; if endpoint selection/reset owns
    // the native lock, this block is skipped instead of stalling broadcast.
    void push (const juce::AudioBuffer<float>& source, int numSamples) noexcept;

    void setGainDb (float value) noexcept
    {
        gainDb.store (juce::jlimit (-60.0f, 12.0f, value),
                      std::memory_order_release);
    }
    float getGainDb() const noexcept
    {
        return gainDb.load (std::memory_order_acquire);
    }

    void setMuted (bool value) noexcept
    {
        muted.store (value, std::memory_order_release);
    }
    bool isMuted() const noexcept
    {
        return muted.load (std::memory_order_acquire);
    }

    Status getStatus() const;
    static bool isUnsafeVirtualOutput (const juce::String& name) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    std::atomic<float> gainDb { 0.0f };
    std::atomic<bool> muted { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonitorOutputClient)
};

} // namespace askp::live
