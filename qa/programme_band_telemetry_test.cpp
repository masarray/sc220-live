#include "../live/ProgrammeBandTelemetry.h"
#include "../live/ProgrammeLoudnessIntegrator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

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

bool runRhythmResponseTest()
{
    askp::live::ProgrammeBandTelemetry telemetry;
    telemetry.prepare (sampleRate);

    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    for (int block = 0; block < 96; ++block)
        telemetry.process (left.data(), right.data(), blockSize);

    std::array<float, askp::live::ProgrammeBandTelemetry::bandCount> before {};
    std::uint64_t generation = 0;
    telemetry.read (before, generation);

    double phase = 0.0;
    const double step = 2.0 * pi * 80.0 / sampleRate;
    for (int block = 0; block < 12; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const float value = 0.75f * static_cast<float> (std::sin (phase));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
            left[static_cast<std::size_t> (sample)] = value;
            right[static_cast<std::size_t> (sample)] = value;
        }
        telemetry.process (left.data(), right.data(), blockSize);
    }

    std::array<float, askp::live::ProgrammeBandTelemetry::bandCount> attack {};
    telemetry.read (attack, generation);
    const auto attackBand = static_cast<std::size_t> (
        std::distance (attack.begin(), std::max_element (attack.begin(), attack.end())));

    left.fill (0.0f);
    right.fill (0.0f);
    for (int block = 0; block < 48; ++block)
        telemetry.process (left.data(), right.data(), blockSize);

    std::array<float, askp::live::ProgrammeBandTelemetry::bandCount> release {};
    telemetry.read (release, generation);

    const float attackRise = attack[1] - before[1];
    const float releaseFall = attack[1] - release[1];
    std::cout << "Rhythm 80 Hz attack rise: " << attackRise
              << " dB, release fall: " << releaseFall << " dB\n";

    return attackBand == 1 && attackRise > 30.0f && releaseFall > 1.5f;
}

bool runMovingLoudnessTest()
{
    askp::live::ProgrammeLoudnessIntegrator meter;
    meter.prepare (sampleRate);
    const auto quietEnergy = askp::live::ProgrammeLoudnessIntegrator::energyFromLufs (-30.0);
    const auto loudEnergy = askp::live::ProgrammeLoudnessIntegrator::energyFromLufs (-12.0);

    for (int frame = 0; frame < static_cast<int> (sampleRate * 0.6); ++frame)
        meter.processFrame (quietEnergy);
    const float quietMomentary = meter.momentaryLufs();

    for (int frame = 0; frame < static_cast<int> (sampleRate * 0.6); ++frame)
        meter.processFrame (loudEnergy);
    const float loudMomentary = meter.momentaryLufs();

    for (int frame = 0; frame < static_cast<int> (sampleRate * 2.4); ++frame)
        meter.processFrame (loudEnergy);
    const float integrated = meter.integratedLufs();

    std::cout << "Quiet LUFS-M: " << quietMomentary << "\n"
              << "Loud LUFS-M: " << loudMomentary << "\n"
              << "Integrated LUFS-I: " << integrated << "\n";

    return std::abs (quietMomentary + 30.0f) < 0.15f
        && std::abs (loudMomentary + 12.0f) < 0.15f
        && loudMomentary - quietMomentary > 17.5f
        && meter.ready() && integrated > -15.0f && integrated < -11.0f;
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
    const bool rhythmResponse = runRhythmResponseTest();
    const bool movingLoudness = runMovingLoudnessTest();

    std::cout << "80 Hz band: " << bass << "\n"
              << "800 Hz band: " << mid << "\n"
              << "8 kHz band: " << treble << "\n"
              << "1.25 kHz anti-phase band: " << antiPhase << "\n"
              << "Pink-noise max/min spread: " << pinkSpreadDb << " dB\n"
              << "Rhythm response: " << (rhythmResponse ? "PASS" : "FAIL") << "\n"
              << "Moving loudness: " << (movingLoudness ? "PASS" : "FAIL") << "\n";

    const bool passed = bass == 1 && mid == 6 && treble == 11
                     && antiPhase == 7 && pinkSpreadDb <= 0.75f
                     && rhythmResponse && movingLoudness;
    return passed ? 0 : 1;
}
