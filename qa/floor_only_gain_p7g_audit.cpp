#include "arsonkupik_dsp.hpp"
#include "../engine/wow_headroom_guard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace arsonkupik;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr std::size_t kFrames = 48000 * 4;
constexpr std::size_t kMeasureStart = 48000 * 2;
constexpr std::size_t kBlock = 64;
constexpr int kLookahead = 48;
constexpr float kCeilingDb = -1.0f;

float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
double gainToDb(double gain) { return 20.0 * std::log10(std::max(gain, 1.0e-12)); }

double rmsStereo(const std::vector<float>& left,
                 const std::vector<float>& right,
                 std::size_t start)
{
    long double power = 0.0;
    for (std::size_t i = start; i < left.size(); ++i)
        power += (long double)left[i] * left[i]
               + (long double)right[i] * right[i];
    return std::sqrt((double)(power / (2.0L * (left.size() - start))));
}

double peakStereo(const std::vector<float>& left,
                  const std::vector<float>& right,
                  std::size_t start)
{
    double peak = 0.0;
    for (std::size_t i = start; i < left.size(); ++i)
        peak = std::max({peak, std::abs((double)left[i]), std::abs((double)right[i])});
    return peak;
}

void makeProgram(std::vector<float>& left,
                 std::vector<float>& right,
                 int kind,
                 double targetPeak)
{
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double t = (double)i / kSampleRate;
        double l = 0.0, r = 0.0;
        if (kind == 0) { // dense full-range master
            const double mod = 0.84 + 0.16 * std::sin(2.0 * kPi * 1.3 * t);
            l = mod * (0.56 * std::sin(2.0 * kPi * 61.0 * t)
                     + 0.31 * std::sin(2.0 * kPi * 113.0 * t)
                     + 0.27 * std::sin(2.0 * kPi * 233.0 * t)
                     + 0.21 * std::sin(2.0 * kPi * 997.0 * t)
                     + 0.14 * std::sin(2.0 * kPi * 3170.0 * t)
                     + 0.08 * std::sin(2.0 * kPi * 9100.0 * t));
            r = mod * (0.54 * std::sin(2.0 * kPi * 61.0 * t + 0.02)
                     + 0.30 * std::sin(2.0 * kPi * 113.0 * t - 0.04)
                     + 0.25 * std::sin(2.0 * kPi * 233.0 * t + 0.10)
                     + 0.20 * std::sin(2.0 * kPi * 997.0 * t + 0.07)
                     + 0.13 * std::sin(2.0 * kPi * 3170.0 * t - 0.10)
                     + 0.09 * std::sin(2.0 * kPi * 9100.0 * t + 0.30));
        } else if (kind == 1) { // transient kick/snare program
            const double kickPhase = std::fmod(t, 0.5);
            const double snarePhase = std::fmod(t + 0.25, 0.5);
            const double kick = kickPhase < 0.08
                ? std::exp(-kickPhase * 35.0)
                    * std::sin(2.0 * kPi * (52.0 + 35.0 * std::exp(-kickPhase * 30.0)) * t)
                : 0.0;
            const double snare = snarePhase < 0.035
                ? std::exp(-snarePhase * 80.0)
                    * (std::sin(2.0 * kPi * 190.0 * t)
                       + 0.4 * std::sin(2.0 * kPi * 4500.0 * t))
                : 0.0;
            l = 0.50 * kick + 0.18 * snare + 0.14 * std::sin(2.0 * kPi * 110.0 * t)
              + 0.11 * std::sin(2.0 * kPi * 900.0 * t)
              + 0.08 * std::sin(2.0 * kPi * 6500.0 * t);
            r = 0.48 * kick + 0.17 * snare + 0.13 * std::sin(2.0 * kPi * 110.0 * t + 0.03)
              + 0.10 * std::sin(2.0 * kPi * 900.0 * t - 0.08)
              + 0.09 * std::sin(2.0 * kPi * 6500.0 * t + 0.25);
        } else { // vocal/instrument detail program
            const double env = 0.65 + 0.25 * std::sin(2.0 * kPi * 0.8 * t);
            l = env * (0.18 * std::sin(2.0 * kPi * 95.0 * t)
                     + 0.16 * std::sin(2.0 * kPi * 190.0 * t)
                     + 0.14 * std::sin(2.0 * kPi * 380.0 * t)
                     + 0.12 * std::sin(2.0 * kPi * 1500.0 * t)
                     + 0.10 * std::sin(2.0 * kPi * 3200.0 * t)
                     + 0.09 * std::sin(2.0 * kPi * 7500.0 * t)
                     + 0.06 * std::sin(2.0 * kPi * 13000.0 * t));
            r = env * (0.17 * std::sin(2.0 * kPi * 95.0 * t + 0.03)
                     + 0.15 * std::sin(2.0 * kPi * 190.0 * t - 0.05)
                     + 0.13 * std::sin(2.0 * kPi * 380.0 * t + 0.09)
                     + 0.11 * std::sin(2.0 * kPi * 1500.0 * t - 0.12)
                     + 0.10 * std::sin(2.0 * kPi * 3200.0 * t + 0.20)
                     + 0.10 * std::sin(2.0 * kPi * 7500.0 * t - 0.25)
                     + 0.06 * std::sin(2.0 * kPi * 13000.0 * t + 0.40));
        }
        left[i] = (float)l;
        right[i] = (float)r;
    }

    const double sourcePeak = peakStereo(left, right, 0);
    const double scale = targetPeak / std::max(sourcePeak, 1.0e-12);
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = (float)(left[i] * scale);
        right[i] = (float)(right[i] * scale);
    }
}

