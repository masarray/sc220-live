#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <cstring>
#include <vector>

#include "arsonkupik_dsp.hpp"
#include "wow_headroom_guard.hpp"
#include "LicenseManager.h"

namespace askp {

// Lock-free stereo tap feeding the editor's liquid rhythm orb. Keeping both
// channels lets the UI place percussion and travelling ripples on their real
// sonic side; all band analysis remains on the message thread.
class ScopeTap {
public:
    // Covers one complete 24 Hz visual interval through 192 kHz, so a timer
    // scheduling wobble cannot discard the leading drum transient and retain
    // only a later vocal syllable from the same interval.
    static constexpr int kSize = 8192;

    void setEnabled(bool enabled) noexcept
    {
        enabled_.store(enabled, std::memory_order_release);
    }

    void push(const float* l, const float* r, int n) noexcept
    {
        if (!enabled_.load(std::memory_order_acquire) || l == nullptr || n <= 0)
            return;

        const float* right = r != nullptr ? r : l;
        int w = producerWrite_;
        const int read = readPublished_.load(std::memory_order_acquire);
        const int writable = (read - w - 1) & (kQueueSize - 1);
        const int count = juce::jmin(n, writable);
        if (count <= 0)
            return; // visual data may drop; the audio thread never waits

        const int first = juce::jmin(count, kQueueSize - w);
        std::memcpy(leftQueue_.data() + w, l, (size_t)first * sizeof(float));
        std::memcpy(rightQueue_.data() + w, right, (size_t)first * sizeof(float));
        const int second = count - first;
        if (second > 0) {
            std::memcpy(leftQueue_.data(), l + first, (size_t)second * sizeof(float));
            std::memcpy(rightQueue_.data(), right + first, (size_t)second * sizeof(float));
        }

        w = (w + count) & (kQueueSize - 1);
        producerWrite_ = w;
        writePublished_.store(w, std::memory_order_release);
    }

