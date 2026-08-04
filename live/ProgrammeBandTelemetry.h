#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace askp::live {

// Extremely small read-only programme analyser for the SC220 Live surface.
//
// The JUCE audio callback feeds the exact protected post-limiter stereo buffer.
// Fourteen fixed band-pass biquads measure block RMS and publish only atomic dB
// values. There is no audio queue, no copied sample frame, no FFT, no lock, no
// allocation, and no lifecycle enable/disable switch. The filters never alter
// programme audio; they only read it.
class ProgrammeBandTelemetry final
{
public:
    static constexpr std::size_t bandCount = 14;

    void prepare (double newSampleRate) noexcept
    {
        const double safeRate = std::max (8000.0, newSampleRate);
        constexpr double pi = 3.1415926535897932384626433832795;
        constexpr double q = 1.10;

        for (std::size_t band = 0; band < bandCount; ++band)
        {
            const double frequency = std::min (
                static_cast<double> (centreFrequencies[band]), safeRate * 0.42);
            const double omega = 2.0 * pi * frequency / safeRate;
            const double alpha = std::sin (omega) / (2.0 * q);
            const double a0 = 1.0 + alpha;

            coefficients[band].b0 = static_cast<float> ( alpha / a0);
            coefficients[band].b1 = 0.0f;
            coefficients[band].b2 = static_cast<float> (-alpha / a0);
            coefficients[band].a1 = static_cast<float> (-2.0 * std::cos (omega) / a0);
            coefficients[band].a2 = static_cast<float> ((1.0 - alpha) / a0);
        }

        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : states)
            for (auto& state : channel)
                state = {};

        for (auto& value : publishedDb)
            value.store (-120.0f, std::memory_order_relaxed);

        publishedGeneration.store (0, std::memory_order_release);
    }

    void process (const float* left, const float* right, int samples) noexcept
    {
        if (left == nullptr || right == nullptr || samples <= 0)
            return;

        std::array<double, bandCount> energy {};
        for (int sample = 0; sample < samples; ++sample)
        {
            const float inputLeft = left[sample];
            const float inputRight = right[sample];

            for (std::size_t band = 0; band < bandCount; ++band)
            {
                const float filteredLeft = processSample (
                    inputLeft, coefficients[band], states[0][band]);
                const float filteredRight = processSample (
                    inputRight, coefficients[band], states[1][band]);

                energy[band] += 0.5 * (
                    static_cast<double> (filteredLeft) * filteredLeft
                    + static_cast<double> (filteredRight) * filteredRight);
            }
        }

        const double inverseSamples = 1.0 / static_cast<double> (samples);
        for (std::size_t band = 0; band < bandCount; ++band)
        {
            const float rms = static_cast<float> (
                std::sqrt (energy[band] * inverseSamples));
            const float db = 20.0f * std::log10 (std::max (rms, 1.0e-9f));
            publishedDb[band].store (db, std::memory_order_relaxed);
        }

        publishedGeneration.fetch_add (1, std::memory_order_release);
    }

    bool read (std::array<float, bandCount>& destination,
               std::uint64_t& generation) const noexcept
    {
        const auto before = publishedGeneration.load (std::memory_order_acquire);
        if (before == 0)
            return false;

        for (std::size_t band = 0; band < bandCount; ++band)
            destination[band] = publishedDb[band].load (std::memory_order_relaxed);

        // Atomic band values make a concurrent block transition safe. Loading
        // generation again lets the UI display the newest callback generation.
        generation = publishedGeneration.load (std::memory_order_acquire);
        return generation != 0;
    }

    std::uint64_t generation() const noexcept
    {
        return publishedGeneration.load (std::memory_order_acquire);
    }

    static constexpr const std::array<float, bandCount>& frequencies() noexcept
    {
        return centreFrequencies;
    }

private:
    struct Coefficients
    {
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
    };

    struct State
    {
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;
    };

    static float processSample (float input, const Coefficients& coefficient,
                                State& state) noexcept
    {
        const float output = coefficient.b0 * input
                           + coefficient.b1 * state.x1
                           + coefficient.b2 * state.x2
                           - coefficient.a1 * state.y1
                           - coefficient.a2 * state.y2;
        state.x2 = state.x1;
        state.x1 = input;
        state.y2 = state.y1;
        state.y1 = output;
        return output;
    }

    inline static constexpr std::array<float, bandCount> centreFrequencies {{
        50.0f, 80.0f, 125.0f, 200.0f, 315.0f, 500.0f, 800.0f,
        1250.0f, 2000.0f, 3150.0f, 5000.0f, 8000.0f, 12000.0f, 16000.0f
    }};

    std::array<Coefficients, bandCount> coefficients {};
    std::array<std::array<State, bandCount>, 2> states {};
    std::array<std::atomic<float>, bandCount> publishedDb {};
    std::atomic<std::uint64_t> publishedGeneration { 0 };
};

// C++17 inline variable: one process-wide analyser shared by generated engine
// and generated UI translation units.
inline ProgrammeBandTelemetry programmeBandTelemetry;

} // namespace askp::live
