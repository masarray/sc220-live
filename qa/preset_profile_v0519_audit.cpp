#include "arsonkupik_dsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
constexpr double kPi = 3.1415926535897932384626433832795;

enum class Material { Rock, Dark, DenseBright, Acoustic };

struct Render {
    std::vector<float> left;
    std::vector<float> right;
    double rms = 0.0;
    double peak = 0.0;
    bool finite = true;
};

float noise(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0x00ffffffu)
         * (2.0f / 16777215.0f) - 1.0f;
}

std::pair<float, float> sampleFor(Material material, double t,
                                  std::uint32_t& state)
{
    const auto s = [t](double hz) {
        return static_cast<float>(std::sin(2.0 * kPi * hz * t));
    };
    if (material == Material::Rock) {
        const double beat = std::fmod(t, 0.50);
        const double backbeat = std::fmod(t + 0.25, 0.50);
        const float kick = static_cast<float>(0.24 * std::exp(-beat * 17.0))
                         * (s(62.0) + 0.24f * s(124.0));
        const float snare = static_cast<float>(0.12 * std::exp(-backbeat * 24.0))
                          * (0.72f * noise(state) + 0.28f * s(190.0));
        const float guitar = 0.085f * s(182.0) + 0.060f * s(364.0)
                           + 0.040f * s(1460.0) + 0.025f * s(2920.0);
        const float cymbal = 0.014f * noise(state)
                           * (0.35f + 0.65f
                              * static_cast<float>(std::exp(-beat * 8.0)));
        return {kick + snare * 0.92f + guitar + cymbal,
                kick + snare * 1.08f + guitar * 0.94f - cymbal * 0.76f};
    }
    if (material == Material::Dark) {
        const float mid = 0.13f * s(74.0) + 0.10f * s(221.0)
                        + 0.055f * s(442.0) + 0.018f * s(1320.0);
        return {mid, mid * 0.97f};
    }
    if (material == Material::DenseBright) {
        const float common = 0.070f * s(110.0) + 0.055f * s(220.0)
                           + 0.050f * s(880.0) + 0.042f * s(2380.0)
                           + 0.035f * s(5200.0) + 0.020f * noise(state);
        const float side = 0.018f * s(3170.0) + 0.014f * s(7600.0);
        return {common + side, common - side};
    }
    const double pluckPhase = std::fmod(t, 0.38);
    const float pluck = static_cast<float>(std::exp(-pluckPhase * 9.0))
                      * (0.115f * s(196.0) + 0.070f * s(392.0)
                         + 0.035f * s(1760.0));
    const float voice = 0.060f * s(740.0) + 0.028f * s(1480.0);
    return {pluck + voice, pluck * 0.96f + voice * 1.03f};
}

Render render(const arsonkupik::PresetProfile& profile, Material material,
              double sampleRate, std::size_t block, bool neutralScene)
{
    arsonkupik::ArSonKuPikEngine engine;
    engine.prepare(sampleRate, 2);
    arsonkupik::RuntimeParams params;
    params.preset_index = 0;
    params.advanced_override = true;
    params.enhance = profile.macro.enhance;
    params.smart_bass = profile.macro.smart_bass;
    params.smart_treble = profile.macro.smart_treble;
    params.vocal_body = profile.macro.vocal_body;
    params.stereo_magic = profile.macro.stereo_magic;
    params.smart_protect = profile.macro.smart_protect;
    params.output_trim_db = profile.output_trim_db;
    // Neutralise the release vitality calibration only inside this A/B
    // harness. Runtime presets intentionally receive those common multipliers.
    constexpr double kNeutralDetail = 1.0 / 1.045;
    constexpr double kNeutralArticulation = 1.0 / 1.025;
    constexpr double kNeutralBodyParticle = 1.0 / 1.012;
    params.detail_focus =
        neutralScene ? kNeutralDetail : profile.detail_focus;
    params.articulation_focus =
        neutralScene ? kNeutralArticulation : profile.articulation_focus;
    params.density_tolerance =
        neutralScene ? 1.0 : profile.density_tolerance;
    params.body_focus =
        neutralScene ? kNeutralBodyParticle : profile.body_focus;
    params.space_focus = neutralScene ? 1.0 : profile.space_focus;
    engine.set_runtime_params(params);
    engine.reset();

    const std::size_t total =
        static_cast<std::size_t>(std::round(sampleRate * 0.90));
    const std::size_t settle =
        static_cast<std::size_t>(std::round(sampleRate * 0.30));
    std::vector<float> left(block);
    std::vector<float> right(block);
    std::array<float*, 2> planes{{left.data(), right.data()}};
    std::uint32_t state = 0x4d617341u;
    Render result;
    result.left.reserve(total - settle);
    result.right.reserve(total - settle);
    double power = 0.0;
    std::size_t measured = 0;

    for (std::size_t offset = 0; offset < total; offset += block) {
        const std::size_t count = std::min(block, total - offset);
        for (std::size_t i = 0; i < count; ++i) {
            const auto input = sampleFor(
                material, static_cast<double>(offset + i) / sampleRate, state);
            left[i] = input.first;
            right[i] = input.second;
        }
        engine.process(planes.data(), 2, count);
        for (std::size_t i = 0; i < count; ++i) {
            const float l = left[i];
            const float r = right[i];
            result.finite = result.finite
                && std::isfinite(l) && std::isfinite(r);
            result.peak = std::max(
                result.peak,
                static_cast<double>(std::max(std::abs(l), std::abs(r))));
            if (offset + i >= settle) {
                result.left.push_back(l);
                result.right.push_back(r);
                power += 0.5 * (static_cast<double>(l) * l
                              + static_cast<double>(r) * r);
                ++measured;
            }
        }
    }
    result.rms = std::sqrt(power / std::max<std::size_t>(1, measured));
    return result;
}