struct LimiterStats {
    double maxGrDb = 0.0;
    double p95GrDb = 0.0;
};

LimiterStats applyLookahead(std::vector<float>& left,
                            std::vector<float>& right)
{
    const float ceiling = dbToGain(kCeilingDb);
    const float releaseFast = 1.0f - std::exp(-1.0f / (float)(kSampleRate * 0.034));
    const float releaseSlow = 1.0f - std::exp(-1.0f / (float)(kSampleRate * 0.120));
    std::vector<float> delayL((std::size_t)kLookahead + 1, 0.0f);
    std::vector<float> delayR((std::size_t)kLookahead + 1, 0.0f);
    std::vector<float> history((std::size_t)kLookahead, 1.0f);
    std::vector<float> gr;
    gr.reserve(left.size());
    int write = 0, histWrite = 0, hold = 0;
    double historySum = (double)kLookahead;
    float detector = 1.0f;

    for (std::size_t i = 0; i < left.size(); ++i) {
        const float peak = std::max(std::abs(left[i]), std::abs(right[i]));
        const float requested = peak > ceiling ? ceiling / (peak + 1.0e-9f) : 1.0f;
        if (requested < detector) {
            detector = requested;
            hold = kLookahead;
        } else if (hold > 0) {
            --hold;
        } else {
            const float alpha = detector >= WowHeadroomGuard::kShallowLimiterGainThreshold
                ? releaseFast : releaseSlow;
            detector += alpha * (1.0f - detector);
        }
        const float old = history[(std::size_t)histWrite];
        history[(std::size_t)histWrite] = detector;
        if (++histWrite == kLookahead) histWrite = 0;
        historySum += (double)detector - old;
        const float gain = (float)(historySum / (double)kLookahead);

        int read = write + 1;
        if (read == kLookahead + 1) read = 0;
        const float delayedL = delayL[(std::size_t)read];
        const float delayedR = delayR[(std::size_t)read];
        delayL[(std::size_t)write] = left[i];
        delayR[(std::size_t)write] = right[i];
        write = read;
        left[i] = delayedL * gain;
        right[i] = delayedR * gain;
        gr.push_back((float)std::max(0.0, -gainToDb(gain)));
    }

    std::sort(gr.begin() + (std::ptrdiff_t)kMeasureStart, gr.end());
    const auto first = gr.begin() + (std::ptrdiff_t)kMeasureStart;
    const auto count = (std::size_t)std::distance(first, gr.end());
    const auto p95 = first + (std::ptrdiff_t)std::min(count - 1,
        (std::size_t)std::floor(0.95 * (double)(count - 1)));
    return { gr.empty() ? 0.0 : (double)*std::max_element(first, gr.end()),
             count > 0 ? (double)*p95 : 0.0 };
}

struct Result {
    double engineLiftDb = 0.0;
    double engineCrestDeltaDb = 0.0;
    double normalizationDb = 0.0;
    double finalLiftDb = 0.0;
    double crestDeltaDb = 0.0;
    double peak = 0.0;
    double unprotectedLimiterDemandDb = 0.0;
    LimiterStats limiter;
};

