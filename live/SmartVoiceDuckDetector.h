#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace askp::live {

/**
    Allocation-free speech-presence detector for the PC-music ducking sidechain.

    Design goals:
      - reject keyboard clicks / taps and other short impulses;
      - react to sustained human speech rather than raw full-band peak level;
      - optionally reference the frozen SC220 learned-noise floor;
      - add no audio-path latency and no FFT/adaptive-learning workload.

    The detector is intentionally a control-only sidechain. It never modifies
    the SC220 programme samples.
*/
class SmartVoiceDuckDetector final
{
public:
    void prepare (double nextSampleRate) noexcept
    {
        sampleRate = std::max (8000.0, nextSampleRate);

        // One-pole speech-band sidechain: reject handling/desk rumble below
        // ~120 Hz and much of the keyboard/click energy above ~3.8 kHz.
        highPassPole = static_cast<float> (
            std::exp (-2.0 * 3.14159265358979323846 * 120.0 / sampleRate));
        lowPassFeed = static_cast<float> (
            1.0 - std::exp (-2.0 * 3.14159265358979323846 * 3800.0 / sampleRate));

        minimumSpeechSamples = static_cast<int> (std::lround (sampleRate * 0.045));
        holdSamples = static_cast<int> (std::lround (sampleRate * 0.300));
        reset();
    }

    void reset() noexcept
    {
        previousInput.fill (0.0f);
        highPassState.fill (0.0f);
        lowPassState.fill (0.0f);
        candidateSamples = 0;
        holdRemaining = 0;
        speaking = false;
    }

    /**
        Returns true only after speech-like energy has remained present long
        enough to reject isolated clicks. detector is the dry/pre-enhancer SC220
        sidechain, so Smart Enhancer coloration cannot create false triggers.
    */
    bool process (const juce::AudioBuffer<float>& detector,
                  int numSamples,
                  bool hasFrozenNoiseProfile,
                  float learnedNoiseFloorDb) noexcept
    {
        if (numSamples <= 0 || detector.getNumChannels() <= 0)
            return speaking;

        const int channels = std::min (2, detector.getNumChannels());
        double rawEnergy = 0.0;
        double speechEnergy = 0.0;
        float speechPeak = 0.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                const float input = detector.getSample (channel, sample);

                // DC-safe first-order HPF.
                const float hp = highPassPole
                    * (highPassState[static_cast<std::size_t> (channel)]
                       + input - previousInput[static_cast<std::size_t> (channel)]);
                previousInput[static_cast<std::size_t> (channel)] = input;
                highPassState[static_cast<std::size_t> (channel)] = hp;

                // First-order LPF completes the speech-focused bandpass.
                auto& lp = lowPassState[static_cast<std::size_t> (channel)];
                lp += lowPassFeed * (hp - lp);

                rawEnergy += static_cast<double> (input) * static_cast<double> (input);
                speechEnergy += static_cast<double> (lp) * static_cast<double> (lp);
                speechPeak = std::max (speechPeak, std::abs (lp));
            }
        }

        const double frames = static_cast<double> (std::max (1, numSamples * channels));
        const float rawRms = static_cast<float> (std::sqrt (rawEnergy / frames));
        const float speechRms = static_cast<float> (std::sqrt (speechEnergy / frames));
        const float rawDb = gainToDb (rawRms);
        const float speechDb = gainToDb (speechRms);
        const float peakDb = gainToDb (speechPeak);
        const float crestDb = peakDb - speechDb;
        const float speechBandRatioDb = speechDb - rawDb;

        // Frozen profile is only a threshold reference. It is never updated here.
        const float openThresholdDb = hasFrozenNoiseProfile
            ? std::clamp (learnedNoiseFloorDb + 16.0f, -46.0f, -30.0f)
            : -40.0f;
        const float closeThresholdDb = openThresholdDb - 5.0f;

        // Sustained speech must have useful energy in the voice band. A very
        // high crest factor is usually a click/tap; strong speech gets an escape
        // hatch so plosives at the beginning of a word do not prevent opening.
        const bool aboveOpen = speechDb >= openThresholdDb;
        const bool speechBandDominant = speechBandRatioDb >= -10.0f;
        const bool impulseLike = crestDb > 18.0f && speechDb < openThresholdDb + 8.0f;
        const bool candidate = aboveOpen && speechBandDominant && ! impulseLike;

        if (! speaking)
        {
            if (candidate)
            {
                candidateSamples = std::min (candidateSamples + numSamples,
                                             minimumSpeechSamples);
                if (candidateSamples >= minimumSpeechSamples)
                {
                    speaking = true;
                    holdRemaining = holdSamples;
                }
            }
            else
            {
                // Require continuity: separate keyboard taps do not accumulate
                // confidence across the gaps between keystrokes.
                candidateSamples = 0;
            }
        }
        else
        {
            if (speechDb >= closeThresholdDb && speechBandDominant)
            {
                holdRemaining = holdSamples;
            }
            else if (holdRemaining > 0)
            {
                holdRemaining = std::max (0, holdRemaining - numSamples);
            }
            else
            {
                speaking = false;
                candidateSamples = 0;
            }
        }

        return speaking;
    }

private:
    static float gainToDb (float value) noexcept
    {
        return value > 1.0e-7f
            ? 20.0f * std::log10 (value)
            : -140.0f;
    }

    double sampleRate = 48000.0;
    float highPassPole = 0.984f;
    float lowPassFeed = 0.392f;
    std::array<float, 2> previousInput {};
    std::array<float, 2> highPassState {};
    std::array<float, 2> lowPassState {};
    int minimumSpeechSamples = 2160; // 45 ms @ 48 kHz
    int holdSamples = 14400;         // 300 ms @ 48 kHz
    int candidateSamples = 0;
    int holdRemaining = 0;
    bool speaking = false;
};

} // namespace askp::live
