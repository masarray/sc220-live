#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

namespace askp::live {

/**
    Lightweight profile-based denoiser for live music.

    Design goals:
      - user-learned static noise profile (RX-style workflow, original code);
      - short FFT, stereo-linked attenuation and conservative reduction;
      - fast transient release, slow reduction engagement;
      - no allocation or locking in the audio callback;
      - complementary smooth downward expander for silence cleanup.
*/
class MusicalNoiseClean
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int maxFftSize = 512;
    static constexpr int maxBins = maxFftSize / 2 + 1;

    enum class Quality
    {
        live256,
        clean512
    };

    MusicalNoiseClean();

    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset();

    void setQuality (Quality quality);
    Quality getQuality() const noexcept { return requestedQuality.load(); }

    void setEnabled (bool shouldEnable) noexcept { enabled.store (shouldEnable); }
    bool isEnabled() const noexcept { return enabled.load(); }

    void setReductionDb (float value) noexcept;
    void setPreserve (float value01) noexcept;
    void setThresholdOffsetDb (float value) noexcept;
    void setListenRemoved (bool shouldListen) noexcept { listenRemoved.store (shouldListen); }

    float getReductionDb() const noexcept { return reductionDb.load(); }
    float getPreserve() const noexcept { return preserve.load(); }
    bool isListeningRemoved() const noexcept { return listenRemoved.load(); }

    void startLearning (double seconds = 2.0) noexcept;
    bool isLearning() const noexcept { return learningActive.load(); }
    bool hasProfile() const noexcept { return profileValid.load(); }
    float getLearningProgress() const noexcept { return learningProgress.load(); }
    float getEstimatedNoiseFloorDb() const noexcept { return estimatedNoiseFloorDb.load(); }

    /** In-place spectral processing. Adds fftSize samples of deterministic latency. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept { return activeFftSize.load(); }

private:
    void reconfigureIfNeeded() noexcept;
    void beginLearningOnAudioThread() noexcept;
    void processFrame() noexcept;
    float frequencyWeight (int bin) const noexcept;
    float calculateLinkedGain (int bin, float linkedPower, float localTonality,
                               float transientProtect) noexcept;

    struct FixedFft
    {
        void configure (int newSize) noexcept;
        void forward (float* interleavedWork) const noexcept;
        void inverse (float* interleavedWork) const noexcept;

        int size = 256;
        int stages = 8;
        std::array<int, maxFftSize> reversed {};
        std::array<float, maxFftSize / 2> cosine {};
        std::array<float, maxFftSize / 2> sine {};
    };

    struct SubsonicFilter
    {
        void prepare (double sampleRate, float cutoffHz) noexcept;
        void reset() noexcept;
        float process (float input) noexcept;

        float coefficient = 0.0f;
        float previousInput1 = 0.0f;
        float previousOutput1 = 0.0f;
        float previousInput2 = 0.0f;
        float previousOutput2 = 0.0f;
    };

    FixedFft fixedFft;

    double sampleRate = 48000.0;
    int channelCount = 2;
    int fftSize = 256;
    int hopSize = 128;
    int inputWritePos = 0;
    int outputReadPos = 0;
    int hopCounter = 0;
    int64_t samplesSeen = 0;

    static constexpr int outputFifoSize = maxFftSize * 4;

    std::array<std::array<float, maxFftSize>, maxChannels> inputRing {};
    std::array<std::array<float, outputFifoSize>, maxChannels> outputRing {};
    std::array<std::array<float, outputFifoSize>, maxChannels> dryDelayRing {};
    std::array<std::array<float, maxFftSize * 2>, maxChannels> fftWork {};
    std::array<std::array<float, maxBins>, maxChannels> currentPower {};
    std::array<std::array<float, maxBins>, maxChannels> previousMagnitude {};
    std::array<std::array<float, maxBins>, maxChannels> profilePower {};
    std::array<std::array<double, maxBins>, maxChannels> profileAccumulator {};
    std::array<float, maxBins> linkedGainState {};
    std::array<float, maxFftSize> window {};
    std::array<SubsonicFilter, maxChannels> subsonicFilters {};

    std::atomic<Quality> requestedQuality { Quality::live256 };
    std::atomic<int> activeFftSize { 256 };
    std::atomic<bool> qualityDirty { false };
    std::atomic<bool> enabled { true };
    std::atomic<float> reductionDb { 5.0f };
    std::atomic<float> preserve { 0.75f };
    std::atomic<float> thresholdOffsetDb { 1.5f };
    std::atomic<bool> listenRemoved { false };

    std::atomic<bool> learnRequested { false };
    std::atomic<double> learnSecondsRequested { 2.0 };
    std::atomic<bool> learningActive { false };
    std::atomic<bool> profileValid { false };
    std::atomic<float> learningProgress { 0.0f };
    std::atomic<float> estimatedNoiseFloorDb { -90.0f };

    int learningFramesTarget = 1;
    int learningFramesCaptured = 0;
    double learningTimeDomainEnergy = 0.0;
    int64_t learningTimeDomainSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicalNoiseClean)
};

/** Smooth stereo-linked downward expander controlled by a pre-enhancer detector. */
class SmoothDownwardExpander
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled (bool value) noexcept { enabled.store (value); }
    void setAmount (float value01) noexcept;
    float getAmount() const noexcept { return amount.load(); }
    bool isEnabled() const noexcept { return enabled.load(); }
    void setNoiseFloorDb (float value) noexcept { learnedNoiseFloorDb.store (value); }
    void setThresholdOffsetDb (float value) noexcept { thresholdOffsetDb.store (value); }

    void process (juce::AudioBuffer<float>& target,
                  const juce::AudioBuffer<float>& detector) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    std::atomic<bool> enabled { true };
    std::atomic<float> amount { 0.55f };
    std::atomic<float> learnedNoiseFloorDb { -72.0f };
    std::atomic<float> thresholdOffsetDb { 6.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    double sampleRate = 48000.0;
    float detectorEnvelope = 0.0f;
    float gainDbState = 0.0f;
    int holdSamplesRemaining = 0;
    bool openState = true;
};

/** Final post-rack linked safety limiter. */
class LiveSafetyLimiter
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setCeilingDb (float value) noexcept { ceilingDb.store (juce::jlimit (-6.0f, -0.1f, value)); }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }

private:
    double sampleRate = 48000.0;
    float envelope = 0.0f;
    std::atomic<float> ceilingDb { -1.0f };
    std::atomic<float> gainReductionDb { 0.0f };
};

} // namespace askp::live
