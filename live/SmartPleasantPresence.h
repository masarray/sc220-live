#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace askp::live {

// Zero-latency, stereo-linked dynamic control for the two upper-vocal regions
// that become tiring after enhancement. The lower detector catches compact
// "cing/cik" presence resonances; the upper detector catches true sibilance.
// Both lanes stay dormant when their band is proportionate to the programme,
// preserving air, articulation and transient contrast.
class SmartPleasantPresence final
{
public:
    struct Snapshot
    {
        float midHighReductionDb = 0.0f;
        float deEssReductionDb = 0.0f;
    };

    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& audio) noexcept;

    void setStrength (float normalised) noexcept
    {
        strength.store (juce::jlimit (0.0f, 1.0f, normalised),
                        std::memory_order_relaxed);
    }

    Snapshot snapshot() const noexcept
    {
        return {
            publishedMidHighReductionDb.load (std::memory_order_relaxed),
            publishedDeEssReductionDb.load (std::memory_order_relaxed)
        };
    }

private:
    static float envelopeStep (float current, float input,
                               float attackCoefficient,
                               float releaseCoefficient) noexcept;
    static float softGate (float valueDb, float closedDb, float openDb) noexcept;

    static constexpr int maximumChannels = 2;
    std::array<juce::dsp::StateVariableTPTFilter<float>, maximumChannels> midHighBands;
    std::array<juce::dsp::StateVariableTPTFilter<float>, maximumChannels> sibilanceBands;

    double preparedSampleRate = 48000.0;
    int preparedChannels = maximumChannels;
    float broadAttack = 0.0f;
    float broadRelease = 0.0f;
    float midHighAttack = 0.0f;
    float midHighRelease = 0.0f;
    float sibilanceAttack = 0.0f;
    float sibilanceRelease = 0.0f;
    float reductionAttack = 0.0f;
    float reductionRelease = 0.0f;

    float broadEnvelope = 0.0f;
    float midHighEnvelope = 0.0f;
    float sibilanceEnvelope = 0.0f;
    float midHighReductionDb = 0.0f;
    float deEssReductionDb = 0.0f;

    std::atomic<float> strength { 0.78f };
    std::atomic<float> publishedMidHighReductionDb { 0.0f };
    std::atomic<float> publishedDeEssReductionDb { 0.0f };
};

} // namespace askp::live
