#pragma once

#include <algorithm>
#include <cmath>

namespace arsonkupik {

// P3 sustained-energy WOW guard for deterministic musical presets.
//
// P7B natural peak budget. A single kick/snare may spend a short 0.65 dB
// transient budget, while sustained material is held near 0.20 dB of limiting.
// Program gain is reduced before the final lookahead stage when peak density is
// high, preserving the -1 dBFS ceiling and avoiding limiter-driven character.
class WowHeadroomGuard {
public:
    static constexpr float kSafeCeilingDb = -1.0f;
    static constexpr float kTransientLimiterBudgetDb = 0.65f;
    static constexpr float kSustainedLimiterBudgetDb = 0.20f;
    // Compatibility name used by the final limiter and older QA helpers. The
    // shallow path is allowed to begin at the bounded transient budget.
    static constexpr float kLimiterBudgetDb = kTransientLimiterBudgetDb;
    static constexpr float kSustainedAttackSeconds = 0.050f;
    static constexpr float kSustainedReleaseSeconds = 0.260f;
    static constexpr float kGainReductionSeconds = 0.052f;
    static constexpr float kRecoverySeconds = 0.055f;
    static constexpr float kShallowLimiterReleaseSeconds = 0.034f;
    static constexpr float kDeepLimiterReleaseSeconds = 0.120f;
    static constexpr float kShallowLimiterGainThreshold = 0.92789749f; // -0.65 dB
    static constexpr float kMinimumGainDb = -12.0f;

    void reset(float initialDb = 0.0f) noexcept
    {
        currentDb_ = std::isfinite(initialDb) ? initialDb : 0.0f;
        sustainedPeak_ = 1.0e-5f;
        primed_ = true;
    }

    float processLocked(float lockedDb,
                        float wetPeakLinear,
                        double sampleRate,
                        int blockSamples) noexcept
    {
        if (!std::isfinite(lockedDb))
            lockedDb = 0.0f;
        if (!std::isfinite(wetPeakLinear) || wetPeakLinear < 0.0f)
            wetPeakLinear = 0.0f;

        const float sr = static_cast<float>(std::max(1.0, sampleRate));
        const float count = static_cast<float>(std::max(1, blockSamples));
        const float peak = std::max(wetPeakLinear, 1.0e-5f);

        // Peak-density envelope. Attack is intentionally tens of milliseconds:
        // one isolated transient barely moves it, while a continuously hot
        // commercial master raises it and receives controlled program trim.
        const float sustainedTau = peak > sustainedPeak_
            ? kSustainedAttackSeconds : kSustainedReleaseSeconds;
        const float sustainedAlpha = 1.0f
            - std::exp(-count / (sustainedTau * sr));
        sustainedPeak_ += sustainedAlpha * (peak - sustainedPeak_);
        sustainedPeak_ = std::max(sustainedPeak_, 1.0e-5f);

        const float instantPeakDb = 20.0f * std::log10(peak);
        const float sustainedPeakDb = 20.0f * std::log10(sustainedPeak_);
        const float instantAllowedDb = kSafeCeilingDb - instantPeakDb
                                     + kTransientLimiterBudgetDb;
        const float sustainedAllowedDb = kSafeCeilingDb - sustainedPeakDb
                                       + kSustainedLimiterBudgetDb;
        const float hardSafetyTarget = std::clamp(
            std::min(lockedDb, instantAllowedDb), kMinimumGainDb, lockedDb);
        const float sustainedTarget = std::clamp(
            std::min(lockedDb, sustainedAllowedDb), kMinimumGainDb, lockedDb);

        if (!primed_) {
            currentDb_ = std::min(hardSafetyTarget, sustainedTarget);
            primed_ = true;
        } else if (hardSafetyTarget < currentDb_) {
            // The lookahead loop will ramp this reduction over ~1 ms. This is
            // the only immediate path and guarantees no transient can demand
            // more than the explicit limiter budget.
            currentDb_ = hardSafetyTarget;
        } else {
            const float targetDb = std::min(lockedDb, sustainedTarget);
            const float tau = targetDb < currentDb_
                ? kGainReductionSeconds : kRecoverySeconds;
            const float alpha = 1.0f - std::exp(-count / (tau * sr));
            currentDb_ += alpha * (targetDb - currentDb_);
        }

        currentDb_ = std::clamp(currentDb_, kMinimumGainDb, lockedDb);
        return currentDb_;
    }

    float currentDb() const noexcept { return currentDb_; }
    float sustainedPeak() const noexcept { return sustainedPeak_; }

private:
    float currentDb_ = 0.0f;
    float sustainedPeak_ = 1.0e-5f;
    bool primed_ = false;
};

} // namespace arsonkupik
