#pragma once

#include <atomic>
#include <cstdint>

namespace askp::live {

// Small lock-free bridge from the audio callback to the UI. Integrated loudness
// remains part of LiveAudioEngine::Status; this bridge carries only the current
// 400 ms BS.1770 momentary value so the dial can react visibly in real time.
class ProgrammeLiveLoudnessTelemetry final
{
public:
    void reset() noexcept
    {
        momentaryLufs.store (-120.0f, std::memory_order_relaxed);
        publishedGeneration.store (0, std::memory_order_release);
    }

    void publish (float value) noexcept
    {
        momentaryLufs.store (value, std::memory_order_relaxed);
        publishedGeneration.fetch_add (1, std::memory_order_release);
    }

    bool read (float& value, std::uint64_t& generation) const noexcept
    {
        const auto current = publishedGeneration.load (std::memory_order_acquire);
        if (current == 0)
            return false;
        value = momentaryLufs.load (std::memory_order_relaxed);
        generation = current;
        return true;
    }

private:
    std::atomic<float> momentaryLufs { -120.0f };
    std::atomic<std::uint64_t> publishedGeneration { 0 };
};

inline ProgrammeLiveLoudnessTelemetry programmeLiveLoudnessTelemetry;

} // namespace askp::live
