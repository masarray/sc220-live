#include "arsonkupik_dsp.hpp"

#include <algorithm>

namespace arsonkupik {
namespace {
EqBand b(const char* id, EqType type, double frequency, double gain, double q,
         int slope = 12, bool enabled = true)
{
    EqBand band;
    band.id = id;
    band.type = type;
    band.frequency = frequency;
    band.gain_db = gain;
    band.q = q;
    band.slope_db_per_oct = slope;
    band.enabled = enabled;
    return band;
}

PresetPolicy signature_policy()
{
    PresetPolicy p;
    p.context = ProcessingContext::Signature;
    // P7G open gain architecture: Gain Match OFF never pushes programme toward
    // one exact loudness value and never attenuates creative lift merely for
    // being audible. It protects only a small Active floor. Gain Match ON
    // remains the strict comparator; final peak safety remains independent.
    p.target_wow_lift_db = 0.90;
    p.target_prechain_peak_db = -13.4;
    p.max_smart_makeup_db = 4.2;
    p.max_limiter_gr_db = 0.85;
    p.max_width_percent = 190.0;
    p.deterministic_level = true;
    p.fixed_headroom_db = -4.20;
    p.fixed_restore_ratio = 0.93;
    p.moderate_wow_correction_db = 0.0;
    p.max_locked_wow_db = 1.50;
    return p;
}

std::vector<EqBand> masari_eq()
{
    return {
        b("cut-low", EqType::LowCut, 25, 0, 0.70710678, 24),
        b("low-body", EqType::LowShelf, 76, 1.30, 0.64),
        // Keep the 325 Hz cleanup, but no longer scoop the upper strings and
        // piano body before Smart Warm Particles has a chance to assess them.
        b("mud-clean", EqType::Bell, 325, -0.62, 1.06),
        b("presence", EqType::Bell, 2180, 1.16, 0.64),
        b("detail", EqType::Bell, 6250, 1.60, 0.52),
        b("sparkle", EqType::HighShelf, 12650, 4.38, 0.38),
        b("vocal-acoustic-body", EqType::Bell, 170, 0.90, 2.50),
        b("vocal-body-490", EqType::Bell, 490, 1.50, 0.80)
    };
}

CompressorConfig masari_comp()
{
    return {true, -24.4, 1.7, 24.0, 0.032, 0.21, 0.72, 88.0};
}

ColorConfig masari_color()
{
    ColorConfig c;
    c.enabled = true;
    c.drive = 1.36;
    c.body_freq = 166;
    c.body = 12.8;
    c.smart_bass = 55;
    c.warmth_freq = 500;
    c.warmth = 12.8;
    c.harmonics_freq = 2180;
    c.harmonics = 8;
    c.air_freq = 12650;
    c.air = 48.8;
    c.ai_high_repair = 46;
    c.velvet_treble = 66;
    c.god_particles = 92.0;
    c.vocal_tickle = 67;
    c.vocal_presence = 55;
    c.mid_projection = 65;
    c.mix = 30.5;
    c.stereo_mid = 70;
    c.mode = "mastering";
    return c;
}

WidthConfig masari_width()
{
    return {true, 73, 153, 100, 101, 128, 190, 62, true, 150, 4.0};
}

OutputConfig masari_output()
{
    // Limiter is a clean safety stage only. P7G keeps creative lift untouched
    // in normal operation; the wrapper adds only floor protection before its
    // lookahead ceiling, while standalone callers receive the same safety intent.
    return {0.0, 0.0, true, -1.00, 0.0, true, false};
}

Preset make_signature()
{
    Preset p;
    p.id = "default";
    p.name = "Mas Ari Signature";
    p.description = "Adaptive flagship: authoritative bass torque, luxurious body, lively vocal, spacious 3D depth and silky ear-tickle detail with content-aware restraint.";
    p.eq = masari_eq();
    p.compressor = masari_comp();
    p.color = masari_color();
    p.width = masari_width();
    p.output = masari_output();
    p.macro = {100, 78, 68, 80, 76, 88, true};
    p.policy = signature_policy();

    // P7B keeps the P6/P7A Signature tone recipe: EQ, particles, stereo and
    // microdetail remain intact. Only the gain architecture is made more natural before final peak safety;
    // the approved compressor tone and density remain unchanged.
    for (auto& band : p.eq) {
        if (band.id == "vocal-acoustic-body") {
            band.gain_db = 1.06;
            band.q = 2.30;
        } else if (band.id == "mud-clean") {
            band.gain_db = -0.10;
        } else if (band.id == "vocal-body-490") {
            band.frequency = 575.0;
            band.gain_db = 1.96;
            band.q = 0.68;
        }
    }
    p.eq.push_back(b("signature-sub-glide", EqType::LowShelf, 52, 0.90, 0.62));
    p.eq.push_back(b("signature-deep-bloom", EqType::Bell, 78, 0.78, 0.74));
    p.eq.push_back(b("signature-bass-torque", EqType::Bell, 118, 0.96, 0.78));
    p.eq.push_back(b("signature-chest-authority", EqType::Bell, 185, 0.53, 0.94));
    return p;
}

PresetProfile profile(const char* id, const char* name, const char* category,
                      const char* description, double enhance, double bass,
                      double treble, double vocal, double stereo,
                      double detail, double articulation, double density,
                      double body, double space, double outputTrimDb = 0.0)
{
    PresetProfile p;
    p.id = id;
    p.name = name;
    p.category = category;
    p.description = description;
    p.macro = {100.0, enhance, bass, treble, vocal, stereo, true};
    p.detail_focus = detail;
    p.articulation_focus = articulation;
    p.density_tolerance = density;
    p.body_focus = body;
    p.space_focus = space;
    p.output_trim_db = outputTrimDb;
    return p;
}

std::vector<Preset> make_presets()
{
    std::vector<Preset> presets;
    presets.reserve(1);
    presets.push_back(make_signature());
    return presets;
}

std::vector<PresetProfile> make_preset_profiles()
{
    std::vector<PresetProfile> profiles;
    profiles.reserve(13);
    profiles.push_back(profile(
        "signature-balanced", "Signature Balanced", "Signature",
        "The reference Mas Ari Signature workflow: authoritative bass, stable body, lively Mid detail and mono-safe depth.",
        78, 68, 80, 76, 88, 1.070, 1.100, 1.060, 1.000, 1.000));
    profiles.push_back(profile(
        "transparent-polish", "Transparent Polish", "Mastering",
        "Low-footprint polish for material that already balances well.",
        68, 60, 68, 68, 70, 1.020, 1.000, 0.980, 0.990, 0.980, 0.40));
    profiles.push_back(profile(
        "open-detailed", "Open & Detailed", "Mastering",
        "More articulation, separation and upper-air motion without adding high Side.",
        82, 58, 90, 78, 84, 1.140, 1.120, 1.100, 0.980, 1.020));
    profiles.push_back(profile(
        "warm-glue", "Warm Glue", "Mastering",
        "Cohesive body and restrained polish for lean or overly clinical mixes.",
        76, 75, 62, 72, 70, 1.030, 0.980, 1.040, 1.080, 0.960));
    profiles.push_back(profile(
        "dense-mix-clarity", "Dense Mix Clarity", "Problem Solving",
        "Recovers Mid articulation inside crowded arrangements while retaining harshness protection.",
        84, 55, 82, 84, 76, 1.120, 1.080, 1.140, 0.960, 0.980));
    profiles.push_back(profile(
        "dark-mix-lift", "Dark Mix Lift", "Problem Solving",
        "Lifts obscured detail and transient texture without a static high-shelf push.",
        86, 64, 94, 82, 82, 1.150, 1.120, 1.120, 1.000, 1.000));
    profiles.push_back(profile(
        "bright-mix-safe", "Bright Mix Safe", "Problem Solving",
        "Preserves energy while closing particle lift earlier on bright or brittle material.",
        72, 65, 54, 68, 72, 0.980, 0.960, 0.900, 1.020, 0.950, 0.34));
    profiles.push_back(profile(
        "punch-separation", "Punch & Separation", "Mix Bus",
        "Adds rhythmic microcontrast and instrument separation without global loudness pumping.",
        84, 72, 76, 74, 86, 1.080, 1.140, 1.070, 1.030, 1.040));
    profiles.push_back(profile(
        "vocal-forward-silky", "Vocal Forward & Silky", "Tracks",
        "Brings vocal texture and consonant detail forward while keeping the stereo centre stable.",
        78, 58, 78, 94, 68, 1.100, 1.120, 1.080, 1.010, 0.960));
    profiles.push_back(profile(
        "guitar-definition-body", "Guitar Definition & Body", "Tracks",
        "Balances pick definition, cabinet texture and musical low-mid body.",
        80, 62, 76, 82, 78, 1.090, 1.100, 1.080, 1.050, 1.000));
    profiles.push_back(profile(
        "bass-authority", "Bass Authority", "Tracks",
        "Keeps the proven Signature bass torque while avoiding broad low-mid inflation.",
        76, 92, 64, 68, 72, 1.020, 1.050, 1.000, 1.080, 0.960));
    profiles.push_back(profile(
        "drums-punch-skin", "Drums Punch & Skin", "Tracks",
        "Emphasises attack, snare skin and cymbal texture with bounded dense-section closure.",
        86, 80, 82, 64, 82, 1.090, 1.160, 1.080, 1.040, 1.000));
    profiles.push_back(profile(
        "acoustic-natural-air", "Acoustic Natural Air", "Tracks",
        "Natural string texture, vocal breath and room detail with conservative width.",
        74, 60, 78, 80, 74, 1.080, 1.100, 1.040, 1.000, 0.980));
    return profiles;
}
} // namespace

const char* processing_context_name(ProcessingContext context)
{
    switch (context) {
    case ProcessingContext::Signature: return "SIGNATURE";
    case ProcessingContext::Master:    return "MASTER";
    case ProcessingContext::MixBus:    return "MIX BUS";
    case ProcessingContext::Track:     return "TRACK";
    case ProcessingContext::Creative:  return "CREATIVE";
    }
    return "SIGNATURE";
}

const std::vector<Preset>& factory_presets()
{
    static const std::vector<Preset> presets = make_presets();
    return presets;
}

const Preset* find_preset(const std::string& id)
{
    (void)id;
    // P7A maps every legacy or unknown factory ID to the sole Signature engine.
    return &default_preset();
}

const Preset& default_preset()
{
    return factory_presets().front();
}

const std::vector<PresetProfile>& factory_preset_profiles()
{
    static const std::vector<PresetProfile> profiles = make_preset_profiles();
    return profiles;
}

const PresetProfile* find_preset_profile(const std::string& id)
{
    const auto& profiles = factory_preset_profiles();
    const auto it = std::find_if(profiles.begin(), profiles.end(),
        [&](const PresetProfile& profile) { return profile.id == id; });
    return it != profiles.end() ? &*it : &default_preset_profile();
}

const PresetProfile& default_preset_profile()
{
    return factory_preset_profiles().front();
}

} // namespace arsonkupik
