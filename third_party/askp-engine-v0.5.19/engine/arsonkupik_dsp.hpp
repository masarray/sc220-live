#pragma once

// Keep this header and arsonkupik_dsp.cpp from the same release package.
// The implementation checks this revision at compile time to prevent partial
// patch overlays from producing misleading cascades of undeclared-symbol errors.
#define ASKP_DSP_HEADER_REVISION 2026072702

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arsonkupik {

constexpr double kDefaultSampleRate = 48000.0;
constexpr int kMaxChannels = 8;

double db_to_gain(double db);
double gain_to_db(double gain);
double clamp(double value, double lo, double hi);
double clamp01(double value);

enum class EqType {
    LowCut,
    LowShelf,
    Bell,
    Notch,
    HighShelf,
    HighCut,
    AllPass
};

struct EqBand {
    std::string id;
    EqType type = EqType::Bell;
    double frequency = 1000.0;
    double gain_db = 0.0;
    double q = 0.70710678;
    int slope_db_per_oct = 12;
    bool enabled = true;
    // 0 = exact identity, 1 = full filter coefficients.
    double coefficient_mix = 1.0;
};

struct CompressorConfig {
    bool enabled = true;
    double threshold_db = -24.4;
    double ratio = 1.7;
    double knee_db = 24.0;
    double attack_sec = 0.032;
    double release_sec = 0.21;
    double makeup_gain_db = 0.88;
    double parallel_mix = 92.0; // percent
};

struct ColorConfig {
    bool enabled = true;
    double drive = 3.12;
    double body_freq = 166.0;
    double body = 12.8;
    double smart_bass = 55.0;
    double warmth_freq = 500.0;
    double warmth = 12.8;
    double harmonics_freq = 2180.0;
    double harmonics = 34.0;
    double air_freq = 12650.0;
    double air = 48.8;
    double ai_high_repair = 46.0;
    double velvet_treble = 66.0;
    double god_particles = 92.0;
    double vocal_tickle = 67.0;
    double vocal_presence = 55.0;
    double mid_projection = 65.0;
    double mix = 30.5;
    double stereo_mid = 70.0;
    std::string mode = "mastering";
    // Smart Warm Core control. A stable 72-300 Hz foundation remains present;
    // local 285-430 Hz mud and core transients may only reduce a tiny extension
    // or harmonic tickle, never the foundation itself.
    double warm_particles = 72.0;
    double warm_box_guard = 82.0;
};

struct WidthConfig {
    bool enabled = true;
    double mix = 73.0;
    double width = 153.0;
    double low_width = 100.0;
    double low_mid_width = 101.0;
    double mid_width = 128.0;
    double high_width = 200.0;
    double source_protect = 56.0;
    bool mono_bass = true;
    double mono_bass_freq = 150.0;
    double side_tone = 4.28;
};

struct OutputConfig {
    double input_gain_db = 0.0;
    double output_gain_db = -0.55;
    bool limiter_enabled = true;
    double limiter_ceiling_db = -1.05;
    double limiter_drive = 0.76;
    bool punch_protect = true;
    bool bypass = false;
};

struct MacroConfig {
    double masari_feel = 100.0;
    double enhance = 65.0;
    double smart_bass = 55.0;
    double smart_treble = 70.0;
    double vocal_body = 65.0;
    double stereo_magic = 85.0;
    bool smart_protect = true;
};

// v0.5: factory presets are operating contexts, not only tonal snapshots.
// The policy controls how much level, headroom and stereo freedom a preset is
// allowed to use at its intended insert position in a DAW.
enum class ProcessingContext {
    Signature,
    Master,
    MixBus,
    Track,
    Creative
};

const char* processing_context_name(ProcessingContext context);

struct PresetPolicy {
    ProcessingContext context = ProcessingContext::Creative;
    // Gain Match OFF is not a loudness matcher. The wrapper may lift only when
    // Active falls below this identity floor; it never attenuates creative DSP
    // lift merely for exceeding a target. Peak safety remains independent.
    double target_wow_lift_db = 0.90;
    double target_prechain_peak_db = -13.0;
    double max_smart_makeup_db = 4.5;
    double max_limiter_gr_db = 0.85;
    double max_width_percent = 180.0;
    // Signature uses a content-independent reserve/restore pair. P7G then
    // measures once for floor-only protection before final lookahead safety,
    // preventing kicks and song sections from changing the perceived base level.
    bool deterministic_level = false;
    double fixed_headroom_db = -4.4;
    double fixed_restore_ratio = 0.84;
    double moderate_wow_correction_db = 0.0;
    double max_locked_wow_db = 1.50;
};

struct Preset {
    std::string id;
    std::string name;
    std::string description;
    std::vector<EqBand> eq;
    CompressorConfig compressor;
    ColorConfig color;
    WidthConfig width;
    OutputConfig output;
    MacroConfig macro;
    PresetPolicy policy;
};

const std::vector<Preset>& factory_presets();
const Preset* find_preset(const std::string& id);
const Preset& default_preset();

// v0.5.19: Mas Ari Signature is the immutable DSP engine. Preset profiles are
// curated starting points layered on top of that engine; they never own or
// swap DSP topology. Visible macros provide the main workflow, while the five
// tightly bounded scene dimensions let factory profiles guide the adaptive
// lanes without becoming unrelated engines.
struct PresetProfile {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::string engine_id = "mas-ari-signature";
    MacroConfig macro;
    double output_trim_db = 0.0;
    double detail_focus = 1.0;
    double articulation_focus = 1.0;
    double density_tolerance = 1.0;
    double body_focus = 1.0;
    double space_focus = 1.0;
};

const std::vector<PresetProfile>& factory_preset_profiles();
const PresetProfile* find_preset_profile(const std::string& id);
const PresetProfile& default_preset_profile();

struct RuntimeParams {
    std::string preset_id = "default";
    // Real-time wrappers should prefer the factory index. It avoids creating a
    // temporary std::string in the audio callback; preset_id remains supported
    // for the standalone engine API and existing tools.
    int preset_index = -1;
    double enhance = 65.0;
    double smart_bass = 55.0;
    double smart_treble = 70.0;
    double vocal_body = 65.0;
    double stereo_magic = 85.0;
    double output_trim_db = -0.55;
    // Factory/user presets carry these bounded scene dimensions. They are
    // smoothed inside the one Signature engine and affect generated lanes only.
    double detail_focus = 1.0;
    double articulation_focus = 1.0;
    double density_tolerance = 1.0;
    double body_focus = 1.0;
    double space_focus = 1.0;
    bool smart_protect = true;
    bool bypass = false;
    bool advanced_override = false;
    // Wrappers with their own latency-aligned raw bypass can skip the engine's
    // per-sample smoother and wet/dry blend. Shared-engine callers default to
    // the original internal bypass behaviour.
    bool external_bypass = false;
    // Wrappers with a latency-compensated lookahead limiter can disable the
    // engine's zero-lookahead peak stage and avoid cascading two limiters.
    bool external_peak_limiter = false;
};

class Biquad {
public:
    Biquad() = default;
    void reset();
    void set_identity();
    void set(EqType type, double sample_rate, double frequency, double q,
             double gain_db = 0.0, double coefficient_mix = 1.0);
    float process(float x);
    bool sanitize_state() noexcept;
private:
    void set_coefficient_targets(double b0, double b1, double b2,
                                 double a1, double a2, double sampleRate);

    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
    double target_b0_ = 1.0, target_b1_ = 0.0, target_b2_ = 0.0;
    double target_a1_ = 0.0, target_a2_ = 0.0;
    double coefficient_alpha_ = 1.0;
    double z1_ = 0.0, z2_ = 0.0;
    bool coefficients_initialised_ = false;
    bool coefficients_settled_ = true;
};

class OnePoleSmoother {
public:
    void prepare(double sample_rate, double seconds, double initial = 0.0);
    double process(double target);
    void reset(double value = 0.0);
private:
    double alpha_ = 0.001;
    double state_ = 0.0;
};

struct MeterState {
    double input_peak_db = -120.0;
    double output_peak_db = -120.0;
    double gain_reduction_db = 0.0;
    double correlation = 1.0;
    bool clipping = false;
};

// P7C/P7D/P7E read-only engineering telemetry. Values are bounded permission scalars
// applied only to additive/residue lanes; 1.0 means the approved Signature
// recipe, below 1.0 means content-aware restraint and slightly above 1.0 means
// a small need-based lift. Dry foundations and global output gain never use
// these values.
struct AdaptiveSignatureState {
    double bass_extension = 1.0;
    double warm_particles = 1.0;
    double lowmid_particles = 1.0;
    double presence_particles = 1.0;
    double tickle_particles = 1.0;
    double air_particles = 1.0;
    double skin_particles = 1.0;
    double low_level_support = 1.0;
    double generated_side = 1.0;
    double presence_articulation = 1.0;
    double tickle_articulation = 1.0;
    double air_articulation = 1.0;
    double skin_articulation = 1.0;
    double low_density = 0.0;
    double presence_density = 0.0;
    double air_density = 0.0;
    double stereo_risk = 0.0;
    double vocal_body_companion = 0.0;
    double vocal_body_fast_bridge = 0.0;
    double vocal_body_center_confidence = 0.0;
    // P1B is a separate male chest/throat permission. The original P1 field
    // remains unchanged for bright female-vocal regression and telemetry.
    double male_vocal_body_companion = 0.0;
    double male_vocal_chest_confidence = 0.0;
    double male_vocal_center_confidence = 0.0;
};

class DynamicsProcessor {
public:
    void prepare(double sample_rate, const CompressorConfig& cfg);
    void set_config(const CompressorConfig& cfg);
    void reset();
    std::pair<float,float> process(float l, float r);
    double gain_reduction_db() const { return last_gr_db_; }
private:
    void update_time_constants();

    double sample_rate_ = kDefaultSampleRate;
    CompressorConfig cfg_{};
    double env_ = 0.0;
    double last_gr_db_ = 0.0;
    double attack_coeff_ = 0.0;
    double release_coeff_ = 0.0;
    double knee_start_gain_ = 1.0;
    double ratio_slope_ = 0.0;
    double makeup_gain_ = 1.0;
    double dry_mix_ = 0.0;
    double wet_mix_ = 1.0;
};

class LimiterProcessor {
public:
    void prepare(double sample_rate, const OutputConfig& cfg);
    void set_config(const OutputConfig& cfg);
    void reset();
    std::pair<float,float> process(float l, float r);
    double gain_reduction_db() const { return last_gr_db_; }
private:
    void update_constants();

    double sample_rate_ = kDefaultSampleRate;
    OutputConfig cfg_{};
    double env_ = 0.0;
    double last_gr_db_ = 0.0;
    double drive_gain_ = 1.0;
    double release_alpha_ = 0.0;
    double ceiling_gain_ = 1.0;
};

class ArSonKuPikEngine {
public:
    ArSonKuPikEngine();
    void prepare(double sample_rate, std::size_t channels);
    void set_runtime_params(const RuntimeParams& params);
    void set_output_trim_db(double db) noexcept;
    void reset();
    void process(float** planes, std::size_t channels, std::size_t frames);
    const MeterState& meters() const { return meters_; }
    const AdaptiveSignatureState& adaptive_state() const { return adaptive_state_; }
#if defined(ASKP_ENGINEERING_TEST_HOOKS)
    void set_engineering_male_vocal_body_enabled(bool enabled) noexcept
    {
        engineering_male_vocal_body_enabled_ = enabled;
    }
#endif
    const Preset& current_preset() const { return *preset_; }

private:
    struct SaturationRuntime {
        double drive = 1.0;
        double inverse_drive = 1.0;
        double wet = 0.0;
        double dry = 1.0;
    };

    struct ColorRuntime {
        bool active = false;
        bool bass_active = false, warm_active = false;
        bool presence_active = false, air_active = false;
        bool side_air_active = false, mid_active = false;
        bool low_body_active = false, tickle_active = false;
        bool skin_active = false;
        bool smart_warm_active = false;
        bool body_side_active = false, upper_body_active = false;
        SaturationRuntime bass, warm, presence, air, side_air;
        SaturationRuntime mid_anchor, low_body, tickle, skin;
        double bass_wet = 0.0, warm_wet = 0.0, presence_wet = 0.0;
        double air_wet = 0.0, side_air_wet = 0.0, mid_wet = 0.0;
        double low_body_wet = 0.0, tickle_wet = 0.0, skin_wet = 0.0;
        // Smart Warm Core v4.2: stable Core/Living Body foundations plus a
        // very small guarded 280-820 Hz Upper Body lane for guitar, piano and
        // vocal warmth. Protection never multiplies a foundation.
        double core_foundation_wet = 0.0;
        double core_extension_wet = 0.0;
        double body_foundation_wet = 0.0;
        double body_extension_wet = 0.0;
        double body_side_mix = 0.0;
        double upper_body_foundation_wet = 0.0;
        double upper_body_extension_wet = 0.0;
        double upper_body_side_mix = 0.0;
        double vocal_body_companion_wet = 0.0;
        double male_vocal_body_wet = 0.0;
        double warm_core_guard = 0.0;
        double warm_box_guard = 0.0;
        double warm_upper_guard = 0.0;
        double warm_transient_guard = 0.0;
        double warm_particle_wet = 0.0;
        // P5 Smart Low/Mid-Low Polish. This is a residue-only particle sourced
        // from the existing 140-300 Hz body plus the narrow approximately 240-360 Hz density
        // detector, so no new biquad lane or static bass boost is required.
        double lowmid_particle_wet = 0.0;
        double lowmid_particle_side_mix = 0.0;
        // P4 Stereo-Aware Harmonic Context. Each particle family has a stable
        // polish floor plus a guarded extension. Side particles are derived
        // only from existing band-limited Side content and remain below Mid.
        double warm_particle_side_mix = 0.0;
        double presence_side_mix = 0.0;
        double tickle_side_mix = 0.0;
        double air_side_mix = 0.0;
        double skin_side_mix = 0.0;
        double presence_floor = 0.0;
        double tickle_floor = 0.0;
        double air_floor = 0.0;
        double skin_floor = 0.0;
        double high_guard_depth = 0.0;
        double compensation = 1.0;
        double dry_keep = 1.0;
    };

    struct WidthRuntime {
        bool active = false;
        bool narrow = false;
        bool gen_lowmid_active = false;
        bool gen_mid_active = false;
        bool gen_air_active = false;
        double side_scale = 1.0;
        double width_mix = 0.0;
        double source_protect = 0.0;
        double body_base = 0.0, mid_base = 0.0, high_base = 0.0, air_base = 0.0;
        double gen_lowmid_base = 0.0, gen_mid_base = 0.0, gen_air_base = 0.0;
        SaturationRuntime side_air;
    };

    void load_working_preset(const Preset& preset);
    void rebuild_from_preset();
    void rebuild_eq();
    void rebuild_color_filters();
    void rebuild_width_filters();
    void rebuild_color_runtime();
    void rebuild_width_runtime();
    void apply_macros();
    void update_scene_controls() noexcept;
    void sanitize_filter_states() noexcept;
    std::pair<float,float> process_color(float l, float r);
    std::pair<float,float> process_width(float l, float r);
    static SaturationRuntime make_saturation_runtime(double drive);
    float soft_saturate(float x, const SaturationRuntime& runtime) const;
    float harmonic_residue(float x, const SaturationRuntime& runtime) const;
    void harmonic_residue_pair(float& left, float& right,
                               const SaturationRuntime& runtime) const;
    void soft_saturate_pair(float& left, float& right,
                            const SaturationRuntime& runtime) const;
    float process_channel_eq(std::size_t ch, float x);
    void update_meters(double input_peak, double output_peak,
                       double correlation, double gr_db, std::size_t frames);

    double sample_rate_ = kDefaultSampleRate;
    std::size_t channels_ = 2;
    RuntimeParams params_{};
    const Preset* preset_ = nullptr;
    Preset working_;
    MeterState meters_{};
    AdaptiveSignatureState adaptive_state_{};
#if defined(ASKP_ENGINEERING_TEST_HOOKS)
    bool engineering_male_vocal_body_enabled_ = true;
#endif
    ColorRuntime color_runtime_{};
    WidthRuntime width_runtime_{};

    static constexpr std::size_t kMaxRealtimeEqBands = 64;
    static constexpr std::size_t kMaxRealtimeEqStages = 256;
    std::vector<std::array<Biquad, kMaxChannels>> eq_filters_;

    // Color / psychoacoustic filters. Stereo paths use indexes 0=L, 1=R.
    std::array<Biquad,2> bass_post_hp_;
    std::array<Biquad,2> bass_post_lp_;
    std::array<Biquad,2> warmth_hp_;
    std::array<Biquad,2> warmth_lp_;
    std::array<Biquad,2> upper_body_hp_;
    std::array<Biquad,2> upper_body_lp_;
    Biquad vocal_body_detector_lp_;
    std::array<Biquad,2> presence_pre_;
    std::array<Biquad,2> presence_pre_lp_;
    std::array<Biquad,2> presence_post_hp_;
    std::array<Biquad,2> presence_post_lp_;
    std::array<Biquad,2> air_pre_;
    std::array<Biquad,2> air_pre_lp_;
    std::array<Biquad,2> air_post_hp_;
    std::array<Biquad,2> air_post_lp_;
    std::array<Biquad,2> deharsh_;
    Biquad side_air_hp_;
    Biquad side_air_tone_;
    Biquad mid_anchor_bp1_;
    Biquad mid_anchor_bp2_;
    // Compact high-color bus: Tickle reuses the stereo Presence source and
    // Skin reuses the stereo Air source. One private boundary filter per lane
    // preserves their source focus without four independent biquads each.
    Biquad vocal_tickle_lp_;
    Biquad treble_skin_bp_;
    Biquad low_body_hp_;
    Biquad low_body_lp_;
    Biquad low_body_side_hp_;
    Biquad low_body_side_lp_;
    Biquad warm_box_hp_;
    Biquad warm_box_lp_;
    Biquad warm_particle_dc_hp_;
    Biquad warm_particle_post_lp_;
    // Lightweight first-order HP for the bass Side particle. The 72-150 Hz
    // squared source only needs DC/low-boundary removal, not another biquad.
    double warm_particle_side_hp_coeff_ = 0.0;
    float warm_particle_side_hp_x1_ = 0.0f;
    float warm_particle_side_hp_y1_ = 0.0f;
    // P5 low/mid-low particle uses two first-order HP states. The squared
    // 140-360 Hz source already limits the useful output below about 720 Hz.
    double warm_lowmid_particle_hp_coeff_ = 0.0;
    float warm_lowmid_particle_hp_x1_ = 0.0f;
    float warm_lowmid_particle_hp_y1_ = 0.0f;
    float warm_lowmid_particle_side_hp_x1_ = 0.0f;
    float warm_lowmid_particle_side_hp_y1_ = 0.0f;

    // Generated phase-safe width from mid copy.
    Biquad width_lowmid_hp_;
    Biquad width_lowmid_lp_;
    Biquad width_mid_hp_;
    Biquad width_mid_lp_;
    Biquad width_high_hp_;
    Biquad width_air_tone_;

    // Smart Multiband M/S Imager v2: real-side analysis/enhancement.
    // These filters process the existing Side signal, so musical stereo
    // instruments are preserved while risky low anti-phase energy is reduced.
    Biquad width_side_sub_hp_;
    Biquad width_side_sub_lp_;
    Biquad width_side_body_hp_;
    Biquad width_side_body_lp_;
    Biquad width_side_mid_hp_;
    Biquad width_side_mid_lp_;
    Biquad width_side_high_hp_;
    Biquad width_side_high_lp_;
    Biquad width_side_air_hp_;
    Biquad width_side_air_tone_;

    DynamicsProcessor compressor_;
    LimiterProcessor limiter_;

    OnePoleSmoother bypass_smooth_;
    OnePoleSmoother meter_smooth_;

    // Slow detectors used by Smart Stereo Integrity Engine.
    double width_mid_env_ = 1.0e-6;
    double width_side_env_ = 1.0e-6;
    double width_side_fast_env_ = 1.0e-6;
    double width_side_slow_env_ = 1.0e-6;
    double width_corr_env_ = 1.0;
    double width_corr_dot_accum_ = 0.0;
    double width_corr_l_energy_accum_ = 0.0;
    double width_corr_r_energy_accum_ = 0.0;
    std::uint32_t width_corr_counter_ = 0;
    // P3 Smart Mid-Low Context. These envelopes are fed from the existing
    // 240-1100 Hz generated-Mid and original-Side filter outputs, so the
    // detector adds no extra biquad stages. It distinguishes narrow body,
    // healthy stereo guitars, center-led material and excessive Side energy.
    double width_body_mid_env_ = 1.0e-6;
    double width_body_side_env_ = 1.0e-6;
    double width_projection_mid_env_ = 1.0e-6;
    double width_projection_side_env_ = 1.0e-6;
    double width_body_vitality_ = 0.75;
    double width_projection_vitality_ = 0.75;
    // P3 gains are evaluated at the same 16-sample control rate as correlation.
    // Filter/envelope work remains per sample, but ratios, smoothsteps and all
    // context multipliers are cached to keep the VST lightweight.
    std::uint32_t width_context_counter_ = 0;
    double width_low_reduce_cached_ = 0.0;
    double width_body_gain_cached_ = 0.0;
    double width_mid_gain_cached_ = 0.0;
    double width_high_gain_cached_ = 0.0;
    double width_air_gain_cached_ = 0.0;
    double width_gen_lowmid_gain_cached_ = 0.0;
    double width_gen_mid_gain_cached_ = 0.0;
    double width_gen_air_gain_cached_ = 0.0;
    double width_mid_anchor_gain_cached_ = 1.0;
    double width_slow_alpha_ = 0.0;
    double width_fast_alpha_ = 0.0;
    double width_corr_alpha_ = 0.0;

    // Smart Warm Core v4.2 detector. Fixed biquads observe only local 72-150 Hz
    // core, 140-300 Hz body, 285-430 Hz mud and 280-820 Hz upper-body energy.
    // No high-mid reference, allocation, lock, FFT or lookahead is used.
    double warm_core_fast_env_ = 1.0e-6;   // fast core envelope for particle guard
    double warm_core_slow_env_ = 1.0e-6;   // slow 72-150 Hz core envelope
    double warm_mud_env_ = 1.0e-6;         // 285-430 Hz mud envelope
    double warm_living_body_env_ = 1.0e-6; // 140-300 Hz living-body envelope
    double warm_upper_body_env_ = 1.0e-6;  // 240-1100 Hz guitar/talk-box bridge envelope
    double warm_upper_body_fast_env_ = 1.0e-6;
    double warm_input_env_ = 1.0e-6;
    double warm_core_permission_ = 0.75;
    double warm_body_permission_ = 0.75;
    double warm_upper_permission_ = 0.75;
    double warm_particle_permission_ = 1.0;
    double warm_lowmid_particle_permission_ = 0.75;
    double vocal_body_companion_permission_ = 0.0;
    double male_vocal_body_permission_ = 0.0;
    // P7C/P7D content need for the additive bass saturation lane. The stable
    // linear Core/Living Body foundations are deliberately excluded.
    double adaptive_bass_permission_ = 1.0;
    double warm_core_side_env_ = 1.0e-6;
    double warm_lowmid_side_env_ = 1.0e-6;
    double warm_particle_side_context_ = 0.0;
    double warm_lowmid_side_context_ = 0.0;
    float warm_particle_side_cached_ = 0.0f;
    float warm_lowmid_particle_side_cached_ = 0.0f;
    std::uint32_t warm_control_counter_ = 0;
    double warm_fast_alpha_ = 0.0;
    double warm_slow_alpha_ = 0.0;
    double warm_control_attack_alpha_ = 0.0;
    double warm_control_release_alpha_ = 0.0;
    double warm_particle_attack_alpha_ = 0.0;
    double warm_particle_release_alpha_ = 0.0;
    double vocal_body_companion_attack_alpha_ = 0.0;
    double vocal_body_companion_release_alpha_ = 0.0;
    double vocal_body_companion_reject_alpha_ = 0.0;
    double male_vocal_body_companion_attack_alpha_ = 0.0;
    double male_vocal_body_companion_release_alpha_ = 0.0;
    double male_vocal_body_companion_reject_alpha_ = 0.0;

    // P4 band-local high context. Presence/Tickle and Air/Skin no longer
    // share one permission. Mid/Side envelopes are taken from their existing
    // source buses, so the added context has no extra analysis filters.
    double presence_fast_env_ = 1.0e-6;
    double presence_slow_env_ = 1.0e-6;
    double presence_mid_env_ = 1.0e-6;
    double presence_side_env_ = 1.0e-6;
    double tickle_env_ = 1.0e-6;
    double air_fast_env_ = 1.0e-6;
    double air_slow_env_ = 1.0e-6;
    double air_mid_env_ = 1.0e-6;
    double air_side_env_ = 1.0e-6;
    double skin_env_ = 1.0e-6;
    double high_input_fast_env_ = 1.0e-6;
    double high_input_env_ = 1.0e-6;
    double presence_permission_ = 1.0;
    double tickle_permission_ = 1.0;
    double air_permission_ = 1.0;
    double skin_permission_ = 1.0;
    // Slow P7D vitality scalars sit after the fast protection permissions. They
    // keep the P7A/P7B recipe at unity for normal material and move below it
    // only for sustained spectral excess. Sparse material may receive a very
    // small Mid-focused lift.
    double adaptive_presence_need_ = 1.0;
    double adaptive_tickle_need_ = 1.0;
    double adaptive_air_need_ = 1.0;
    double adaptive_skin_need_ = 1.0;
    // Quiet/sparse passages otherwise excite nonlinear residue much less than
    // a dense chorus. This bounded Mid-only scalar restores consistency without
    // changing the dry foundation, Side image or global gain.
    double low_level_particle_support_ = 1.0;
    double presence_side_context_ = 0.0;
    double air_side_context_ = 0.0;
    bool high_analysis_phase_ = false;
    std::uint32_t high_control_counter_ = 0;
    double high_fast_alpha_ = 0.0;
    double high_slow_alpha_ = 0.0;
    double high_control_attack_alpha_ = 0.0;
    double high_control_release_alpha_ = 0.0;

    // P7D material-profile adaptation. A 32-sample control step feeds slow,
    // hysteretic permission movement: restraint arrives in about 360 ms and
    // the approved Signature vitality returns over about 300 ms. Transient
    // safety remains handled by the existing faster local guards.
    double adaptive_content_reduce_alpha_ = 0.0;
    double adaptive_content_restore_alpha_ = 0.0;
    double low_level_support_reduce_alpha_ = 0.0;
    double adaptive_generated_side_permission_ = 1.0;

    // v0.5.19 preset profiles steer only already-generated Signature material.
    // Targets glide inside the one engine, so a recall never swaps topology
    // or creates a sample edge.
    double scene_detail_target_ = 1.0;
    double scene_articulation_target_ = 1.0;
    double scene_density_target_ = 1.0;
    double scene_body_target_ = 1.0;
    double scene_space_target_ = 1.0;
    double scene_detail_ = 1.0;
    double scene_articulation_ = 1.0;
    double scene_density_ = 1.0;
    double scene_body_ = 1.0;
    double scene_space_ = 1.0;
    double scene_control_alpha_ = 0.0;
    std::uint32_t scene_control_counter_ = 0;

    // P7E Safe Particle Articulation. These bounded, unity-or-higher scalars
    // lift only generated Mid residue during moderate musical articulation.
    // They return to unity on steady, dense or harsh material; original Mid,
    // original Side and every generated Side lane remain untouched.
    double microdetail_presence_articulation_ = 1.0;
    double microdetail_tickle_articulation_ = 1.0;
    double microdetail_air_articulation_ = 1.0;
    double microdetail_skin_articulation_ = 1.0;
    double microdetail_articulation_attack_alpha_ = 0.0;
    double microdetail_articulation_release_alpha_ = 0.0;

    // Smart gain staging reserves clean working headroom before the creative
    // chain. P7B keeps deterministic restore below unity; P7G leaves safe
    // creative lift intact and protects only the minimum Active floor.
    double smart_input_peak_env_ = 1.0e-6;
    double smart_prelim_peak_env_ = 1.0e-6;
    double smart_headroom_db_ = 0.0;
    double smart_makeup_db_ = 0.0;
    double smart_headroom_gain_ = 1.0;
    double smart_makeup_gain_ = 1.0;
    std::uint32_t smart_gain_control_counter_ = 0;
    static constexpr std::uint32_t kFastControlInterval = 16;
};

} // namespace arsonkupik