Result runCase(int kind, double inputPeak, double targetDb,
               bool floorOnly)
{
    std::vector<float> left(kFrames), right(kFrames);
    makeProgram(left, right, kind, inputPeak);
    const auto dryLeft = left;
    const auto dryRight = right;

    const auto& preset = default_preset();
    RuntimeParams params;
    params.preset_id = preset.id;
    params.advanced_override = true;
    params.enhance = preset.macro.enhance;
    params.smart_bass = preset.macro.smart_bass;
    params.smart_treble = preset.macro.smart_treble;
    params.vocal_body = preset.macro.vocal_body;
    params.stereo_magic = preset.macro.stereo_magic;
    params.output_trim_db = 0.0;
    params.smart_protect = true;
    params.external_bypass = true;
    params.external_peak_limiter = true;

    ArSonKuPikEngine engine;
    engine.prepare(kSampleRate, 2);
    engine.set_runtime_params(params);
    for (std::size_t offset = 0; offset < kFrames; offset += kBlock) {
        const auto count = std::min(kBlock, kFrames - offset);
        float* planes[2] = { left.data() + offset, right.data() + offset };
        engine.process(planes, 2, count);
    }

    const double dryRms = rmsStereo(dryLeft, dryRight, kMeasureStart);
    const double wetRms = rmsStereo(left, right, kMeasureStart);
    const double engineLiftDb = gainToDb(wetRms / dryRms);
    const double dryPeakBefore = peakStereo(dryLeft, dryRight, kMeasureStart);
    const double wetPeakBefore = peakStereo(left, right, kMeasureStart);
    const double engineCrestDeltaDb = gainToDb(wetPeakBefore / wetRms)
                                    - gainToDb(dryPeakBefore / dryRms);
    double correctionDb = floorOnly
        ? (engineLiftDb < targetDb ? targetDb - engineLiftDb : 0.0)
        : targetDb - engineLiftDb;
    correctionDb = std::clamp(correctionDb, -12.0,
        default_preset().policy.max_locked_wow_db);

    const double unprotectedLimiterDemandDb = std::max(
        0.0, gainToDb(wetPeakBefore) + correctionDb - kCeilingDb);

    // Mirror the wrapper's sustained-energy guard: floor-only gain is reduced
    // before final limiting when hot material would spend more than the P7G
    // transient/sustained limiter budgets.
    WowHeadroomGuard guard;
    guard.reset((float)correctionDb);
    for (std::size_t offset = 0; offset < left.size(); offset += kBlock) {
        const auto count = std::min(kBlock, left.size() - offset);
        float blockPeak = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            blockPeak = std::max(blockPeak, std::abs(left[offset + i]));
            blockPeak = std::max(blockPeak, std::abs(right[offset + i]));
        }
        const float appliedDb = guard.processLocked(
            (float)correctionDb, blockPeak, kSampleRate, (int)count);
        const float gain = dbToGain(appliedDb);
        for (std::size_t i = 0; i < count; ++i) {
            left[offset + i] *= gain;
            right[offset + i] *= gain;
        }
    }
    const LimiterStats limiter = applyLookahead(left, right);

    const double finalRms = rmsStereo(left, right, kMeasureStart);
    const double dryPeak = peakStereo(dryLeft, dryRight, kMeasureStart);
    const double finalPeak = peakStereo(left, right, kMeasureStart);
    const double dryCrest = gainToDb(dryPeak / dryRms);
    const double finalCrest = gainToDb(finalPeak / finalRms);
    return {engineLiftDb, engineCrestDeltaDb, correctionDb,
            gainToDb(finalRms / dryRms), finalCrest - dryCrest,
            finalPeak, unprotectedLimiterDemandDb, limiter};
}
} // namespace

int main()
{
    std::cout << std::fixed << std::setprecision(3);
    bool pass = true;
    const std::array<const char*, 3> names{"dense", "transient", "detail"};
    for (int kind = 0; kind < 3; ++kind) {
        for (double inputPeak : {0.25, 0.60, 0.97}) {
            const Result natural = runCase(kind, inputPeak, 0.90, true);
            const Result forced = runCase(kind, inputPeak, 4.00, false);
            const bool floorOnly = natural.normalizationDb >= -0.001
                && (natural.engineLiftDb < 0.90
                    || std::abs(natural.normalizationDb) <= 0.001);
            const bool naturalOk = natural.finalLiftDb >= 1.50
                && natural.finalLiftDb <= 5.60
                && natural.peak <= dbToGain(-0.98f)
                && natural.limiter.p95GrDb <= 0.65
                && natural.limiter.maxGrDb <= 0.70
                && natural.crestDeltaDb >= -5.00
                && floorOnly;
            const bool ok = naturalOk;
            pass = pass && ok;
            std::cout << names[(std::size_t)kind]
                      << " input_peak=" << inputPeak
                      << " engine_lift_db=" << natural.engineLiftDb
                      << " engine_crest_delta_db=" << natural.engineCrestDeltaDb
                      << " normalize_db=" << natural.normalizationDb
                      << " final_lift_db=" << natural.finalLiftDb
                      << " crest_delta_db=" << natural.crestDeltaDb
                      << " limiter_p95_db=" << natural.limiter.p95GrDb
                      << " limiter_max_db=" << natural.limiter.maxGrDb
                      << " natural_raw_limiter_demand_db="
                      << natural.unprotectedLimiterDemandDb
                      << " old_plus4_raw_limiter_demand_db="
                      << forced.unprotectedLimiterDemandDb
                      << " result=" << (ok ? "PASS" : "FAIL") << '\n';
        }
    }
    std::cout << "P7G_FLOOR_ONLY_GAIN_ARCHITECTURE="
              << (pass ? "PASS" : "FAIL") << '\n';
    return pass ? 0 : 1;
}
