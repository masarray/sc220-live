#include "../live/ProgrammeBandTelemetry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

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

    for (int block = 0; block < 96; ++block)
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
} // namespace

int main()
{
    askp::live::ProgrammeBandTelemetry telemetry;

    const auto bass = runTone (telemetry, 80.0, false);
    const auto mid = runTone (telemetry, 800.0, false);
    const auto treble = runTone (telemetry, 8000.0, false);
    const auto antiPhase = runTone (telemetry, 1250.0, true);

    std::cout << "80 Hz band: " << bass << "\n"
              << "800 Hz band: " << mid << "\n"
              << "8 kHz band: " << treble << "\n"
              << "1.25 kHz anti-phase band: " << antiPhase << "\n";

    const bool passed = bass == 1 && mid == 6 && treble == 11 && antiPhase == 7;
    return passed ? 0 : 1;
}
