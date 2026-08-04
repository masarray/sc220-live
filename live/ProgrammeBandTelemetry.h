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
// Fourteen proportional-bandwidth biquads measure stereo power. A calibrated
// 180 ms envelope publishes the stable pink-flat band level. A second fast
// envelope publishes only display activity, allowing musical attacks to feel
// alive without biasing steady-state frequency balance or changing audio.
// There is no copied sample frame, FFT, lock, allocation, or lifecycle switch.
class ProgrammeBandTelemetry final
{
public:
    static constexpr std::size_t bandCount = 14;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = std::max (8000.0, newSampleRate);
        constexpr double pi = 3.1415926535897932384626433832795;

        for (std::size_t band = 0; band < bandCount; ++band)
        {
            const double nominal = static_cast<double> (centreFrequencies[band]);
            const double frequency = std::min (nominal, sampleRate * 0.42);

            const double lowerBoundary = band == 0
                ? nominal / std::sqrt (static_cast<double> (centreFrequencies[1]) / nominal)
                : std::sqrt (static_cast<double> (centreFrequencies[band - 1]) * nominal);
            const double upperBoundary = band + 1 == bandCount
                ? nominal * std::sqrt (nominal / static_cast<double> (centreFrequencies[band - 1]))
                : std::sqrt (nominal * static_cast<double> (centreFrequencies[band + 1]));
            const double bandwidth = std::max (1.0, upperBoundary - lowerBoundary);
            const double q = std::clamp (nominal / bandwidth, 0.55, 8.0);

            const double omega = 2.0 * pi * frequency / sampleRate;
            const double alpha = std::sin (omega) / (2.0 * q);
            const double a0 = 1.0 + alpha;

            coefficients[band].b0 = static_cast<float> ( alpha / a0);
            coefficients[band].b1 = 0.0f;
            coefficients[band].b2 = static_cast<float> (-alpha / a0);
            coefficients[band].a1 = static_cast<float> (-2.0 * std::cos (omega) / a0);
            coefficients[band].a2 = static_cast<float> ((1.0 - alpha) / a0);
        }

