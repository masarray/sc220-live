#include "SmartPleasantPresence.h"

#include <algorithm>
#include <cmath>

namespace askp::live {
namespace {
float coefficientForMilliseconds (double sampleRate, double milliseconds) noexcept
{
    return static_cast<float> (std::exp (
        -1.0 / (juce::jmax (1.0, sampleRate) * milliseconds * 0.001)));
}

float linearFromReductionDb (float reductionDb) noexcept
{
    return juce::Decibels::decibelsToGain (-juce::jmax (0.0f, reductionDb));
}
} // namespace

void SmartPleasantPresence::prepare (double sampleRate,
                                     int maximumBlockSize,
                                     int channels)
{
    preparedSampleRate = juce::jmax (8000.0, sampleRate);
    preparedChannels = juce::jlimit (1, maximumChannels, channels);

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = preparedSampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32> (juce::jmax (1, maximumBlockSize));
    monoSpec.numChannels = 1;

    for (auto& filter : midHighBands)
    {
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.setCutoffFrequency (3900.0f);
        filter.setResonance (1.25f);
        filter.prepare (monoSpec);
    }

    for (auto& filter : sibilanceBands)
    {
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.setCutoffFrequency (7350.0f);
        filter.setResonance (0.82f);
        filter.prepare (monoSpec);
    }

    broadAttack = coefficientForMilliseconds (preparedSampleRate, 2.5);
    broadRelease = coefficientForMilliseconds (preparedSampleRate, 90.0);
    midHighAttack = coefficientForMilliseconds (preparedSampleRate, 1.4);
    midHighRelease = coefficientForMilliseconds (preparedSampleRate, 62.0);
    sibilanceAttack = coefficientForMilliseconds (preparedSampleRate, 0.7);
    sibilanceRelease = coefficientForMilliseconds (preparedSampleRate, 48.0);
    reductionAttack = coefficientForMilliseconds (preparedSampleRate, 4.0);
    reductionRelease = coefficientForMilliseconds (preparedSampleRate, 105.0);
    reset();
}

void SmartPleasantPresence::reset() noexcept
{
    for (auto& filter : midHighBands) filter.reset();
    for (auto& filter : sibilanceBands) filter.reset();
    broadEnvelope = 0.0f;
    midHighEnvelope = 0.0f;
    sibilanceEnvelope = 0.0f;
    midHighReductionDb = 0.0f;
    deEssReductionDb = 0.0f;
    publishedMidHighReductionDb.store (0.0f, std::memory_order_relaxed);
    publishedDeEssReductionDb.store (0.0f, std::memory_order_relaxed);
}

float SmartPleasantPresence::envelopeStep (float current, float input,
                                           float attackCoefficient,
                                           float releaseCoefficient) noexcept
{
    const float coefficient = input > current ? attackCoefficient : releaseCoefficient;
    return input + coefficient * (current - input);
}

float SmartPleasantPresence::softGate (float valueDb,
                                       float closedDb,
                                       float openDb) noexcept
{
    const float position = juce::jlimit (0.0f, 1.0f,
        (valueDb - closedDb) / juce::jmax (0.001f, openDb - closedDb));
    return position * position * (3.0f - 2.0f * position);
}

void SmartPleasantPresence::process (juce::AudioBuffer<float>& audio) noexcept
{
    const int channels = juce::jmin (preparedChannels, audio.getNumChannels());
    const int samples = audio.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return;

    std::array<float*, maximumChannels> writePointers {};
    for (int channel = 0; channel < channels; ++channel)
        writePointers[static_cast<size_t> (channel)] = audio.getWritePointer (channel);

    const float activeStrength = strength.load (std::memory_order_relaxed);
    float blockMidHighMaximum = 0.0f;
    float blockDeEssMaximum = 0.0f;

    for (int sample = 0; sample < samples; ++sample)
    {
        std::array<float, maximumChannels> dry {};
        std::array<float, maximumChannels> midHigh {};
        std::array<float, maximumChannels> sibilance {};
        float broadPeak = 0.0f;
        float midHighPeak = 0.0f;
        float sibilancePeak = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<size_t> (channel);
            dry[index] = writePointers[index][sample];
            midHigh[index] = midHighBands[index].processSample (0, dry[index]);
            sibilance[index] = sibilanceBands[index].processSample (0, dry[index]);
            broadPeak = juce::jmax (broadPeak, std::abs (dry[index]));
            midHighPeak = juce::jmax (midHighPeak, std::abs (midHigh[index]));
            sibilancePeak = juce::jmax (sibilancePeak, std::abs (sibilance[index]));
        }

        broadEnvelope = envelopeStep (
            broadEnvelope, broadPeak, broadAttack, broadRelease);
        midHighEnvelope = envelopeStep (
            midHighEnvelope, midHighPeak, midHighAttack, midHighRelease);
        sibilanceEnvelope = envelopeStep (
            sibilanceEnvelope, sibilancePeak, sibilanceAttack, sibilanceRelease);

        const float broadDb = juce::Decibels::gainToDecibels (
            juce::jmax (broadEnvelope, 1.0e-7f), -140.0f);
        const float midHighDb = juce::Decibels::gainToDecibels (
            juce::jmax (midHighEnvelope, 1.0e-7f), -140.0f);
        const float sibilanceDb = juce::Decibels::gainToDecibels (
            juce::jmax (sibilanceEnvelope, 1.0e-7f), -140.0f);

        // Relative band dominance keeps the action level-independent. The
        // absolute gates prevent breaths/noise from being pulled forward, while
        // the ceilings keep speech articulate rather than lispy or dulled.
        const float midHighDominance = midHighDb - broadDb;
        const float sibilanceDominance = sibilanceDb - broadDb;
        const float midHighTarget = juce::jlimit (0.0f, 1.70f,
            activeStrength * softGate (midHighDb, -45.0f, -29.0f)
            * (juce::jmax (0.0f, midHighDominance + 9.2f) * 0.24f
               + juce::jmax (0.0f, midHighDb + 25.0f) * 0.055f));
        const float deEssTarget = juce::jlimit (0.0f, 4.25f,
            activeStrength * softGate (sibilanceDb, -47.0f, -31.0f)
            * (juce::jmax (0.0f, sibilanceDominance + 8.0f) * 0.34f
               + juce::jmax (0.0f, sibilanceDb + 27.0f) * 0.070f));

        midHighReductionDb = envelopeStep (
            midHighReductionDb, midHighTarget, reductionAttack, reductionRelease);
        deEssReductionDb = envelopeStep (
            deEssReductionDb, deEssTarget, reductionAttack, reductionRelease);
        blockMidHighMaximum = juce::jmax (blockMidHighMaximum, midHighReductionDb);
        blockDeEssMaximum = juce::jmax (blockDeEssMaximum, deEssReductionDb);

        const float midHighSubtraction = 1.0f - linearFromReductionDb (midHighReductionDb);
        const float deEssSubtraction = 1.0f - linearFromReductionDb (deEssReductionDb);
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<size_t> (channel);
            writePointers[index][sample] = dry[index]
                - midHighSubtraction * midHigh[index]
                - deEssSubtraction * sibilance[index];
        }
    }

    publishedMidHighReductionDb.store (blockMidHighMaximum, std::memory_order_relaxed);
    publishedDeEssReductionDb.store (blockDeEssMaximum, std::memory_order_relaxed);
}

} // namespace askp::live
