#include "../live/ProgrammeBandTelemetry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;
constexpr double pi = 3.1415926535897932384626433832795;

std::size_t runTone (askp::live::ProgrammeBandTelemetry& telemetry,
                     double frequency, bool antiPhase)
{
    telemetry.prepare (sampleRate);
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    double phase = 0.0;
    const double phaseStep = 2.0 * pi * frequency / sampleRate;

    for (int block = 0; block < 192; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float value = static_cast<float> (std::sin (phase));
            phase += phaseStep;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
            left[static_cast<std::size_t> (sample)] = value;
            right[static_cast<std::size_t> (sample)] = antiPhase ? -value : value;
        }
        telemetry.process (left.data(), right.data(), blockSize);
    }

    std::array<float, askp::live::ProgrammeBandTelemetry::bandCount> levels {};
    std::uint64_t generation = 0;
    if (! telemetry.read (levels, generation) || generation == 0)
        return askp::live::ProgrammeBandTelemetry::bandCount;

    return static_cast<std::size_t> (
        std::distance (levels.begin(), std::max_element (levels.begin(), levels.end())));
}

struct Oscillator
{
    double sine = 0.0;
    double cosine = 1.0;
    double sineStep = 0.0;
    double cosineStep = 1.0;

    float next() noexcept
    {
        const float output = static_cast<float> (sine);
        const double nextSine = sine * cosineStep + cosine * sineStep;
        const double nextCosine = cosine * cosineStep - sine * sineStep;
        sine = nextSine;
        cosine = nextCosine;
        return output;
    }
};

float runPinkFlatness (askp::live::ProgrammeBandTelemetry& telemetry)
{
    telemetry.prepare (sampleRate);
    constexpr std::size_t oscillatorCount = 256;
    constexpr double minimumFrequency = 25.0;
    constexpr double maximumFrequency = 18000.0;
    std::array<Oscillator, oscillatorCount> oscillators {};

    for (std::size_t index = 0; index < oscillatorCount; ++index)
    {
        const double normal = (static_cast<double> (index) + 0.5)
                            / static_cast<double> (oscillatorCount);
        const double frequency = minimumFrequency
            * std::pow (maximumFrequency / minimumFrequency, normal);
        const double phase = 2.0 * pi
            * std::fmod (static_cast<double> (index) * 0.6180339887498949, 1.0);
        const double phaseStep = 2.0 * pi * frequency / sampleRate;
        oscillators[index].sine = std::sin (phase);
        oscillators[index].cosine = std::cos (phase);
        oscillators[index].sineStep = std::sin (phaseStep);
        oscillators[index].cosineStep = std::cos (phaseStep);
    }

    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    const float gain = 0.15f / std::sqrt (static_cast<float> (oscillatorCount));
    const int blocks = static_cast<int> (sampleRate * 5.0 / blockSize);
    for (int block = 0; block < blocks; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            float value = 0.0f;
            for (auto& oscillator : oscillators)
                value += oscillator.next();
            value *= gain;
            left[static_cast<std::size_t> (sample)] = value;
            right[static_cast<std::size_t> (sample)] = value;
        }
        telemetry.process (left.data(), right.data(), blockSize);
    }

    std::array<float, askp::live::ProgrammeBandTelemetry::bandCount> levels {};
    std::uint64_t generation = 0;
    if (! telemetry.read (levels, generation) || generation == 0)
        return std::numeric_limits<float>::infinity();

    const auto limits = std::minmax_element (levels.begin(), levels.end());
    std::cout << "Pink-calibrated bands (dB):";
    for (const auto value : levels)
        std::cout << ' ' << value;
    std::cout << "\n";
    return *limits.second - *limits.first;
}
} // namespace

int main()
{
    askp::live::ProgrammeBandTelemetry telemetry;

    const auto bass = runTone (telemetry, 80.0, false);
    const auto mid = runTone (telemetry, 800.0, false);
    const auto treble = runTone (telemetry, 8000.0, false);
    const auto antiPhase = runTone (telemetry, 1250.0, true);
    const auto pinkSpreadDb = runPinkFlatness (telemetry);

    std::cout << "80 Hz band: " << bass << "\n"
              << "800 Hz band: " << mid << "\n"
              << "8 kHz band: " << treble << "\n"
              << "1.25 kHz anti-phase band: " << antiPhase << "\n"
              << "Pink-noise max/min spread: " << pinkSpreadDb << " dB\n";

    const bool passed = bass == 1 && mid == 6 && treble == 11
                     && antiPhase == 7 && pinkSpreadDb <= 0.75f;
    return passed ? 0 : 1;
}
