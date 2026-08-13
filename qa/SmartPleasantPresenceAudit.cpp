#include "SmartPleasantPresence.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <iostream>

namespace {
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 128;
constexpr int renderSamples = 48000;

struct RenderResult
{
    float gainDb = 0.0f;
    float channelMismatchDb = 0.0f;
    float midHighReductionDb = 0.0f;
    float deEssReductionDb = 0.0f;
    float peak = 0.0f;
};

RenderResult renderTone (double frequency, float amplitude, bool antiPhase)
{
    askp::live::SmartPleasantPresence processor;
    processor.prepare (sampleRate, blockSize, 2);
    processor.setStrength (1.0f);

    double inputEnergy = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    int measuredSamples = 0;
    float peak = 0.0f;

    juce::AudioBuffer<float> block (2, blockSize);
    for (int offset = 0; offset < renderSamples; offset += blockSize)
    {
        const int samples = juce::jmin (blockSize, renderSamples - offset);
        block.setSize (2, samples, false, false, true);
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto absoluteSample = offset + sample;
            const float value = amplitude * static_cast<float> (std::sin (
                juce::MathConstants<double>::twoPi * frequency
                * static_cast<double> (absoluteSample) / sampleRate));
            block.setSample (0, sample, value);
            block.setSample (1, sample, antiPhase ? -value : value);
        }

        processor.process (block);
        if (offset >= renderSamples / 2)
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                const float dry = amplitude * static_cast<float> (std::sin (
                    juce::MathConstants<double>::twoPi * frequency
                    * static_cast<double> (offset + sample) / sampleRate));
                const float left = block.getSample (0, sample);
                const float right = block.getSample (1, sample);
                inputEnergy += static_cast<double> (dry) * dry;
                leftEnergy += static_cast<double> (left) * left;
                rightEnergy += static_cast<double> (right) * right;
                peak = juce::jmax (peak, std::abs (left), std::abs (right));
                ++measuredSamples;
            }
        }
    }

    const float inputRms = static_cast<float> (std::sqrt (inputEnergy / measuredSamples));
    const float leftRms = static_cast<float> (std::sqrt (leftEnergy / measuredSamples));
    const float rightRms = static_cast<float> (std::sqrt (rightEnergy / measuredSamples));
    const auto snapshot = processor.snapshot();
    return {
        juce::Decibels::gainToDecibels (leftRms / inputRms),
        std::abs (juce::Decibels::gainToDecibels (
            juce::jmax (leftRms, 1.0e-9f) / juce::jmax (rightRms, 1.0e-9f))),
        snapshot.midHighReductionDb,
        snapshot.deEssReductionDb,
        peak
    };
}
} // namespace

int main()
{
    const auto body = renderTone (1000.0, 0.28f, false);
    const auto presence = renderTone (2200.0, 0.28f, false);
    const auto midHigh = renderTone (3900.0, 0.28f, false);
    const auto sibilance = renderTone (7350.0, 0.28f, false);
    const auto air = renderTone (12000.0, 0.28f, false);
    const auto stereo = renderTone (7350.0, 0.28f, true);

    bool ok = true;
    const auto require = [&ok] (bool condition, const char* message)
    {
        std::cout << (condition ? "[ PASS ] " : "[ FAIL ] ") << message << '\n';
        ok = ok && condition;
    };

    require (body.gainDb > -0.45f,
             "1 kHz vocal body remains essentially unchanged");
    require (presence.gainDb > -0.65f,
             "2.2 kHz vocal presence remains forward and polished");
    require (midHigh.gainDb < -0.75f && midHigh.gainDb > -3.2f,
             "3.9 kHz cing/cik resonance is restrained but not dulled");
    require (sibilance.gainDb < -1.35f && sibilance.gainDb > -5.2f,
             "7.35 kHz sibilance receives stronger bounded control");
    require (air.gainDb > -0.75f,
             "12 kHz air remains open rather than globally darkened");
    require (midHigh.midHighReductionDb > 0.7f
             && midHigh.midHighReductionDb <= 1.71f,
             "mid-high detector publishes bounded dynamic reduction");
    require (sibilance.deEssReductionDb > 1.2f
             && sibilance.deEssReductionDb <= 4.26f,
             "de-esser publishes bounded dynamic reduction");
    require (stereo.channelMismatchDb < 0.02f,
             "stereo-linked control preserves anti-phase channel balance");
    require (body.peak < 0.34f && presence.peak < 0.34f
             && midHigh.peak < 0.34f && sibilance.peak < 0.34f
             && air.peak < 0.34f,
             "dynamic subtraction does not create excessive overshoot");

    std::cout << "Body=" << body.gainDb
              << " dB  Presence=" << presence.gainDb
              << " dB  MidHigh=" << midHigh.gainDb
              << " dB  Sibilance=" << sibilance.gainDb
              << " dB  Air=" << air.gainDb
              << " dB  StereoMismatch=" << stereo.channelMismatchDb << " dB\n";
    return ok ? 0 : 1;
}
