#pragma once

#include <atomic>

namespace askp::live {

// Small lock-free state shared by the generated SC220 engine and UI. It keeps
// dual-mono trims and live loudness telemetry out of the audio-device status
// lock and adds no allocation or callback locking.
struct SC220P3RuntimeState final
{
    std::atomic<float> dualMonoLeftFaderDb { 0.0f };
    std::atomic<float> dualMonoRightFaderDb { 0.0f };
    std::atomic<float> momentaryLufs { -120.0f };
};

inline SC220P3RuntimeState sc220P3RuntimeState;

} // namespace askp::live