double correlation(const Render& a, const Render& b)
{
    double dot = 0.0;
    double aa = 0.0;
    double bb = 0.0;
    const auto count = std::min(a.left.size(), b.left.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto pair : {
                 std::pair<float, float>{a.left[i], b.left[i]},
                 std::pair<float, float>{a.right[i], b.right[i]}}) {
            dot += static_cast<double>(pair.first) * pair.second;
            aa += static_cast<double>(pair.first) * pair.first;
            bb += static_cast<double>(pair.second) * pair.second;
        }
    }
    return dot / std::sqrt(std::max(1.0e-24, aa * bb));
}

double residualDb(const Render& a, const Render& b)
{
    double diff = 0.0;
    double reference = 0.0;
    std::size_t samples = 0;
    const auto count = std::min(a.left.size(), b.left.size());
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto pair : {
                 std::pair<float, float>{a.left[i], b.left[i]},
                 std::pair<float, float>{a.right[i], b.right[i]}}) {
            const double d = static_cast<double>(pair.first) - pair.second;
            diff += d * d;
            reference += static_cast<double>(pair.first) * pair.first;
            ++samples;
        }
    }
    const double diffRms = std::sqrt(diff / std::max<std::size_t>(1, samples));
    const double refRms =
        std::sqrt(reference / std::max<std::size_t>(1, samples));
    return 20.0 * std::log10((diffRms + 1.0e-15) / (refRms + 1.0e-15));
}
} // namespace

int main()
{
    const auto& profiles = arsonkupik::factory_preset_profiles();
    bool finiteAndContained = profiles.size() == 13;
    double worstPeak = 0.0;
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (const auto& profile : profiles) {
            for (auto material : {Material::Rock, Material::Dark,
                                  Material::DenseBright,
                                  Material::Acoustic}) {
                const auto result =
                    render(profile, material, sampleRate, 64, false);
                finiteAndContained = finiteAndContained
                    && result.finite && result.peak <= 1.00001;
                worstPeak = std::max(worstPeak, result.peak);
            }
        }
    }

    const auto& balanced = arsonkupik::default_preset_profile();
    const auto rockNeutral =
        render(balanced, Material::Rock, 48000.0, 64, true);
    const auto rockFocused =
        render(balanced, Material::Rock, 48000.0, 64, false);
    const auto& darkLift = profiles.at(5);
    const auto darkNeutral =
        render(darkLift, Material::Acoustic, 48000.0, 64, true);
    const auto darkFocused =
        render(darkLift, Material::Acoustic, 48000.0, 64, false);

    const double rockCorrelation = correlation(rockNeutral, rockFocused);
    const double rockLevelDelta = 20.0 * std::log10(
        (rockFocused.rms + 1.0e-15) / (rockNeutral.rms + 1.0e-15));
    const double darkResidual = residualDb(darkNeutral, darkFocused);
    const bool rockPreserved = rockCorrelation >= 0.9990
                            && std::abs(rockLevelDelta) <= 0.35;
    const bool sceneAudibleButBounded =
        darkResidual >= -90.0 && darkResidual <= -22.0;

    std::printf("profiles=%zu multirate/material finite=%s worstPeak=%.9f\n",
                profiles.size(), finiteAndContained ? "yes" : "no",
                worstPeak);
    std::printf(
        "rock preservation correlation=%.9f levelDelta=%+.4f dB\n",
        rockCorrelation, rockLevelDelta);
    std::printf("dark-lift articulated focused/neutral residual=%+.3f dB\n",
                darkResidual);
    std::printf("V0519_PRESET_PROFILE_AUDIT=%s\n",
                finiteAndContained && rockPreserved && sceneAudibleButBounded
                    ? "PASS" : "FAIL");
    return finiteAndContained && rockPreserved && sceneAudibleButBounded
        ? 0 : 1;
}