    // Consumes each captured sample once and returns the number copied. If a
    // high sample rate produces more than the visual frame can analyse, only
    // the newest fixed-size tail is retained. The producer is never blocked.
    int consumeLatest(float* leftDest, float* rightDest, int maxSamples) noexcept
    {
        maxSamples = juce::jlimit(0, kSize, maxSamples);
        if (leftDest == nullptr || rightDest == nullptr || maxSamples <= 0)
            return 0;

        int read = consumerRead_;
        const int published = writePublished_.load(std::memory_order_acquire);
        int available = (published - read) & (kQueueSize - 1);
        if (available > maxSamples) {
            read = (published - maxSamples) & (kQueueSize - 1);
            available = maxSamples;
        }

        const int first = juce::jmin(available, kQueueSize - read);
        if (first > 0) {
            std::memcpy(leftDest, leftQueue_.data() + read,
                        (size_t)first * sizeof(float));
            std::memcpy(rightDest, rightQueue_.data() + read,
                        (size_t)first * sizeof(float));
        }
        const int second = available - first;
        if (second > 0) {
            std::memcpy(leftDest + first, leftQueue_.data(),
                        (size_t)second * sizeof(float));
            std::memcpy(rightDest + first, rightQueue_.data(),
                        (size_t)second * sizeof(float));
        }

        consumerRead_ = published;
        readPublished_.store(published, std::memory_order_release);
        return available;
    }

private:
    static constexpr int kQueueSize = 16384; // power of two, one slot stays empty
    std::array<float, kQueueSize> leftQueue_{};
    std::array<float, kQueueSize> rightQueue_{};
    int producerWrite_ = 0;              // audio thread only
    int consumerRead_ = 0;               // message thread only
    std::atomic<int> writePublished_{0};
    std::atomic<int> readPublished_{0};
    std::atomic<bool> enabled_{false};
};

struct SharedMeters {
    std::atomic<float> inputPeakDb{-120.0f};
    std::atomic<float> outputPeakDb{-120.0f};
    std::atomic<float> inputPeakDbL{-120.0f};
    std::atomic<float> inputPeakDbR{-120.0f};
    std::atomic<float> outputPeakDbL{-120.0f};
    std::atomic<float> outputPeakDbR{-120.0f};
    std::atomic<float> gainReductionDb{0.0f};
    std::atomic<float> gainMatchDb{0.0f};
    std::atomic<bool>  gainMatchReady{false};
    std::atomic<float> correlation{1.0f};
    std::atomic<bool>  clipping{false};
};

class Processor : public juce::AudioProcessor,
                  private juce::AudioProcessorValueTreeState::Listener {
public:
    Processor();
    ~Processor() override;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "ArSonKuPik"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Shared with editor
    juce::AudioProcessorValueTreeState apvts;
    ScopeTap scopeTap;
    SharedMeters meters;
    LicenseManager licenseManager;

    // A/B compare (message thread). Slot 0 = A, slot 1 = B. Excludes the
    // utility INPUT/OUTPUT, Gain Match and BYPASS controls by design.
    int  getAbSlot() const noexcept { return abSlot_; }
    void selectAbSlot(int slot);
    void copyAbToOtherSlot();

    static const juce::StringArray& presetIds();
    static const juce::StringArray& presetNames();
    static juce::String presetDescription(int index);
    static juce::String presetCategory(int index);
    void applyPresetMacroToParameters(int index);
    void selectFactoryPreset(int index);
    int currentFactoryPresetIndex() const noexcept;
    juce::String currentPresetName() const;
    juce::String currentPresetCategory() const;
    juce::String currentPresetDescription() const;
    bool currentPresetIsModified() const noexcept
    {
        return presetModified_.load(std::memory_order_acquire);
    }
    bool currentPresetIsUser() const;

    struct UserPresetInfo {
        juce::String id;
        juce::String name;
        juce::String category;
        juce::String description;
    };
    std::vector<UserPresetInfo> userPresets() const;
    juce::Result loadUserPreset(const juce::String& id);
    juce::Result saveUserPresetAs(const juce::String& name);
    juce::Result saveCurrentUserPreset();
    juce::Result deleteCurrentUserPreset();

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void buildRuntimeParams(arsonkupik::RuntimeParams& out,
                            float enhance, float bass, float treble,
                            float vocal, float stereo, float trim,
                            bool smartProtect) const;
    void advanceDryDelay(juce::AudioBuffer<float>& buffer, std::size_t channels,
                         int numSamples, bool storeScratch, bool writeOutput);
    void primeWetDelayFromDry() noexcept;
    void resetLookaheadGain() noexcept;
    void resetDryReferenceGain() noexcept;
    void processLookaheadSafety(juce::AudioBuffer<float>& buffer,
                                std::size_t channels, int numSamples,
                                float wowGainStart, float wowGainEnd,
                                float finalGainStart, float finalGainEnd,
                                const std::array<float, 3>& matchGainStart,
                                const std::array<float, 3>& matchGainEnd,
                                std::array<double, 3>& preMatchPowerSum,
                                double& preMatchTotalPowerSum,
                                float& outputPeakL, float& outputPeakR) noexcept;
    void updateGainMatchController(const std::array<double, 3>& preMatchPowerSum,
                                   double preMatchTotalPowerSum,
                                   std::size_t channels, int numSamples,
                                   bool allowLearning) noexcept;
    void resetSignatureWow() noexcept;
    void resetGainMatchMeasurement(bool keepTarget) noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::File userPresetDirectory();
    juce::Result writeUserPreset(const juce::String& id,
                                 const juce::String& name,
                                 const juce::File& target);
    void clearActiveUserPreset();

    struct AbSnapshot {
        bool  valid = false;
        int   profile = 0;
        float enhance = 0, bass = 0, treble = 0, vocal = 0, stereo = 0, trim = 0;
        bool  trimLock = false, protect = true;
    };
    void captureAb(AbSnapshot& s) const;
    void applyAb(const AbSnapshot& s);
    int abSlot_ = 0;                 // message thread only
    AbSnapshot abSnaps_[2];          // message thread only

    // User-preset metadata is never read by processBlock. Project state embeds
    // all APVTS values, so a session remains portable even if a user preset
    // file is later renamed or removed.
    mutable juce::CriticalSection presetStateLock_;
    juce::String activeUserPresetId_;
    juce::String activeUserPresetName_;
    juce::String activeUserPresetCategory_;
    juce::String activeUserPresetDescription_;
    juce::File activeUserPresetFile_;
    std::atomic<bool> presetModified_{false};
    std::atomic<bool> presetRecallInProgress_{false};

    arsonkupik::ArSonKuPikEngine engine_;
    juce::AudioBuffer<float> warmupScratch_;
    juce::AudioBuffer<float> bypassDry_;
    juce::AudioBuffer<float> wetLookaheadDelay_;
    juce::AudioBuffer<float> dryLatencyDelay_;
    int realtimeScratchSamples_ = 0;
    // Creative controls retune EQ/color/width coefficients; utility Output
    // Trim only updates a scalar consumed by the engine. Keeping these paths
    // separate prevents Input/Final Output/Bypass changes from rebuilding the
    // creative rack.
    std::atomic<bool> creativeParamsDirty_{true};
    std::atomic<bool> utilityParamsDirty_{true};

    // --- Zipper-free control path (v0.1.1) ---------------------------------
    // Continuous knobs are low-passed (~80 ms). Creative coefficients are
    // retuned at a bounded 200 Hz while utility gains continue to update every
    // block. P7A owns one fixed Signature topology, so no standby renderer or
    // preset-transition allocation exists in the realtime path.
    using Smooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smooth smInput_, smEnhance_, smBass_, smTreble_, smVocal_, smStereo_, smTrim_, smFinalOut_, smBypass_;
    bool lastBypassTarget_ = false;
    static constexpr int kRealtimeScratchFloorSamples = 8192;
    static constexpr double kCreativeRetuneHz = 200.0;
    int creativeRetuneIntervalSamples_ = 1;
    int creativeRetuneCountdownSamples_ = 0;
    float lastEngineEnhance_ = 0.0f;
    float lastEngineBass_ = 0.0f;
    float lastEngineTreble_ = 0.0f;
    float lastEngineVocal_ = 0.0f;
    float lastEngineStereo_ = 0.0f;
    float lastEngineTrim_ = 0.0f;
    bool lastEngineSmartProtect_ = true;

    // --- P7G floor-only Signature level manager -----------------------------
    // One clean scalar protects only the minimum Active floor before final peak
    // safety. Exact loudness equality belongs exclusively to Gain Match ON.
    static constexpr float kSafeCeilingDb = -1.0f; // comp clamp reference
    float wowCompDb_ = 0.0f;            // audio thread only
    bool  wowCompPrimed_ = false;       // audio thread only
    float wowWetPeakEnv_ = 0.0f;        // audio thread only
    float wowDryPowerEnv_ = 0.0f;       // audio thread only
    float wowWetPowerEnv_ = 0.0f;       // audio thread only
    bool  wowMeasurePrimed_ = false;    // audio thread only
    int   wowFreezeSamples_ = 0;        // audio thread only
    int   wowLockMeasureSamples_ = 0;   // signal samples accumulated before lock
    float wowLockedDb_ = 0.0f;          // content-independent held WOW gain
    bool  wowGainLocked_ = false;       // prevents long-term loudness breathing
    arsonkupik::WowHeadroomGuard wowHeadroomGuard_; // P1 locked-WOW peak budget
    // P7G has one Signature identity. Its one-time floor result remains fixed
    // after lock, with no per-preset cache or handover.
    static constexpr double kLookaheadSeconds = 0.001;
    static constexpr float kOutputCeilingDb = -1.0f;
    int lookaheadSamples_ = 1;
    int lookaheadBufferSamples_ = 2;
    int wetDelayWrite_ = 0;
    int dryDelayWrite_ = 0;
    std::vector<float> lookaheadGainHistory_;
    int lookaheadGainWrite_ = 0;
    double lookaheadGainSum_ = 1.0;
    float lookaheadDetectorGain_ = 1.0f;
    float lookaheadGain_ = 1.0f;

    // P5.1 internal Reference Bypass uses the same -1 dBFS lookahead ceiling
    // as the active path, but remains independent of every creative and
    // utility control. The detector runs continuously so A/B toggles are
    // phase-aligned and click-free. Host/technical bypass is intentionally
    // untouched and remains the host's raw path.
    std::vector<float> dryReferenceGainHistory_;
    int dryReferenceGainWrite_ = 0;
    double dryReferenceGainSum_ = 1.0;
    float dryReferenceDetectorGain_ = 1.0f;
    float dryReferenceGain_ = 1.0f;
    int dryReferenceDetectorHold_ = 0;
    float dryReferenceGrDb_ = 0.0f;
    float dryReferenceInputPeakL_ = 0.0f;
    float dryReferenceInputPeakR_ = 0.0f;
    float dryReferenceOutputPeakL_ = 0.0f;
    float dryReferenceOutputPeakR_ = 0.0f;
    float lookaheadReleaseAlphaFast_ = 0.0f;
    float lookaheadReleaseAlphaSlow_ = 0.0f;
    int lookaheadDetectorHold_ = 0;
    float safetyGrDb_ = 0.0f;           // audio thread only
    float inputAppliedGain_ = 1.0f;      // block-ramped utility input gain
    float wowAppliedGain_ = 1.0f;        // block-ramped floor/safety gain
    float finalOutAppliedGain_ = 1.0f;   // block-ramped final output gain
    // First-load handover from the continuously latency-aligned RAW path to the
    // creative path. This is audio-thread state only and uses preallocated
    // bypass scratch, so cold filter/UI activation cannot create an edge.
    int startupFadeSamplesRemaining_ = 0;
    int startupFadeSamplesTotal_ = 1;

    // --- Loudness-only monitoring gain match -------------------------------
    // P6 measures full-band pre/post power and applies one attenuation value to
    // every complementary band. Equal band gains reconstruct a transparent
    // scalar level change, so the DSP's tone, separation and particles remain
    // audible while bypass comparisons stay close in loudness. Analysis is
    // fused into existing loops and allocates nothing in the audio callback.
    static constexpr float kGainMatchMaxAttenuationDb = -10.0f;
    static constexpr float kGainMatchSignalFloorDb = -55.0f;
    static constexpr float kGainMatchEnhancementAllowanceDb = 0.0f;
    // P6.1 protects bass impact without introducing a low-band gain offset. The
    // controller may only relax the same full-band scalar by a small amount when
    // the aligned LP160 transient envelope would otherwise fall behind bypass.
    static constexpr float kGainMatchLowTransientMaxDeficitDb = 0.20f;
    static constexpr float kGainMatchLowTransientMaxReliefDb = 0.30f;
    static constexpr float kGainMatchLowTransientReleaseSeconds = 0.40f;
    static constexpr std::size_t kGainMatchBandCount = 3;
    enum GainMatchBand : std::size_t { kGainMatchLow = 0, kGainMatchBody = 1, kGainMatchUpper = 2 };
    using GainMatchBands = std::array<float, kGainMatchBandCount>;
    // Complementary one-pole split: low = LP160, body = LP800-LP160,
    // upper = input-LP800. Equal gains reconstruct the input exactly.
    std::array<float, 2> gainMatchRawLowState_{};
    std::array<float, 2> gainMatchRawBodyState_{};
    std::array<float, 2> gainMatchWetLowState_{};
    std::array<float, 2> gainMatchWetBodyState_{};
    float gainMatchLowAlpha_ = 1.0f;
    float gainMatchBodyAlpha_ = 1.0f;
    std::array<double, kGainMatchBandCount> gainMatchRawBlockPowerSum_{};
    double gainMatchRawTotalBlockPowerSum_ = 0.0;
    std::array<float, kGainMatchBandCount> gainMatchRawPowerEnv_{};
    std::array<float, kGainMatchBandCount> gainMatchWetPowerEnv_{};
    float gainMatchRawTotalPowerEnv_ = 0.0f;
    float gainMatchWetTotalPowerEnv_ = 0.0f;
    float gainMatchRawLowBlockPeak_ = 0.0f;
    float gainMatchWetLowBlockPeak_ = 0.0f;
    float gainMatchRawLowTransientEnv_ = 0.0f;
    float gainMatchWetLowTransientEnv_ = 0.0f;
    std::array<float, kGainMatchBandCount> gainMatchTargetBandDb_{};
    std::array<float, kGainMatchBandCount> gainMatchAppliedBandDb_{};
    float gainMatchTargetDb_ = 0.0f; // weighted UI summary
    float gainMatchAppliedDb_ = 0.0f; // weighted UI summary
    int gainMatchSignalSamples_ = 0;
    bool gainMatchMeasurePrimed_ = false;
    bool gainMatchReady_ = false;

    // P7A keeps one learned monitoring target for the single Signature engine.
    // No preset cache or transition target exists; stale post-bypass estimates
    // are relearned through the existing slow engagement path.
    bool gainMatchTargetIsSeed_ = false;
    bool gainMatchSlowEngagement_ = false;
    int gainMatchBypassSettledSamples_ = 0;
    bool gainMatchTargetStale_ = false;
    bool lastGainMatchEnabled_ = false;
    bool activeSmartProtect_ = true; // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};

} // namespace askp