        calculatePinkNoiseCalibration();
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : states)
            for (auto& state : channel)
                state = {};

        calibratedPower.fill (0.0);
        fastPower.fill (0.0);
        powerInitialised = false;
        for (auto& value : publishedDb)
            value.store (-120.0f, std::memory_order_relaxed);
        for (auto& value : publishedActivity)
            value.store (0.0f, std::memory_order_relaxed);

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
        const double calibratedCoefficient = std::exp (
            -static_cast<double> (samples) / (sampleRate * calibratedSmoothingSeconds));

        for (std::size_t band = 0; band < bandCount; ++band)
        {
            const double blockPower = energy[band] * inverseSamples;
            const double position = static_cast<double> (band)
                                  / static_cast<double> (bandCount - 1);

            // Fast activity follows the musical role of each region: bass keeps
            // a little weight; upper percussion reacts and releases faster.
            const double fastAttackSeconds = 0.012 - 0.006 * position;
            const double fastReleaseSeconds = 0.110 - 0.055 * position;
            const double fastSeconds = blockPower > fastPower[band]
                ? fastAttackSeconds : fastReleaseSeconds;
            const double fastCoefficient = std::exp (
                -static_cast<double> (samples) / (sampleRate * fastSeconds));

            if (! powerInitialised)
            {
                calibratedPower[band] = blockPower;
                fastPower[band] = blockPower;
            }
            else
            {
                calibratedPower[band] = calibratedCoefficient * calibratedPower[band]
                                      + (1.0 - calibratedCoefficient) * blockPower;
                fastPower[band] = fastCoefficient * fastPower[band]
                                + (1.0 - fastCoefficient) * blockPower;
            }

            const double calibratedDb = 10.0 * std::log10 (
                std::max (calibratedPower[band], 1.0e-18));
            const double fastDb = 10.0 * std::log10 (
                std::max (fastPower[band], 1.0e-18));
            const float displayDb = static_cast<float> (calibratedDb)
                                  + pinkCalibrationDb[band];

            // Ignore ordinary stationary-noise ripple. Only a clear onset above
            // the calibrated envelope becomes visual lift/pulse information.
            const float activity = static_cast<float> (std::clamp (
                (fastDb - calibratedDb - activityThresholdDb) / activityRangeDb,
                0.0, 1.0));

            publishedDb[band].store (displayDb, std::memory_order_relaxed);
            publishedActivity[band].store (activity, std::memory_order_relaxed);
        }
        powerInitialised = true;
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

        generation = publishedGeneration.load (std::memory_order_acquire);
        return generation != 0;
    }

    void readActivity (std::array<float, bandCount>& destination) const noexcept
    {
        for (std::size_t band = 0; band < bandCount; ++band)
            destination[band] = publishedActivity[band].load (std::memory_order_relaxed);
    }

    std::uint64_t generation() const noexcept
    {
        return publishedGeneration.load (std::memory_order_acquire);
    }

    static constexpr const std::array<float, bandCount>& frequencies() noexcept
    {
        return centreFrequencies;
    }

    const std::array<float, bandCount>& pinkCalibrationOffsetsDb() const noexcept
    {
        return pinkCalibrationDb;
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

    static double responsePower (const Coefficients& coefficient,
                                 double omega) noexcept
    {
        const double cosine1 = std::cos (omega);
        const double sine1 = std::sin (omega);
        const double cosine2 = std::cos (2.0 * omega);
        const double sine2 = std::sin (2.0 * omega);

        const double numeratorReal = coefficient.b0
                                   + coefficient.b1 * cosine1
                                   + coefficient.b2 * cosine2;
        const double numeratorImag = -coefficient.b1 * sine1
                                   - coefficient.b2 * sine2;
        const double denominatorReal = 1.0
                                     + coefficient.a1 * cosine1
                                     + coefficient.a2 * cosine2;
        const double denominatorImag = -coefficient.a1 * sine1
                                     - coefficient.a2 * sine2;
        const double numerator = numeratorReal * numeratorReal
                               + numeratorImag * numeratorImag;
        const double denominator = denominatorReal * denominatorReal
                                 + denominatorImag * denominatorImag;
        return numerator / std::max (denominator, 1.0e-30);
    }

    void calculatePinkNoiseCalibration() noexcept
    {
        constexpr std::size_t integrationPoints = 4096;
        constexpr double pi = 3.1415926535897932384626433832795;
        const double minimumFrequency = 20.0;
        const double maximumFrequency = std::max (
            minimumFrequency * 2.0, std::min (20000.0, sampleRate * 0.49));
        const double logMinimum = std::log (minimumFrequency);
        const double logStep = (std::log (maximumFrequency) - logMinimum)
                             / static_cast<double> (integrationPoints);

        std::array<double, bandCount> responseDb {};
        double referenceDb = 0.0;
        for (std::size_t band = 0; band < bandCount; ++band)
        {
            double pinkWeightedPower = 0.0;
            for (std::size_t point = 0; point < integrationPoints; ++point)
            {
                // Equal steps in ln(f) are the correct integration measure for
                // pink noise because its power spectral density is proportional
                // to 1/f and df = f d(ln f).
                const double frequency = std::exp (
                    logMinimum + (static_cast<double> (point) + 0.5) * logStep);
                const double omega = 2.0 * pi * frequency / sampleRate;
                pinkWeightedPower += responsePower (coefficients[band], omega);
            }

            responseDb[band] = 10.0 * std::log10 (
                std::max (pinkWeightedPower, 1.0e-30));
            referenceDb += responseDb[band];
        }

        referenceDb /= static_cast<double> (bandCount);
        for (std::size_t band = 0; band < bandCount; ++band)
            pinkCalibrationDb[band] = static_cast<float> (std::clamp (
                referenceDb - responseDb[band], -12.0, 12.0));
    }

    inline static constexpr std::array<float, bandCount> centreFrequencies {{
        50.0f, 80.0f, 125.0f, 200.0f, 315.0f, 500.0f, 800.0f,
        1250.0f, 2000.0f, 3150.0f, 5000.0f, 8000.0f, 12000.0f, 16000.0f
    }};
    static constexpr double calibratedSmoothingSeconds = 0.180;
    static constexpr double activityThresholdDb = 1.5;
    static constexpr double activityRangeDb = 5.0;

    double sampleRate = 48000.0;
    std::array<Coefficients, bandCount> coefficients {};
    std::array<std::array<State, bandCount>, 2> states {};
    std::array<double, bandCount> calibratedPower {};
    std::array<double, bandCount> fastPower {};
    std::array<float, bandCount> pinkCalibrationDb {};
    bool powerInitialised = false;
    std::array<std::atomic<float>, bandCount> publishedDb {};
    std::array<std::atomic<float>, bandCount> publishedActivity {};
    std::atomic<std::uint64_t> publishedGeneration { 0 };
};

// C++17 inline variable: one process-wide analyser shared by generated engine
// and generated UI translation units.
inline ProgrammeBandTelemetry programmeBandTelemetry;

} // namespace askp::live
