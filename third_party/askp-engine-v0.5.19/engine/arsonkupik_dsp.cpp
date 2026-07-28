#include "arsonkupik_dsp.hpp"

#if !defined(ASKP_DSP_HEADER_REVISION) || ASKP_DSP_HEADER_REVISION != 2026072702
 #error "ArSonKuPik DSP source/header mismatch: replace engine/arsonkupik_dsp.hpp and engine/arsonkupik_dsp.cpp from the same package."
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#if defined(_M_X64) || defined(__SSE2__)
 #include <immintrin.h>
 #define ASKP_DSP_HAS_SSE2 1
#else
 #define ASKP_DSP_HAS_SSE2 0
#endif

namespace arsonkupik {
namespace {
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kMinDb = -120.0;
// v0.5.19 release vitality calibration. These tiny multipliers act only on
// already-generated lanes; detail, articulation and body particles retain
// their content guards. They do not alter dry audio, output gain, stable
// bass/body foundations, Side safety or limiter policy.
constexpr double kSignatureDetailVitality = 1.045;
constexpr double kSignatureArticulationVitality = 1.025;
constexpr double kSignatureBodyParticleVitality = 1.012;
constexpr double kSignatureMidAnchorVitality = 1.020;


// High-accuracy [7/6] Padé approximation for tanh on the range used by the
// parallel color lanes. Its maximum absolute error on [-4, +4] is about
// 1.5e-5. The color stage applies this only inside a very small wet path, so
// the resulting full-chain residual remains far below the audible floor while
// avoiding the comparatively expensive libm tanh call for every active lane.
inline float fast_tanh_color(float x) noexcept
{
    x = std::clamp(x, -4.0f, 4.0f);
    const float x2 = x * x;
    const float numerator = 135135.0f
        + x2 * (17325.0f + x2 * (378.0f + x2));
    const float denominator = 135135.0f
        + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2));
    return x * numerator / denominator;
}

// Envelope-normalised even harmonic particle. Squaring a band-limited source
// creates a musically useful second harmonic; the following post high-pass
// removes its DC term. The fixed coefficient keeps the result around the
// subtle mastering range instead of behaving like a full-wave rectifier.
inline float even_harmonic_particle(float x, double envelope) noexcept
{
    const double scale = std::max(2.0e-4, envelope * 2.40);
    const double normalised = std::clamp(static_cast<double>(x) / scale,
                                         -1.15, 1.15);
    return static_cast<float>(normalised * normalised * scale * 0.145);
}

// Smooth harmonic ceiling. The limit is proportional to the source envelope,
// so a stronger input may create more particle but never an unbounded blanket
// that masks transients or makes the programme dull.
inline float harmonic_ceiling(float residue, double envelope,
                              double amplitudeRatio) noexcept
{
    const float limit = static_cast<float>(std::max(1.0e-7,
        envelope * amplitudeRatio));
    // One abs, add and divide. This asymptotically approaches the ceiling and
    // is much cheaper than another waveshaper in every particle lane.
    return residue * (limit / (limit + std::abs(residue)));
}

#if ASKP_DSP_HAS_SSE2
inline __m128 fast_tanh_color_ps(__m128 x) noexcept
{
    const __m128 lo = _mm_set1_ps(-4.0f);
    const __m128 hi = _mm_set1_ps(4.0f);
    x = _mm_min_ps(hi, _mm_max_ps(lo, x));
    const __m128 x2 = _mm_mul_ps(x, x);

    __m128 numerator = _mm_add_ps(_mm_set1_ps(378.0f), x2);
    numerator = _mm_add_ps(_mm_set1_ps(17325.0f),
                           _mm_mul_ps(x2, numerator));
    numerator = _mm_add_ps(_mm_set1_ps(135135.0f),
                           _mm_mul_ps(x2, numerator));

    __m128 denominator = _mm_add_ps(_mm_set1_ps(3150.0f),
        _mm_mul_ps(_mm_set1_ps(28.0f), x2));
    denominator = _mm_add_ps(_mm_set1_ps(62370.0f),
                             _mm_mul_ps(x2, denominator));
    denominator = _mm_add_ps(_mm_set1_ps(135135.0f),
                             _mm_mul_ps(x2, denominator));
    return _mm_div_ps(_mm_mul_ps(x, numerator), denominator);
}
#endif

const std::vector<double>& butterworth_q_values(int slope)
{
    // v0.1.1 (VST port): returns references to static tables so the audio
    // thread never allocates when coefficients are retuned.
    static const std::vector<double> q48{0.50979558, 0.60134489, 0.89997622, 2.56291545};
    static const std::vector<double> q36{0.51763809, 0.70710678, 1.93185165};
    static const std::vector<double> q24{0.54119610, 1.30656296};
    static const std::vector<double> q12{0.70710678};
    switch (slope) {
    case 48: return q48;
    case 36: return q36;
    case 24: return q24;
    default: return q12;
    }
}

float sane(float x)
{
    if (!std::isfinite(x)) return 0.0f;
    return std::clamp(x, -8.0f, 8.0f);
}

double mode_factor(const std::string& mode)
{
    if (mode == "mastering") return 0.88;
    if (mode == "modern")    return 0.84;
    if (mode == "creative")  return 0.86;
    if (mode == "warm")      return 0.72;
    if (mode == "mixbus")    return 0.58;
    if (mode == "track")     return 0.62;
    return 0.52; // clean / unknown: lowest nonlinear intensity
}
} // namespace

double db_to_gain(double db) { return std::pow(10.0, db / 20.0); }
double gain_to_db(double gain) { return gain <= 1e-12 ? kMinDb : 20.0 * std::log10(gain); }
double clamp(double value, double lo, double hi) { return std::max(lo, std::min(hi, value)); }
double clamp01(double value) { return clamp(value, 0.0, 1.0); }

void Biquad::reset()
{
    z1_ = 0.0;
    z2_ = 0.0;
}

void Biquad::set_coefficient_targets(double b0, double b1, double b2,
                                        double a1, double a2, double sampleRate)
{
    target_b0_ = b0;
    target_b1_ = b1;
    target_b2_ = b2;
    target_a1_ = a1;
    target_a2_ = a2;

    const double safeRate = sampleRate > 0.0 ? sampleRate : kDefaultSampleRate;
    coefficient_alpha_ = 1.0 - std::exp(-1.0 / (safeRate * 0.006));

    if (!coefficients_initialised_) {
        b0_ = target_b0_;
        b1_ = target_b1_;
        b2_ = target_b2_;
        a1_ = target_a1_;
        a2_ = target_a2_;
        coefficients_initialised_ = true;
    }

    constexpr double settledEpsilon = 1.0e-12;
    coefficients_settled_ = std::abs(target_b0_ - b0_) <= settledEpsilon
                         && std::abs(target_b1_ - b1_) <= settledEpsilon
                         && std::abs(target_b2_ - b2_) <= settledEpsilon
                         && std::abs(target_a1_ - a1_) <= settledEpsilon
                         && std::abs(target_a2_ - a2_) <= settledEpsilon;
}

void Biquad::set_identity()
{
    set_coefficient_targets(1.0, 0.0, 0.0, 0.0, 0.0, kDefaultSampleRate);
}

void Biquad::set(EqType type, double sample_rate, double frequency, double q,
                 double gain_db, double coefficient_mix)
{
    if (sample_rate <= 0.0 || frequency <= 0.0) {
        set_identity();
        return;
    }
    frequency = clamp(frequency, 8.0, sample_rate * 0.475);
    q = std::max(0.05, q);

    const double w0 = 2.0 * kPi * frequency / sample_rate;
    const double cw = std::cos(w0);
    const double sw = std::sin(w0);
    const double A = std::pow(10.0, gain_db / 40.0);
    double alpha = sw / (2.0 * q);

    double b0, b1, b2, a0, a1, a2;
    switch (type) {
    case EqType::LowCut:
        b0 = (1.0 + cw) / 2.0;
        b1 = -(1.0 + cw);
        b2 = (1.0 + cw) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cw;
        a2 = 1.0 - alpha;
        break;
    case EqType::HighCut:
        b0 = (1.0 - cw) / 2.0;
        b1 = 1.0 - cw;
        b2 = (1.0 - cw) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cw;
        a2 = 1.0 - alpha;
        break;
    case EqType::Bell:
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cw;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cw;
        a2 = 1.0 - alpha / A;
        break;
    case EqType::Notch:
        b0 = 1.0;
        b1 = -2.0 * cw;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cw;
        a2 = 1.0 - alpha;
        break;
    case EqType::LowShelf: {
        const double S = std::max(0.1, q);
        alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1.0) - (A - 1.0) * cw + twoSqrtAAlpha);
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
        b2 = A * ((A + 1.0) - (A - 1.0) * cw - twoSqrtAAlpha);
        a0 = (A + 1.0) + (A - 1.0) * cw + twoSqrtAAlpha;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
        a2 = (A + 1.0) + (A - 1.0) * cw - twoSqrtAAlpha;
        break;
    }
    case EqType::HighShelf: {
        const double S = std::max(0.1, q);
        alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1.0) + (A - 1.0) * cw + twoSqrtAAlpha);
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
        b2 = A * ((A + 1.0) + (A - 1.0) * cw - twoSqrtAAlpha);
        a0 = (A + 1.0) - (A - 1.0) * cw + twoSqrtAAlpha;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
        a2 = (A + 1.0) - (A - 1.0) * cw - twoSqrtAAlpha;
        break;
    }
    case EqType::AllPass:
    default:
        b0 = 1.0 - alpha;
        b1 = -2.0 * cw;
        b2 = 1.0 + alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cw;
        a2 = 1.0 - alpha;
        break;
    }

    // Coefficient morphing gives fixed topology a true unity state.
    // mix=0 is exact identity including phase; mix=1 is the v0.5.4 filter.
    const double coefficientMix = clamp01(coefficient_mix);
    const double fullB0 = b0 / a0;
    const double fullB1 = b1 / a0;
    const double fullB2 = b2 / a0;
    const double fullA1 = a1 / a0;
    const double fullA2 = a2 / a0;
    set_coefficient_targets(
        1.0 + (fullB0 - 1.0) * coefficientMix,
        fullB1 * coefficientMix,
        fullB2 * coefficientMix,
        fullA1 * coefficientMix,
        fullA2 * coefficientMix,
        sample_rate);
}

float Biquad::process(float x)
{
    // Macro values arrive at audio-block boundaries. Interpolate coefficients
    // inside the sample loop so block size and automation speed cannot create
    // a discontinuity in filter output/state. Preset changes still use the
    // separate dual-engine crossfade and therefore never rely on this morph.
    if (!coefficients_settled_) {
        b0_ += coefficient_alpha_ * (target_b0_ - b0_);
        b1_ += coefficient_alpha_ * (target_b1_ - b1_);
        b2_ += coefficient_alpha_ * (target_b2_ - b2_);
        a1_ += coefficient_alpha_ * (target_a1_ - a1_);
        a2_ += coefficient_alpha_ * (target_a2_ - a2_);

        // After the state-preserving morph is numerically complete, snap an
        // inaudible sub-ppm residual to the target. Steady playback then avoids
        // five coefficient interpolation operations per filter per sample.
        constexpr double settledEpsilon = 1.0e-7;
        if (std::abs(target_b0_ - b0_) <= settledEpsilon
            && std::abs(target_b1_ - b1_) <= settledEpsilon
            && std::abs(target_b2_ - b2_) <= settledEpsilon
            && std::abs(target_a1_ - a1_) <= settledEpsilon
            && std::abs(target_a2_ - a2_) <= settledEpsilon) {
            b0_ = target_b0_;
            b1_ = target_b1_;
            b2_ = target_b2_;
            a1_ = target_a1_;
            a2_ = target_a2_;
            coefficients_settled_ = true;
        }
    }

    const double y = b0_ * x + z1_;
    z1_ = b1_ * x - a1_ * y + z2_;
    z2_ = b2_ * x - a2_ * y;
#if !defined(NDEBUG)
    if (!std::isfinite(z1_)) z1_ = 0.0;
    if (!std::isfinite(z2_)) z2_ = 0.0;
    return sane(static_cast<float>(y));
#else
    return std::clamp(static_cast<float>(y), -8.0f, 8.0f);
#endif
}

bool Biquad::sanitize_state() noexcept
{
    const bool valid = std::isfinite(z1_) && std::isfinite(z2_)
        && std::isfinite(b0_) && std::isfinite(b1_) && std::isfinite(b2_)
        && std::isfinite(a1_) && std::isfinite(a2_)
        && std::isfinite(target_b0_) && std::isfinite(target_b1_)
        && std::isfinite(target_b2_) && std::isfinite(target_a1_)
        && std::isfinite(target_a2_);
    if (!valid) {
        set_identity();
        reset();
    }
    return valid;
}

void OnePoleSmoother::prepare(double sample_rate, double seconds, double initial)
{
    seconds = std::max(0.001, seconds);
    alpha_ = 1.0 - std::exp(-1.0 / (sample_rate * seconds));
    state_ = initial;
}

double OnePoleSmoother::process(double target)
{
    state_ += alpha_ * (target - state_);
    return state_;
}

void OnePoleSmoother::reset(double value) { state_ = value; }

void DynamicsProcessor::prepare(double sample_rate, const CompressorConfig& cfg)
{
    sample_rate_ = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
    cfg_ = cfg;
    update_time_constants();
    reset();
}

void DynamicsProcessor::set_config(const CompressorConfig& cfg)
{
    cfg_ = cfg;
    update_time_constants();
}

void DynamicsProcessor::update_time_constants()
{
    attack_coeff_ = std::exp(-1.0 / (sample_rate_ * std::max(0.001, cfg_.attack_sec)));
    release_coeff_ = std::exp(-1.0 / (sample_rate_ * std::max(0.001, cfg_.release_sec)));
    const double half_knee = std::max(0.0, cfg_.knee_db) * 0.5;
    knee_start_gain_ = db_to_gain(cfg_.threshold_db - half_knee);
    ratio_slope_ = 1.0 / std::max(1.0, cfg_.ratio) - 1.0;
    makeup_gain_ = db_to_gain(cfg_.makeup_gain_db);
    wet_mix_ = clamp01(cfg_.parallel_mix / 100.0);
    dry_mix_ = 1.0 - wet_mix_;
}

void DynamicsProcessor::reset() { env_ = 0.0; last_gr_db_ = 0.0; }

std::pair<float,float> DynamicsProcessor::process(float l, float r)
{
    if (!cfg_.enabled) return {l, r};
    const double peak = std::max(std::abs(l), std::abs(r));
    const double coeff = peak > env_ ? attack_coeff_ : release_coeff_;
    env_ = coeff * env_ + (1.0 - coeff) * peak;
    double gr_db = 0.0;
    // The lower half of the compressor curve is exact unity. Test it in the
    // linear domain so quiet samples avoid both log10() and pow().
    if (env_ > knee_start_gain_) {
        const double in_db = gain_to_db(env_ + 1e-12);
        const double over = in_db - cfg_.threshold_db;
        if (cfg_.knee_db > 0.0) {
            const double half_knee = cfg_.knee_db * 0.5;
            if (over < half_knee) {
                const double x = over + half_knee;
                const double compressed = x * x / (2.0 * cfg_.knee_db);
                gr_db = compressed * ratio_slope_;
            } else {
                gr_db = over * ratio_slope_;
            }
        } else if (over > 0.0) {
            gr_db = over * ratio_slope_;
        }
    }
    last_gr_db_ = std::abs(std::min(0.0, gr_db));
    const double wet_gain = gr_db < -1.0e-12
                          ? makeup_gain_ * db_to_gain(gr_db)
                          : makeup_gain_;

    // Dry and compressed paths are strongly correlated. An equal-power
    // crossfade adds close to +3 dB around the middle even when no compression
    // is happening, which made the rack unnecessarily hot before the colour
    // and stereo stages. Use a unity-preserving linear parallel blend instead.
    return {sane(static_cast<float>(l * dry_mix_ + l * wet_gain * wet_mix_)),
            sane(static_cast<float>(r * dry_mix_ + r * wet_gain * wet_mix_))};
}

void LimiterProcessor::prepare(double sample_rate, const OutputConfig& cfg)
{
    sample_rate_ = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
    cfg_ = cfg;
    update_constants();
    reset();
}

void LimiterProcessor::set_config(const OutputConfig& cfg)
{
    cfg_ = cfg;
    update_constants();
}

void LimiterProcessor::update_constants()
{
    const double drive_db = clamp(cfg_.limiter_drive, 0.0, 1.0) * 0.45;
    drive_gain_ = db_to_gain(drive_db);
    release_alpha_ = 1.0 - std::exp(
        -1.0 / (sample_rate_ * (cfg_.punch_protect ? 0.120 : 0.075)));
    ceiling_gain_ = db_to_gain(cfg_.limiter_ceiling_db);
}

void LimiterProcessor::reset() { env_ = 0.0; last_gr_db_ = 0.0; }

std::pair<float,float> LimiterProcessor::process(float l, float r)
{
    if (!cfg_.limiter_enabled) return {l, r};

    // Clean sample-peak safety. The old version drove a tanh clipper before a
    // slow-attack envelope, so hot transients were audibly flattened even when
    // the final sample never exceeded 0 dBFS. Keep the small loudness nudge, but
    // use instantaneous linked attack and a smooth release. This guarantees the
    // current sample is attenuated before it leaves the engine without adding a
    // nonlinear clipping layer.
    l = sane(static_cast<float>(l * drive_gain_));
    r = sane(static_cast<float>(r * drive_gain_));

    const double peak = std::max(std::abs(l), std::abs(r));
    if (peak > env_)
        env_ = peak; // instantaneous attack: protect this sample, not the next one
    else
        env_ += release_alpha_ * (peak - env_);

    const double gain = env_ > ceiling_gain_ ? ceiling_gain_ / (env_ + 1e-12) : 1.0;
    last_gr_db_ = gain < 0.999999999 ? -gain_to_db(gain) : 0.0;
    return {sane(static_cast<float>(l * gain)), sane(static_cast<float>(r * gain))};
}

ArSonKuPikEngine::ArSonKuPikEngine()
{
    params_.preset_id.reserve(64);
    working_.eq.reserve(kMaxRealtimeEqBands);
    eq_filters_.reserve(kMaxRealtimeEqStages);
    preset_ = &default_preset();
    load_working_preset(*preset_);
}

void ArSonKuPikEngine::load_working_preset(const Preset& preset)
{
    // Only processing data is copied. Human-readable preset/EQ identifiers are
    // intentionally omitted so preset switches never allocate std::strings in
    // the audio callback.
    working_.eq.resize(preset.eq.size());
    for (std::size_t i = 0; i < preset.eq.size(); ++i) {
        const auto& source = preset.eq[i];
        auto& target = working_.eq[i];
        target.id.clear();
        target.type = source.type;
        target.frequency = source.frequency;
        target.gain_db = source.gain_db;
        target.q = source.q;
        target.slope_db_per_oct = source.slope_db_per_oct;
        target.enabled = source.enabled;
        target.coefficient_mix = source.coefficient_mix;
    }
    working_.compressor = preset.compressor;
    working_.color = preset.color;
    working_.width = preset.width;
    working_.output = preset.output;
    working_.macro = preset.macro;
    working_.policy = preset.policy;
}

void ArSonKuPikEngine::prepare(double sample_rate, std::size_t channels)
{
    sample_rate_ = sample_rate > 0.0 ? sample_rate : kDefaultSampleRate;
    channels_ = std::max<std::size_t>(1, std::min<std::size_t>(channels, kMaxChannels));
    width_slow_alpha_ = 1.0 - std::exp(-1.0 / (sample_rate_ * 0.055));
    width_fast_alpha_ = 1.0 - std::exp(-1.0 / (sample_rate_ * 0.006));
    // Correlation is measured over a short 16-sample window. This is more
    // meaningful than the old instantaneous sign-like correlation and reduces
    // sqrt/normalisation work from every sample to control rate.
    width_corr_alpha_ = 1.0 - std::exp(
        -static_cast<double>(kFastControlInterval) / (sample_rate_ * 0.090));
    warm_fast_alpha_ = 1.0 - std::exp(-1.0 / (sample_rate_ * 0.006));
    warm_slow_alpha_ = 1.0 - std::exp(-1.0 / (sample_rate_ * 0.180));
    // Permission is updated once per 32 samples. Risk reduction is deliberate
    // but slow enough to avoid tonal pumping; restoration is much slower.
    warm_control_attack_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.170));
    warm_control_release_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 1.050));
    warm_particle_attack_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.024));
    warm_particle_release_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.260));
    vocal_body_companion_attack_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.045));
    vocal_body_companion_release_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.220));
    vocal_body_companion_reject_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.075));
    // P1B male body follows complete chest/throat phrases rather than syllable
    // edges. Rejection is faster than release so kick, bass and dense guitars do
    // not leave a warm low-mid tail after the detector loses vocal confidence.
    male_vocal_body_companion_attack_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.075));
    male_vocal_body_companion_release_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.300));
    male_vocal_body_companion_reject_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.090));
    // P4 high context runs every second sample. Coefficients use the true
    // two-sample interval, preserving the same detector time constants.
    high_fast_alpha_ = 1.0 - std::exp(-2.0 / (sample_rate_ * 0.004));
    high_slow_alpha_ = 1.0 - std::exp(-2.0 / (sample_rate_ * 0.110));
    warm_particle_side_hp_coeff_ = std::exp(-2.0 * kPi * 150.0 / sample_rate_);
    warm_lowmid_particle_hp_coeff_ = std::exp(-2.0 * kPi * 250.0 / sample_rate_);
    high_control_attack_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.020));
    high_control_release_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.260));
    // P7D: the fast P4 guards already catch transient/harsh events. The slow
    // material profile therefore restrains only sustained excess and returns
    // to the approved P7A/P7B vitality without a long dull hangover.
    adaptive_content_reduce_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.360));
    adaptive_content_restore_alpha_ = 1.0 - std::exp(-32.0 / (sample_rate_ * 0.300));
    low_level_support_reduce_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.045));
    // P7E follows musical articulation quickly enough to restore perceived
    // particle motion, then releases smoothly to avoid a bright pumping tail.
    microdetail_articulation_attack_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.018));
    microdetail_articulation_release_alpha_ =
        1.0 - std::exp(-32.0 / (sample_rate_ * 0.140));
    // Five scalar scene dimensions move at the existing 16-sample control
    // cadence. This is much cheaper than a parallel engine/crossfade and keeps
    // factory or user preset recalls click-free.
    scene_control_alpha_ = 1.0 - std::exp(
        -static_cast<double>(kFastControlInterval) / (sample_rate_ * 0.120));
    bypass_smooth_.prepare(sample_rate_, 0.075, params_.bypass ? 1.0 : 0.0);
    meter_smooth_.prepare(sample_rate_, 0.180, 0.0);
    compressor_.prepare(sample_rate_, working_.compressor);
    limiter_.prepare(sample_rate_, working_.output);
    rebuild_from_preset();
}

void ArSonKuPikEngine::set_runtime_params(const RuntimeParams& params)
{
    const auto& presets = factory_presets();
    const Preset* requestedPreset = nullptr;
    if (params.preset_index >= 0 && params.preset_index < (int)presets.size())
        requestedPreset = &presets[(std::size_t)params.preset_index];
    else
        requestedPreset = find_preset(params.preset_id);
    if (requestedPreset == nullptr)
        requestedPreset = &default_preset();

    // Utility trim and bypass are consumed directly in process(). Preserve the
    // original sonic mapping, but do not rebuild the creative rack for a pure
    // utility move.
    const bool creativeChanged = requestedPreset != preset_
        || params.advanced_override != params_.advanced_override
        || params.smart_protect != params_.smart_protect
        || std::abs(params.enhance - params_.enhance) > 1.0e-4
        || std::abs(params.smart_bass - params_.smart_bass) > 1.0e-4
        || std::abs(params.smart_treble - params_.smart_treble) > 1.0e-4
        || std::abs(params.vocal_body - params_.vocal_body) > 1.0e-4
        || std::abs(params.stereo_magic - params_.stereo_magic) > 1.0e-4;

    scene_detail_target_ = clamp(
        params.detail_focus * kSignatureDetailVitality, 0.82, 1.18);
    scene_articulation_target_ = clamp(
        params.articulation_focus * kSignatureArticulationVitality,
        0.80, 1.20);
    scene_density_target_ = clamp(params.density_tolerance, 0.82, 1.18);
    scene_body_target_ = clamp(
        params.body_focus * kSignatureBodyParticleVitality, 0.84, 1.16);
    scene_space_target_ = clamp(params.space_focus, 0.82, 1.14);

    params_ = params;
    if (!creativeChanged)
        return;

    preset_ = requestedPreset;
    load_working_preset(*preset_);
    apply_macros();
    rebuild_from_preset();
}

void ArSonKuPikEngine::set_output_trim_db(double db) noexcept
{
    params_.output_trim_db = db;
}

void ArSonKuPikEngine::reset()
{
    for (auto& chs : eq_filters_) for (auto& b : chs) b.reset();
    for (auto& b : bass_post_hp_) b.reset();
    for (auto& b : bass_post_lp_) b.reset();
    for (auto& b : warmth_hp_) b.reset();
    for (auto& b : warmth_lp_) b.reset();
    for (auto& b : upper_body_hp_) b.reset();
    for (auto& b : upper_body_lp_) b.reset();
    vocal_body_detector_lp_.reset();
    for (auto& b : presence_pre_) b.reset();
    for (auto& b : presence_pre_lp_) b.reset();
    for (auto& b : presence_post_hp_) b.reset();
    for (auto& b : presence_post_lp_) b.reset();
    for (auto& b : air_pre_) b.reset();
    for (auto& b : air_pre_lp_) b.reset();
    for (auto& b : air_post_hp_) b.reset();
    for (auto& b : air_post_lp_) b.reset();
    for (auto& b : deharsh_) b.reset();
    side_air_hp_.reset(); side_air_tone_.reset(); mid_anchor_bp1_.reset(); mid_anchor_bp2_.reset();
    vocal_tickle_lp_.reset();
    treble_skin_bp_.reset();
    low_body_hp_.reset(); low_body_lp_.reset();
    low_body_side_hp_.reset(); low_body_side_lp_.reset();
    warm_box_hp_.reset(); warm_box_lp_.reset();
    warm_particle_dc_hp_.reset(); warm_particle_post_lp_.reset();
    warm_particle_side_hp_x1_ = warm_particle_side_hp_y1_ = 0.0f;
    warm_lowmid_particle_hp_x1_ = warm_lowmid_particle_hp_y1_ = 0.0f;
    warm_lowmid_particle_side_hp_x1_ = warm_lowmid_particle_side_hp_y1_ = 0.0f;
    width_lowmid_hp_.reset(); width_lowmid_lp_.reset(); width_mid_hp_.reset(); width_mid_lp_.reset();
    width_high_hp_.reset(); width_air_tone_.reset();
    width_side_sub_hp_.reset(); width_side_sub_lp_.reset(); width_side_body_hp_.reset(); width_side_body_lp_.reset();
    width_side_mid_hp_.reset(); width_side_mid_lp_.reset(); width_side_high_hp_.reset(); width_side_high_lp_.reset();
    width_side_air_hp_.reset(); width_side_air_tone_.reset();
    width_mid_env_ = 1.0e-6; width_side_env_ = 1.0e-6;
    width_side_fast_env_ = 1.0e-6; width_side_slow_env_ = 1.0e-6;
    width_corr_env_ = 1.0; width_corr_dot_accum_ = 0.0;
    width_corr_l_energy_accum_ = 0.0; width_corr_r_energy_accum_ = 0.0;
    width_corr_counter_ = 0;
    width_body_mid_env_ = 1.0e-6; width_body_side_env_ = 1.0e-6;
    width_projection_mid_env_ = 1.0e-6; width_projection_side_env_ = 1.0e-6;
    width_body_vitality_ = 0.75; width_projection_vitality_ = 0.75;
    width_context_counter_ = 0;
    width_low_reduce_cached_ = 0.0;
    width_body_gain_cached_ = width_mid_gain_cached_ = 0.0;
    width_high_gain_cached_ = width_air_gain_cached_ = 0.0;
    width_gen_lowmid_gain_cached_ = width_gen_mid_gain_cached_ = 0.0;
    width_gen_air_gain_cached_ = 0.0;
    width_mid_anchor_gain_cached_ = 1.0;
    warm_core_fast_env_ = 1.0e-6;
    warm_core_slow_env_ = 1.0e-6;
    warm_mud_env_ = 1.0e-6;
    warm_living_body_env_ = 1.0e-6;
    warm_upper_body_env_ = 1.0e-6;
    warm_upper_body_fast_env_ = 1.0e-6;
    warm_input_env_ = 1.0e-6;
    warm_core_permission_ = 0.75;
    warm_body_permission_ = 0.75;
    warm_upper_permission_ = 0.75;
    warm_particle_permission_ = 1.0;
    warm_lowmid_particle_permission_ = 0.75;
    vocal_body_companion_permission_ = 0.0;
    male_vocal_body_permission_ = 0.0;
    adaptive_bass_permission_ = 1.0;
    warm_control_counter_ = 0;
    warm_core_side_env_ = 1.0e-6;
    warm_lowmid_side_env_ = 1.0e-6;
    warm_particle_side_context_ = 0.0;
    warm_lowmid_side_context_ = 0.0;
    warm_particle_side_cached_ = 0.0f;
    warm_lowmid_particle_side_cached_ = 0.0f;
    presence_fast_env_ = presence_slow_env_ = 1.0e-6;
    presence_mid_env_ = presence_side_env_ = 1.0e-6;
    tickle_env_ = 1.0e-6;
    air_fast_env_ = air_slow_env_ = 1.0e-6;
    air_mid_env_ = air_side_env_ = 1.0e-6;
    skin_env_ = 1.0e-6;
    high_input_fast_env_ = 1.0e-6;
    high_input_env_ = 1.0e-6;
    presence_permission_ = tickle_permission_ = 1.0;
    air_permission_ = skin_permission_ = 1.0;
    adaptive_presence_need_ = adaptive_tickle_need_ = 1.0;
    adaptive_air_need_ = adaptive_skin_need_ = 1.0;
    low_level_particle_support_ = 1.0;
    adaptive_generated_side_permission_ = 1.0;
    scene_detail_ = scene_detail_target_;
    scene_articulation_ = scene_articulation_target_;
    scene_density_ = scene_density_target_;
    scene_body_ = scene_body_target_;
    scene_space_ = scene_space_target_;
    scene_control_counter_ = 0;
    microdetail_presence_articulation_ = microdetail_tickle_articulation_ = 1.0;
    microdetail_air_articulation_ = microdetail_skin_articulation_ = 1.0;
    adaptive_state_ = {};
    presence_side_context_ = air_side_context_ = 0.0;
    high_analysis_phase_ = false;
    high_control_counter_ = 0;
    smart_input_peak_env_ = 1.0e-6;
    smart_prelim_peak_env_ = 1.0e-6;
    smart_headroom_db_ = 0.0;
    smart_makeup_db_ = 0.0;
    smart_headroom_gain_ = 1.0;
    smart_makeup_gain_ = 1.0;
    smart_gain_control_counter_ = 0;

    // Deterministic musical presets use content-independent reserve and restore
    // values. Initialising those values here prevents every standby
    // engine from starting several dB quiet and climbing for ~90 ms after the
    // crossfade has already completed.
    const bool stableMusicalDynamics = preset_->policy.deterministic_level;
    if (stableMusicalDynamics) {
        smart_headroom_db_ = preset_->policy.fixed_headroom_db;
        const double stableRestore = preset_->policy.fixed_restore_ratio;
        smart_makeup_db_ = clamp((-smart_headroom_db_) * stableRestore,
                                 0.0, working_.policy.max_smart_makeup_db);
        smart_headroom_gain_ = db_to_gain(smart_headroom_db_);
        smart_makeup_gain_ = db_to_gain(smart_makeup_db_);
    }

    compressor_.reset(); limiter_.reset();
}

void ArSonKuPikEngine::update_scene_controls() noexcept
{
    if (++scene_control_counter_ < kFastControlInterval)
        return;

    scene_control_counter_ = 0;
    auto move = [this](double& current, double target)
    {
        current += scene_control_alpha_ * (target - current);
    };
    move(scene_detail_, scene_detail_target_);
    move(scene_articulation_, scene_articulation_target_);
    move(scene_density_, scene_density_target_);
    move(scene_body_, scene_body_target_);
    move(scene_space_, scene_space_target_);
}

void ArSonKuPikEngine::apply_macros()
{
    // Hybrid Macro-to-Micro Parameter Mapping Engine
    // ------------------------------------------------
    // 1) Sectoral knobs are center-based: 50% = RAW / unity, left = cleaning,
    //    right = enhancement. Their macro values drive many internal micro
    //    controls through shaped curves rather than linear one-parameter moves.
    // 2) Additive knobs are left-based: 0% = pure clean/bypass for that macro,
    //    right = progressively enables compression, harmonics, EQ and drive.

    const double ui_enhance = params_.advanced_override ? params_.enhance : working_.macro.enhance;
    const double ui_bass    = params_.advanced_override ? params_.smart_bass : working_.macro.smart_bass;
    const double ui_treble  = params_.advanced_override ? params_.smart_treble : working_.macro.smart_treble;
    const double ui_vocal   = params_.advanced_override ? params_.vocal_body : working_.macro.vocal_body;
    const double ui_stereo  = params_.advanced_override ? params_.stereo_magic : working_.macro.stereo_magic;

    auto exp01 = [](double x, double curve) { return std::pow(clamp01(x), curve); };
    auto bipolar = [&](double percent, double curve)
    {
        const double x = clamp01(percent / 100.0);
        const double signedLinear = (x - 0.5) * 2.0;
        return signedLinear >= 0.0 ?  exp01( signedLinear, curve)
                                   : -exp01(-signedLinear, curve);
    };
    auto addBand = [&](const char*, EqType type, double freq, double gain, double q,
                       int slope = 12, bool enabled = true, double coefficientMix = 1.0)
    {
        EqBand b;
        b.type = type;
        b.frequency = freq;
        b.gain_db = gain;
        b.q = q;
        b.slope_db_per_oct = slope;
        b.enabled = enabled;
        b.coefficient_mix = coefficientMix;
        working_.eq.push_back(std::move(b));
    };

    const double enhance = exp01(ui_enhance / 100.0, 1.12); // left-based additive

    const double bassBip   = bipolar(ui_bass,   1.42);
    const double trebleBip = bipolar(ui_treble, 1.42);
    const double vocalBip  = bipolar(ui_vocal,  1.22);
    const double stereoBip = bipolar(ui_stereo, 1.25);

    const double bassBoost   = std::max(0.0,  bassBip);
    const double bassClean   = std::max(0.0, -bassBip);
    const double trebleBoost = std::max(0.0,  trebleBip);
    const double trebleClean = std::max(0.0, -trebleBip);

    // Preserve the approved v0.5.4 character around normal preset positions,
    // but give the final quarter of the knobs a clearly stronger endpoint.
    auto endpointLift = [](double amount)
    {
        const double x = clamp01((amount - 0.55) / 0.45);
        return x * x * (3.0 - 2.0 * x);
    };
    const double bassEndpoint = endpointLift(bassBoost);
    const double trebleEndpoint = endpointLift(trebleBoost);
    const double vocal       = std::max(0.0,  vocalBip);
    const double vocalTuck   = std::max(0.0, -vocalBip);
    const double stereoWide  = std::max(0.0,  stereoBip);
    const double stereoNarrow= std::max(0.0, -stereoBip);
    const double masari      = clamp01(working_.macro.masari_feel / 100.0);

    // Additive engine activation: 0 means raw/pure clean for each additive macro.
    const double colorActivation = clamp01(enhance * 0.72 + vocal * 0.55 + vocalTuck * 0.30
                                         + bassBoost * 0.30 + trebleBoost * 0.30
                                         + trebleClean * 0.18);
    working_.color.enabled = colorActivation > 0.006 || bassClean > 0.015 || trebleClean > 0.015 || vocalTuck > 0.015;
    working_.color.mix = clamp(18.0 * vocal + 30.0 * enhance + 7.0 * vocalTuck + 8.0 * bassBoost + 12.0 * trebleBoost,
                               0.0, 54.0);

    // Smart Bass: right = sub lift / low saturation / body, left = mud clean / smart low-cut.
    working_.color.smart_bass = clamp(100.0 * bassBoost, 0.0, 100.0);
    working_.color.body = clamp(20.5 * bassBoost + 8.5 * bassEndpoint
                                + 7.5 * vocal + 3.0 * enhance, 0.0, 44.0);
    working_.color.warmth = clamp(12.5 * bassBoost + 5.5 * bassEndpoint
                                  + 5.5 * vocal + 2.0 * enhance, 0.0, 34.0);
    // Smart Warm Particles is intentionally strongest when Enhance/Vocal ask
    // for detail, while a strong guard remains available for bass-clean and
    // dense mixes. It does not blindly boost the entire 200-800 Hz region.
    working_.color.warm_particles = clamp(24.0 + 60.0 * enhance
                                           + 30.0 * vocal
                                           + 18.0 * bassBoost,
                                           0.0, 100.0);
    working_.color.warm_box_guard = clamp(64.0 + 18.0 * enhance
                                           + 12.0 * vocal
                                           + 12.0 * bassClean,
                                           58.0, 96.0);
    working_.color.drive = clamp(0.10 + 1.20 * enhance + 1.05 * bassBoost
                                 + 0.22 * bassEndpoint + 0.42 * vocal, 0.0, 3.55);

    // Fixed macro EQ topology keeps stage count constant while knobs move.
    // This prevents rebuild_eq() from reallocating/resetting the complete EQ
    // history at neutral crossings. Inactive Bell/Shelf stages approach 0 dB;
    // inactive low-cut stages morph from exact identity and engage progressively.
    auto topologyActivation = [](double amount)
    {
        const double x = clamp01(amount / 0.025);
        return x * x * (3.0 - 2.0 * x);
    };

    const double bassBoostActive = topologyActivation(bassBoost);
    const double bassCleanActive = topologyActivation(bassClean);
    const double trebleBoostActive = topologyActivation(trebleBoost);
    const double trebleCleanActive = topologyActivation(trebleClean);
    const double vocalActive = topologyActivation(vocal);
    const double vocalTuckActive = topologyActivation(vocalTuck);

    // Smart Bass: right = sub lift / low saturation / body, left = mud clean / smart low-cut.
    // Keep a small linear reference foundation below the low mids. Full-band
    // profile-dependent spectral balance can otherwise let detail-heavy profiles land
    // a few tenths below true bypass around 100-140 Hz even though the isolated
    // engine curve looks bass-forward. The floor is strongest near neutral
    // Smart Bass and recedes as the dedicated bass lane takes over, so Bass
    // Authority is not inflated and no content detector can pump the balance.
    const double bassReferenceFloorDb =
        0.70 * (1.0 - 0.70 * bassBoost)
        * (1.0 - bassCleanActive);
    addBand("macro_bass_reference_floor", EqType::LowShelf, 178.0,
            bassReferenceFloorDb, 0.62);
    addBand("macro_sub_weight", EqType::LowShelf, 58.0 + 12.0 * bassBoost,
            (0.40 + 4.45 * bassBoost + 1.55 * bassEndpoint) * bassBoostActive, 0.64);
    addBand("macro_punch_body", EqType::Bell, 104.0 + 20.0 * bassBoost,
            (0.22 + 2.65 * bassBoost + 0.90 * bassEndpoint) * bassBoostActive, 0.82);

    // Reproduce the v0.5.4 Smart Bass clean lane with fixed allocation.
    // Signature's positive Smart Bass keeps both clean-lane stages exact identity.
    const double lowCutTargetHz = 22.0 + 62.0 * bassClean;
    const double lowCutSecondActive = topologyActivation(
        clamp01((bassClean - 0.55) / 0.10));
    const double lowCutFirstQ = 0.70710678
        + (0.54119610 - 0.70710678) * lowCutSecondActive;
    addBand("macro_smart_low_cut_1", EqType::LowCut, lowCutTargetHz,
            0.0, lowCutFirstQ, 12, true, bassCleanActive);
    addBand("macro_smart_low_cut_2", EqType::LowCut, lowCutTargetHz,
            0.0, 1.30656296, 12, true,
            bassCleanActive * lowCutSecondActive);
    addBand("macro_mud_reduction", EqType::Bell, 185.0 + 95.0 * bassClean,
            (-0.55 - 4.60 * bassClean) * bassCleanActive, 0.82);
    addBand("macro_low_mid_cleanup", EqType::Bell, 380.0 + 160.0 * bassClean,
            (-0.15 - 2.20 * bassClean) * bassCleanActive, 0.95);

    // Smart Treble: right = air/transient sparkle, left = harshness/de-essing clean.
    working_.color.air = clamp(108.0 * trebleBoost + 24.0 * trebleEndpoint
                               - 10.0 * trebleClean, -12.0, 136.0);
    working_.color.god_particles = clamp(100.0 * trebleBoost + 12.0 * enhance, 0.0, 100.0);
    working_.color.vocal_tickle = clamp(30.0 * trebleBoost + 12.0 * trebleEndpoint
                                        + 54.0 * vocal + 8.0 * enhance, 0.0, 112.0);
    working_.color.velvet_treble = clamp(18.0 + 80.0 * trebleClean + 18.0 * trebleBoost, 0.0, 100.0);
    working_.color.ai_high_repair = clamp(20.0 + 76.0 * trebleClean, 0.0, 100.0);
    addBand("macro_air_open", EqType::HighShelf, 8400.0 + 2600.0 * trebleBoost,
            (0.55 + 6.40 * trebleBoost + 1.75 * trebleEndpoint) * trebleBoostActive, 0.56);
    addBand("macro_presence_light", EqType::Bell, 3100.0 + 600.0 * trebleBoost,
            (0.18 + 2.25 * trebleBoost + 0.95 * trebleEndpoint) * trebleBoostActive, 0.90);
    addBand("macro_de_ess", EqType::Bell, 5600.0 + 1700.0 * trebleClean,
            (-0.80 - 5.20 * trebleClean) * trebleCleanActive, 1.45);
    addBand("macro_harsh_smooth", EqType::Bell, 9200.0,
            (-0.35 - 2.50 * trebleClean) * trebleCleanActive, 0.78);

    // Vocal: center-based. 50% = neutral. Right = forward/anti-masking,
    // left = tucked and smoother. Both lanes stay allocated at unity.
    working_.color.vocal_presence = clamp(100.0 * vocal + 8.0 * enhance, 0.0, 100.0);
    working_.color.mid_projection = clamp(100.0 * vocal + 14.0 * enhance, 0.0, 100.0);
    working_.color.stereo_mid = clamp(50.0 + 45.0 * vocal - 24.0 * vocalTuck, 0.0, 100.0);
    addBand("macro_vocal_presence", EqType::Bell, 1450.0 + 380.0 * vocal,
            (0.45 + 4.30 * vocal) * vocalActive, 0.80);
    addBand("macro_anti_masking", EqType::Bell, 330.0,
            (-0.02 - 0.09 * vocal) * vocalActive, 1.12);
    const bool flagshipWarmCalibration = preset_ != nullptr
                                      && preset_->id == "default";
    // Body Density P2: a tiny Signature-only vocal chest increment. Most
    // additional power-chord density is added after compression below.
    const double vocalBodyBase = flagshipWarmCalibration ? 0.20 : 0.14;
    const double vocalBodyRange = flagshipWarmCalibration ? 1.04 : 0.78;
    addBand("macro_vocal_body_support", EqType::Bell, 220.0,
            (vocalBodyBase + vocalBodyRange * vocal) * vocalActive, 0.92);
    addBand("macro_vocal_clarity", EqType::Bell, 3350.0,
            (0.24 + 2.85 * vocal) * vocalActive, 1.00);
    addBand("macro_vocal_air_focus", EqType::Bell, 5200.0,
            (0.10 + 1.25 * vocal) * vocalActive, 1.18);
    addBand("macro_vocal_tuck_presence", EqType::Bell, 1650.0,
            (-0.45 - 3.45 * vocalTuck) * vocalTuckActive, 0.90);
    addBand("macro_vocal_tuck_clarity", EqType::Bell, 3300.0,
            (-0.28 - 2.10 * vocalTuck) * vocalTuckActive, 1.12);
    addBand("macro_vocal_tuck_body", EqType::Bell, 520.0,
            (-0.10 - 0.85 * vocalTuck) * vocalTuckActive, 0.82);

    // Enhance: left = pure clean, right = parallel harmonic excitation + upward-style density.
    working_.color.harmonics = clamp(72.0 * enhance + 14.0 * trebleBoost, 0.0, 100.0);
    working_.color.mix = clamp(working_.color.mix + enhance * (4.0 + masari * 2.0), 0.0, 48.0);
    working_.compressor.enabled = enhance > 0.006 || vocal > 0.006;
    working_.compressor.threshold_db = clamp(-16.0 - 5.2 * enhance - 2.4 * vocal, -25.5, -11.5);
    working_.compressor.ratio = clamp(1.04 + enhance * 0.42 + vocal * 0.48, 1.0, 2.05);
    working_.compressor.attack_sec = clamp(0.044 - 0.018 * vocal + 0.006 * (1.0 - enhance), 0.014, 0.064);
    working_.compressor.release_sec = clamp(0.155 + 0.045 * enhance + 0.040 * vocal, 0.085, 0.300);
    working_.compressor.parallel_mix = clamp(44.0 * enhance + 26.0 * vocal, 0.0, 74.0);
    working_.compressor.makeup_gain_db = clamp(0.45 + 2.35 * enhance + 2.45 * vocal, 0.0, 6.0);

    // Listening feedback: the most bass-forward musical presets must not let
    // linked compression audibly rock the whole mix. Preserve transient tone,
    // but cap ratio/mix/makeup and use a slower, calmer recovery envelope.
    const bool stableSignature = preset_->id == "default";
    const bool stableDeepChest = preset_->id == "sonku-deep-chest";
    const bool stableDangdut = preset_->id == "dangdut-koplo";
    if (preset_->policy.deterministic_level) {
        // Use only a light density layer. Bass hits must not drive a slow
        // full-band gain envelope that makes the vocal and treble rock afterward.
        const double thresholdFloor = stableDeepChest ? -14.5
                                    : (stableDangdut ? -15.8
                                                     : (stableSignature ? -15.2 : -15.8));
        const double ratioCap = stableDeepChest ? 1.10
                              : (stableDangdut ? 1.18
                                               : (stableSignature ? 1.14 : 1.18));
        const double mixCap = stableDeepChest ? 14.0
                            : (stableDangdut ? 22.0
                                             : (stableSignature ? 18.0 : 22.0));
        const double makeupCap = stableDeepChest ? 0.90
                               : (stableDangdut ? 1.40
                                                : (stableSignature ? 1.20 : 1.40));
        working_.compressor.threshold_db = std::max(working_.compressor.threshold_db, thresholdFloor);
        working_.compressor.ratio = std::min(working_.compressor.ratio, ratioCap);
        working_.compressor.attack_sec = std::max(working_.compressor.attack_sec, 0.050);
        working_.compressor.release_sec = stableDeepChest ? 0.120
                                          : (stableDangdut ? 0.155
                                                           : (stableSignature ? 0.140 : 0.155));
        working_.compressor.parallel_mix = std::min(working_.compressor.parallel_mix, mixCap);
        working_.compressor.makeup_gain_db = std::min(working_.compressor.makeup_gain_db, makeupCap);
    }

    // Stereo: 50 = raw, right widens, left narrows to true mono at 0.
    working_.width.enabled = std::abs(stereoBip) > 0.006;
    if (stereoNarrow > 0.006) {
        const double monoScale = 100.0 * (1.0 - stereoNarrow); // 0% at far left = total mono
        working_.width.mix = 100.0;
        working_.width.width = monoScale;
        working_.width.low_mid_width = monoScale;
        working_.width.mid_width = monoScale;
        working_.width.high_width = monoScale;
        working_.width.source_protect = 100.0;
        working_.width.side_tone = 0.0;
    } else if (stereoWide > 0.006) {
        const double contextCap = clamp(working_.policy.max_width_percent, 100.0, 200.0);
        working_.width.mix = clamp(30.0 + 68.0 * stereoWide, 0.0, 100.0);
        working_.width.width = clamp(100.0 + 100.0 * stereoWide, 100.0, contextCap);
        working_.width.low_mid_width = clamp(100.0 + 22.0 * stereoWide, 100.0,
                                              std::min(135.0, 100.0 + (contextCap - 100.0) * 0.34));
        working_.width.mid_width = clamp(100.0 + 50.0 * stereoWide, 100.0,
                                         std::min(168.0, 100.0 + (contextCap - 100.0) * 0.72));
        working_.width.high_width = clamp(100.0 + 116.0 * stereoWide, 100.0,
                                          std::min(224.0, 100.0 + (contextCap - 100.0) * 1.18));
        working_.width.source_protect = clamp(78.0 - 12.0 * stereoWide, 58.0, 96.0);
        working_.width.side_tone = clamp(0.6 + 3.8 * stereoWide, 0.0, 5.0);
    }

    // P7B separated sonic enhancement from loudness demonstration. The old
    // perceived-benefit formula could add almost +3 dB after EQ, compression,
    // particles and width; a later WOW target and Gain Match then fought that
    // level back down. Keep only a tiny static identity allowance here. P7G's
    // wrapper no longer attenuates safe creative lift; it protects only the
    // Active floor before independent lookahead peak safety.
    const double perceived_benefit_db = enhance * 2.15 + vocal * 1.35
                                      + trebleBoost * 0.36 + bassBoost * 0.16
                                      - vocalTuck * 0.10;
    const double natural_identity_db = clamp(perceived_benefit_db * 0.08,
                                             -0.10, 0.25);
    // Keep user Output Trim out of creative macro mapping. It remains a manual
    // utility after the engine tone stages.
    working_.output.output_gain_db = natural_identity_db;
    working_.output.bypass = params_.bypass;
    working_.output.punch_protect = params_.smart_protect;

    const double headroom = bassBoost * 0.50 + trebleBoost * 0.16 + enhance * 0.12 + vocal * 0.08;
    if (params_.smart_protect) {
        working_.output.output_gain_db -= std::max(0.0, headroom - 0.32);
        working_.output.limiter_enabled = true;
        working_.output.limiter_ceiling_db = std::min(working_.output.limiter_ceiling_db, -1.0 - bassBoost * 0.55);
        // No pre-limiter drive in P7B. Limiting is containment, not enhancement.
        working_.output.limiter_drive = 0.0;
    }
    // Signature reserve: the stronger linear low-mid reaches final safety with
    // 0.10 dB more room. P7G floor protection may restore a weak Active path,
    // but safe creative lift is otherwise left untouched.
    if (flagshipWarmCalibration)
        working_.output.output_gain_db -= 0.10;
}

void ArSonKuPikEngine::rebuild_from_preset()
{
    rebuild_eq();
    rebuild_color_filters();
    rebuild_width_filters();
    compressor_.set_config(working_.compressor);
    limiter_.set_config(working_.output);
}

void ArSonKuPikEngine::rebuild_eq()
{
    // v0.1.1 (VST port): state-preserving rebuild. Reusing existing biquad
    // stages (and their z1/z2 history) removes audible crackle when macro
    // knobs retune the EQ while audio is running. The stage count only
    // changes on preset switches; only then do we reallocate and accept a
    // one-time state reset (masked by a short fade in the plugin wrapper).
    std::size_t total = 0;
    for (const auto& band : working_.eq) {
        if (!band.enabled) continue;
        total += (band.type == EqType::LowCut || band.type == EqType::HighCut)
                     ? butterworth_q_values(band.slope_db_per_oct).size()
                     : 1;
    }
    if (eq_filters_.size() != total)
        eq_filters_.assign(total, std::array<Biquad, kMaxChannels>{});

    std::size_t stage = 0;
    for (const auto& band : working_.eq) {
        if (!band.enabled) continue;
        if (band.type == EqType::LowCut || band.type == EqType::HighCut) {
            for (double q : butterworth_q_values(band.slope_db_per_oct)) {
                for (auto& b : eq_filters_[stage])
                    b.set(band.type, sample_rate_, band.frequency, q, band.gain_db, band.coefficient_mix);
                ++stage;
            }
        } else {
            for (auto& b : eq_filters_[stage])
                b.set(band.type, sample_rate_, band.frequency, band.q, band.gain_db, band.coefficient_mix);
            ++stage;
        }
    }
}

void ArSonKuPikEngine::rebuild_color_filters()
{
    const auto& c = working_.color;
    const double body_center = clamp(c.body_freq, 95.0, 260.0);
    const double harmonic_center = clamp(c.harmonics_freq, 1200.0, 3600.0);
    const double air_center = clamp(c.air_freq, 6500.0, 16000.0);
    for (int i = 0; i < 2; ++i) {
        // The old pre Bell used 0 dB gain and was mathematically identity.
        // Saturate directly, then keep the existing bounded bass residue window.
        bass_post_hp_[i].set(EqType::LowCut, sample_rate_, 34.0, 0.707, 0.0);
        bass_post_lp_[i].set(EqType::HighCut, sample_rate_, clamp(body_center * 2.35, 230.0, 420.0), 0.64, 0.0);
        // Smart Warm Core filters are configured as fixed perceptual windows
        // below, after the shared stereo colour filters have been prepared.
        // Smart Harmonic Color v1. Extract a bounded source band, generate only
        // nonlinear residue, then band-limit the residue before mixing it back.
        // This avoids the old broadband parallel-copy loudness and remains safe
        // for an AudioWorklet/WASM port because it is fixed-state, allocation-free.
        presence_pre_[i].set(EqType::LowCut, sample_rate_,
                             clamp(harmonic_center * 0.62, 1100.0, 2200.0), 0.707, 0.0);
        presence_pre_lp_[i].set(EqType::HighCut, sample_rate_,
                                clamp(harmonic_center * 2.25, 4200.0, 6200.0), 0.707, 0.0);
        presence_post_hp_[i].set(EqType::LowCut, sample_rate_, 4000.0, 0.707, 0.0);
        presence_post_lp_[i].set(EqType::HighCut, sample_rate_, 11800.0, 0.707, 0.0);

        air_pre_[i].set(EqType::LowCut, sample_rate_,
                        clamp(air_center * 0.28, 3200.0, 4700.0), 0.707, 0.0);
        air_pre_lp_[i].set(EqType::HighCut, sample_rate_,
                           clamp(air_center * 0.68, 7200.0, 9600.0), 0.707, 0.0);
        air_post_hp_[i].set(EqType::LowCut, sample_rate_, 6200.0, 0.707, 0.0);
        air_post_lp_[i].set(EqType::HighCut, sample_rate_,
                            std::min(17800.0, sample_rate_ * 0.43), 0.707, 0.0);
        const double velvet = clamp01(c.velvet_treble / 100.0);
        const double repair = clamp01(c.ai_high_repair / 100.0);
        deharsh_[i].set(EqType::Bell, sample_rate_, 6800.0 + repair * 1800.0, 1.10, -0.15 - velvet * 0.65);
    }
    side_air_hp_.set(EqType::LowCut, sample_rate_, 7800.0, 0.707, 0.0);
    side_air_tone_.set(EqType::HighShelf, sample_rate_, 10800.0, 0.58, clamp(c.air * 0.036 + c.god_particles * 0.014, -1.2, 4.8));
    mid_anchor_bp1_.set(EqType::LowCut, sample_rate_, 540.0, 0.707, 0.0);
    mid_anchor_bp2_.set(EqType::HighCut, sample_rate_, 4100.0, 0.707, 0.0);
    // Compact bus boundaries. Tickle inherits Presence's lower cut and only
    // narrows the top; Skin inherits Air's upper cut and only narrows the bottom.
    vocal_tickle_lp_.set(EqType::HighCut, sample_rate_, 3000.0, 0.707, 0.0);
    treble_skin_bp_.set(EqType::LowCut, sample_rate_, 4700.0, 0.707, 0.0);
    // Smart Warm Core v4.2 windows. The stereo warmth lane is the phase-safe
    // 72-150 Hz Core lane. Living Body keeps a centred 140-300 Hz foundation;
    // the flagship may add a tiny 145-290 Hz copy of the original Side. The
    // separate approximately 240-360 Hz path is analysis-only and can never create a cut.
    for (int i = 0; i < 2; ++i) {
        warmth_hp_[i].set(EqType::LowCut, sample_rate_, 72.0, 0.707, 0.0);
        warmth_lp_[i].set(EqType::HighCut, sample_rate_, 150.0, 0.707, 0.0);
    }
    low_body_hp_.set(EqType::LowCut, sample_rate_, 140.0, 0.707, 0.0);
    low_body_lp_.set(EqType::HighCut, sample_rate_, 300.0, 0.707, 0.0);
    low_body_side_hp_.set(EqType::LowCut, sample_rate_, 145.0, 0.707, 0.0);
    low_body_side_lp_.set(EqType::HighCut, sample_rate_, 290.0, 0.707, 0.0);
    warm_box_hp_.set(EqType::LowCut, sample_rate_, 250.0, 0.707, 0.0);
    warm_box_lp_.set(EqType::HighCut, sample_rate_, 360.0, 0.707, 0.0);
    for (int i = 0; i < 2; ++i) {
        // Guitar/Talkbox Bridge P0: extend the stable Upper Body foundation
        // down into guitar growl and up through the first talk-box formants.
        upper_body_hp_[i].set(EqType::LowCut, sample_rate_, 240.0, 0.707, 0.0);
        upper_body_lp_[i].set(EqType::HighCut, sample_rate_, 1100.0, 0.707, 0.0);
    }
    // Detector-only low-pass: validates that real body exists below 950 Hz.
    // It is never mixed into audio, so it cannot add parallel-filter phase shift.
    vocal_body_detector_lp_.set(EqType::HighCut, sample_rate_, 950.0, 0.707, 0.0);
    // Stable low-mid foundations remain independent of high-mid content. The
    // P1 cross-domain detector only controls a tiny additive companion.
    warm_particle_dc_hp_.set(EqType::LowCut, sample_rate_, 18.0, 0.707, 0.0);
    warm_particle_post_lp_.set(EqType::HighCut, sample_rate_, 340.0, 0.707, 0.0);
    rebuild_color_runtime();
}

void ArSonKuPikEngine::rebuild_width_filters()
{
    const auto& w = working_.width;
    const double tone = std::max(0.0, w.side_tone);
    const double generated_low_cut = w.mono_bass ? std::max(210.0, w.mono_bass_freq) : 150.0;

    // Generated side is now only a small air/space filler. Real stereo width is
    // handled from the original Side signal in process_width().
    (void)generated_low_cut; // each generated branch already has a stricter HPF
    width_lowmid_hp_.set(EqType::LowCut, sample_rate_, 240.0, 0.707, 0.0);
    width_lowmid_lp_.set(EqType::HighCut, sample_rate_, 1100.0, 0.707, 0.0);
    width_mid_hp_.set(EqType::LowCut, sample_rate_, 900.0, 0.707, 0.0);
    width_mid_lp_.set(EqType::HighCut, sample_rate_, 4000.0 + tone * 22.0, 0.707, 0.0);
    width_high_hp_.set(EqType::LowCut, sample_rate_, 7200.0 + tone * 62.0, 0.707, 0.0);
    width_air_tone_.set(EqType::HighShelf, sample_rate_, 11200.0 + tone * 180.0, 0.62, clamp(w.side_tone * 0.20, -1.2, 4.2));

    // Original-side multiband lanes. Low stereo is not blindly monoed: it is
    // reduced dynamically only when anti-phase/sustained low-side risk is high.
    width_side_sub_hp_.set(EqType::LowCut, sample_rate_, 22.0, 0.707, 0.0);
    width_side_sub_lp_.set(EqType::HighCut, sample_rate_, clamp(w.mono_bass_freq, 95.0, 170.0), 0.707, 0.0);
    width_side_body_hp_.set(EqType::LowCut, sample_rate_, clamp(w.mono_bass_freq * 0.82, 85.0, 150.0), 0.707, 0.0);
    width_side_body_lp_.set(EqType::HighCut, sample_rate_, 680.0, 0.707, 0.0);
    width_side_mid_hp_.set(EqType::LowCut, sample_rate_, 680.0, 0.707, 0.0);
    width_side_mid_lp_.set(EqType::HighCut, sample_rate_, 3600.0, 0.707, 0.0);
    width_side_high_hp_.set(EqType::LowCut, sample_rate_, 3600.0, 0.707, 0.0);
    width_side_high_lp_.set(EqType::HighCut, sample_rate_, 11200.0, 0.707, 0.0);
    width_side_air_hp_.set(EqType::LowCut, sample_rate_, 7800.0 + tone * 42.0, 0.707, 0.0);
    width_side_air_tone_.set(EqType::HighShelf, sample_rate_, 11200.0 + tone * 190.0, 0.62, clamp(w.side_tone * 0.22, -1.2, 4.6));
    rebuild_width_runtime();
}

ArSonKuPikEngine::SaturationRuntime
ArSonKuPikEngine::make_saturation_runtime(double drive)
{
    SaturationRuntime result;
    result.drive = clamp(drive, 0.1, 2.4);
    result.inverse_drive = 1.0 / result.drive;
    result.wet = clamp((result.drive - 1.0) * 0.014, 0.0, 0.026);
    result.dry = 1.0 - result.wet;
    return result;
}

void ArSonKuPikEngine::rebuild_color_runtime()
{
    const ColorRuntime previous = color_runtime_;
    color_runtime_ = {};
    const auto& c = working_.color;
    if (!c.enabled || c.mix <= 0.0)
        return;

    auto& runtime = color_runtime_;
    runtime.active = true;

    const double mix = clamp01(c.mix / 100.0);
    const double harmonic = clamp01(c.harmonics / 100.0);
    const double bass_amt = clamp01(c.smart_bass / 100.0);
    const double bass_over = clamp01((c.body - 26.0) / 14.0);
    const double air_amt = clamp(c.air / 48.0, -0.5, 1.0);
    const double air_over = clamp01((c.air - 96.0) / 40.0);
    const double god = clamp01(c.god_particles / 100.0);
    const double stereo_mid = clamp01(c.stereo_mid / 100.0);
    const double vocal_presence = clamp01(c.vocal_presence / 100.0);
    const double mid_projection = clamp01(c.mid_projection / 100.0);
    const double vocal_tickle = clamp01(c.vocal_tickle / 100.0);
    const double velvet = clamp01(c.velvet_treble / 100.0);
    const double warm_particles = clamp01(c.warm_particles / 100.0);
    const double warm_box_guard = clamp01(c.warm_box_guard / 100.0);
    const double drive = clamp(c.drive * 0.22 + c.harmonics * 0.002, 0.0, 1.60)
                       * mode_factor(c.mode);

    runtime.bass = make_saturation_runtime(0.92 + drive * 0.10 + bass_amt * 0.35);
    runtime.warm = make_saturation_runtime(1.12 + drive * 0.22
                                           + warm_particles * 0.48);
    runtime.presence = make_saturation_runtime(1.22 + drive * 0.22 + harmonic * 0.30);
    runtime.air = make_saturation_runtime(1.30 + std::max(0.0, air_amt) * 0.42 + harmonic * 0.24);
    runtime.side_air = make_saturation_runtime(0.74 + god * 0.28 + std::max(0.0, air_amt) * 0.26);
    runtime.mid_anchor = make_saturation_runtime(0.86 + stereo_mid * 0.38 + vocal_presence * 0.28);
    runtime.low_body = make_saturation_runtime(1.08 + bass_amt * 0.20
                                               + warm_particles * 0.34
                                               + std::max(0.0, c.body) * 0.003);
    runtime.tickle = make_saturation_runtime(1.18 + vocal_tickle * 0.36 + harmonic * 0.16);
    runtime.skin = make_saturation_runtime(1.28 + god * 0.22 + std::max(0.0, air_amt) * 0.28);

    runtime.bass_wet = mix * (0.030 + bass_amt * 0.092
                            + std::max(0.0, c.body) * 0.0025
                            + bass_over * 0.040);
    runtime.warm_wet = mix * clamp(0.020 + c.warmth * 0.0020, 0.0, 0.078)
                     + warm_particles * 0.012;
    runtime.presence_wet = mix * clamp(0.055 + harmonic * 0.070
                                       + vocal_presence * 0.075, 0.0, 0.175);
    runtime.air_wet = mix * clamp(0.072
                                 + std::max(0.0, air_amt) * 0.145
                                 + harmonic * 0.055 + velvet * 0.020
                                 + air_over * 0.035, 0.0, 0.220);
    runtime.side_air_wet = mix * clamp(
        god * (0.036 + std::max(0.0, air_amt) * 0.082 + harmonic * 0.0008)
        + air_over * 0.025, 0.0, 0.155);
    runtime.mid_wet = mix * clamp(stereo_mid * 0.034 + vocal_presence * 0.078
                                 + mid_projection * 0.070, 0.0, 0.185);
    runtime.mid_wet *= kSignatureMidAnchorVitality;
    runtime.low_body_wet = mix * clamp(bass_amt * 0.022
                                      + std::max(0.0, c.body) * 0.0021
                                      + std::max(0.0, c.warmth) * 0.0012,
                                      0.0, 0.070)
                            + warm_particles * 0.010;
    runtime.tickle_wet = mix * clamp(vocal_tickle * 0.095 + harmonic * 0.018,
                                      0.0, 0.125);
    runtime.skin_wet = mix * clamp((god * 0.055 + std::max(0.0, air_amt) * 0.070
                                   + vocal_tickle * 0.028) * (1.0 - velvet * 0.10),
                                   0.0, 0.115);
    runtime.high_guard_depth = clamp(0.44 + velvet * 0.24
                                     + clamp01(c.ai_high_repair / 100.0) * 0.20,
                                     0.35, 0.86);

    runtime.warm_particle_side_mix = 1.20 + stereo_mid * 0.28;
    runtime.presence_side_mix = 0.78 + god * 0.18;
    runtime.tickle_side_mix = 0.62 + god * 0.16;
    runtime.air_side_mix = 0.82 + god * 0.20;
    runtime.skin_side_mix = 0.68 + god * 0.18;
    runtime.presence_floor = 0.62;
    runtime.tickle_floor = 0.46;
    runtime.air_floor = 0.56;
    runtime.skin_floor = 0.38;

    // P6 Signature Microdetail & Ear-Tickle -------------------------------
    // Lift only the generated Mid residue so centre detail, vocal texture and
    // instrument particles become easier to hear without widening the image.
    // Reciprocal Side scaling holds the existing Side contribution essentially
    // unchanged. Tickle's guarded-floor compensation prevents its Side lane from
    // growing when dense material reduces permission, protecting mono/correlation.
    const bool flagshipMicrodetail = preset_ != nullptr
                                  && preset_->id == "default";
    if (flagshipMicrodetail) {
        constexpr double presenceMidLift = 1.09;
        constexpr double airMidLift = 1.07;
        constexpr double tickleMidLift = 1.30;
        constexpr double oldTickleFloor = 0.46;
        constexpr double newTickleFloor = 0.50;

        runtime.presence_wet *= presenceMidLift;
        runtime.presence_side_mix /= presenceMidLift;
        runtime.air_wet *= airMidLift;
        runtime.air_side_mix /= airMidLift;
        runtime.tickle_wet *= tickleMidLift;
        runtime.tickle_side_mix *= (oldTickleFloor / newTickleFloor)
                                 / tickleMidLift;
        runtime.tickle_floor = newTickleFloor;
    }

    // Smart Warm Core uses linear parallel lanes. Keep the legacy saturation
    // runtime members clean so no broad low-mid waveshaping can be reintroduced
    // accidentally by a future call site.
    runtime.warm.wet = 0.0;
    runtime.warm.dry = 1.0;
    runtime.low_body.wet = 0.0;
    runtime.low_body.dry = 1.0;

    // Stable foundation + micro extension. The detector can only reduce the
    // extension; the audible core/body foundation is never multiplied by a
    // gate or guard. Mas Ari Signature receives the audited Moderate contour;
    // every other factory preset keeps the v3 coefficients exactly.
    const bool flagshipWarmCalibration = preset_ != nullptr
                                      && preset_->id == "default";
    if (flagshipWarmCalibration) {
        runtime.core_foundation_wet = mix * clamp(
            0.036 + std::max(0.0, c.warmth) * 0.00185
                  + std::max(0.0, c.body) * 0.00050,
            0.0, 0.083) + warm_particles * 0.011;
        runtime.core_extension_wet = 0.00275 + warm_particles * 0.00475;
        runtime.body_foundation_wet = mix * clamp(
            0.044 + std::max(0.0, c.body) * 0.00255
                  + std::max(0.0, c.warmth) * 0.00205,
            0.0, 0.115) + warm_particles * 0.0135;
        runtime.body_extension_wet = 0.00375 + warm_particles * 0.0075;
        runtime.body_side_mix = 0.12;

        // Upper Body Warmth restores the guitar/piano/vocal bridge that can
        // feel recessed after gain matching. Almost all of the contribution is
        // a stable linear foundation. The content-aware part is deliberately
        // capped to a micro extension so changing arrangements cannot make the
        // low-mid balance breathe or rock.
        // Body Density P2: put the majority of the small thickness lift in
        // the stable post-compressor foundation. Reducing the content-aware
        // extension keeps dense guitars and vocal arrangements from breathing.
        runtime.upper_body_foundation_wet = mix * clamp(
            0.165 + std::max(0.0, c.warmth) * 0.00090
                  + std::max(0.0, c.body) * 0.00045,
            0.0, 0.195) + warm_particles * 0.0020;
        // P3 Smart Mid-Low Context: the foundation stays fixed, while a
        // restrained extension can restore life when 240-1100 Hz body is weak.
        // The detector below reduces it on dense/boxy material, so it is not a
        // permanent broad EQ boost.
        runtime.upper_body_extension_wet = 0.0100 + warm_particles * 0.0080;
        runtime.upper_body_side_mix = 0.68;
        runtime.vocal_body_companion_wet = mix * clamp(
            0.140 + vocal_presence * 0.120, 0.0, 0.240);
        // P1B uses a lower, phrase-stable Mid-only lane. The fixed 0.032 floor
        // keeps a useful chest contribution when a male vocal has less 2-4 kHz
        // energy than the existing female-oriented P1 detector expects.
        runtime.male_vocal_body_wet = 0.032 + mix * clamp(
            0.070 + vocal_presence * 0.070 + warm_particles * 0.020,
            0.0, 0.145);
    } else {
        runtime.core_foundation_wet = mix * clamp(
            0.030 + std::max(0.0, c.warmth) * 0.0017
                  + std::max(0.0, c.body) * 0.00045,
            0.0, 0.075) + warm_particles * 0.010;
        runtime.core_extension_wet = 0.0025 + warm_particles * 0.0045;
        runtime.body_foundation_wet = mix * clamp(
            0.036 + std::max(0.0, c.body) * 0.0023
                  + std::max(0.0, c.warmth) * 0.0019,
            0.0, 0.105) + warm_particles * 0.012;
        runtime.body_extension_wet = 0.0035 + warm_particles * 0.0070;
        runtime.body_side_mix = 0.0;
        runtime.upper_body_foundation_wet = 0.0;
        runtime.upper_body_extension_wet = 0.0;
        runtime.upper_body_side_mix = 0.0;
        runtime.vocal_body_companion_wet = 0.0;
        runtime.male_vocal_body_wet = 0.0;
    }
    runtime.warm_core_guard = 0.72;
    runtime.warm_box_guard = 0.58 + warm_box_guard * 0.34;
    runtime.warm_upper_guard = 0.68;
    runtime.warm_transient_guard = 0.70;
    // P3 Smart Bass Harmonic: about 4 dB more maximum even-harmonic
    // authority than P2, still controlled by transient, mud and harmonic-need
    // permissions. It creates 144-300 Hz audibility from the 72-150 Hz core
    // instead of adding more static sub gain.
    runtime.warm_particle_wet = 0.0076 + warm_particles * 0.0114;
    // P5 adds only harmonic texture above the fundamental. It reuses the
    // existing body/mud sources, and the wet amount remains deliberately below
    // the bass particle so it cannot turn a dense rock mix boxy.
    runtime.lowmid_particle_wet = flagshipWarmCalibration
        ? (0.0055 + warm_particles * 0.0075) : 0.0;
    runtime.lowmid_particle_side_mix = flagshipWarmCalibration
        ? (0.22 + stereo_mid * 0.10) : 0.0;

    runtime.warm_wet = runtime.core_foundation_wet
                     + runtime.core_extension_wet;
    runtime.low_body_wet = runtime.body_foundation_wet
                         + runtime.body_extension_wet;

    // Contributions below this point are below the practical output floor but
    // previously kept complete filter/saturation lanes running.
    constexpr double laneFloor = 1.0e-6;
    runtime.bass_active = runtime.bass_wet > laneFloor;
    runtime.warm_active = runtime.warm_wet > laneFloor;
    runtime.presence_active = runtime.presence_wet > laneFloor;
    runtime.air_active = runtime.air_wet > laneFloor;
    runtime.side_air_active = runtime.side_air_wet > laneFloor;
    runtime.mid_active = runtime.mid_wet > laneFloor;
    runtime.low_body_active = runtime.low_body_wet > laneFloor;
    runtime.body_side_active = runtime.body_side_mix > laneFloor;
    runtime.upper_body_active = runtime.upper_body_foundation_wet
                              + runtime.upper_body_extension_wet > laneFloor;
    runtime.tickle_active = runtime.tickle_wet > laneFloor;
    runtime.skin_active = runtime.skin_wet > laneFloor;
    runtime.smart_warm_active = runtime.warm_active || runtime.low_body_active;

    auto resetStereo = [](auto& filters) {
        for (auto& filter : filters)
            filter.reset();
    };
    if (runtime.bass_active && !previous.bass_active) {
        resetStereo(bass_post_hp_);
        resetStereo(bass_post_lp_);
    }
    if (runtime.warm_active && !previous.warm_active) {
        resetStereo(warmth_hp_);
        resetStereo(warmth_lp_);
    }
    if (runtime.upper_body_active && !previous.upper_body_active) {
        resetStereo(upper_body_hp_);
        resetStereo(upper_body_lp_);
        vocal_body_detector_lp_.reset();
        warm_upper_body_env_ = 1.0e-6;
        warm_upper_body_fast_env_ = 1.0e-6;
        warm_upper_permission_ = 0.75;
    }
    if (runtime.presence_active && !previous.presence_active) {
        resetStereo(presence_pre_);
        resetStereo(presence_pre_lp_);
        resetStereo(presence_post_hp_);
        resetStereo(presence_post_lp_);
    }
    if (runtime.air_active && !previous.air_active) {
        resetStereo(air_pre_);
        resetStereo(air_pre_lp_);
        resetStereo(air_post_hp_);
        resetStereo(air_post_lp_);
    }
    if (runtime.side_air_active && !previous.side_air_active) {
        side_air_hp_.reset();
        side_air_tone_.reset();
    }
    if (runtime.mid_active && !previous.mid_active) {
        mid_anchor_bp1_.reset();
        mid_anchor_bp2_.reset();
    }
    if (runtime.smart_warm_active && !previous.smart_warm_active) {
        low_body_hp_.reset();
        low_body_lp_.reset();
        warm_box_hp_.reset();
        warm_box_lp_.reset();
        warm_particle_dc_hp_.reset();
        warm_particle_post_lp_.reset();
        warm_particle_side_hp_x1_ = warm_particle_side_hp_y1_ = 0.0f;
        warm_core_fast_env_ = 1.0e-6;
        warm_core_slow_env_ = 1.0e-6;
        warm_mud_env_ = 1.0e-6;
        warm_living_body_env_ = 1.0e-6;
        warm_upper_body_env_ = 1.0e-6;
        warm_input_env_ = 1.0e-6;
        warm_core_permission_ = 0.75;
        warm_body_permission_ = 0.75;
        warm_upper_permission_ = 0.75;
        warm_particle_permission_ = 1.0;
        adaptive_bass_permission_ = 1.0;
        warm_core_side_env_ = 1.0e-6;
        warm_particle_side_context_ = 0.0;
        warm_particle_side_cached_ = 0.0f;
        warm_control_counter_ = 0;
    }
    if (runtime.body_side_active && !previous.body_side_active) {
        low_body_side_hp_.reset();
        low_body_side_lp_.reset();
    }
    if (runtime.tickle_active && !previous.tickle_active)
        vocal_tickle_lp_.reset();
    if (runtime.skin_active && !previous.skin_active)
        treble_skin_bp_.reset();
    const bool presenceGroupActive = runtime.presence_active || runtime.tickle_active;
    const bool previousPresenceGroup = previous.presence_active || previous.tickle_active;
    if (presenceGroupActive && !previousPresenceGroup) {
        presence_fast_env_ = presence_slow_env_ = 1.0e-6;
        presence_mid_env_ = presence_side_env_ = 1.0e-6;
        tickle_env_ = 1.0e-6;
        presence_permission_ = tickle_permission_ = 1.0;
        adaptive_presence_need_ = adaptive_tickle_need_ = 1.0;
        low_level_particle_support_ = 1.0;
        microdetail_presence_articulation_ = microdetail_tickle_articulation_ = 1.0;
        presence_side_context_ = 0.0;
    }
    const bool airGroupActive = runtime.air_active || runtime.skin_active;
    const bool previousAirGroup = previous.air_active || previous.skin_active;
    if (airGroupActive && !previousAirGroup) {
        air_fast_env_ = air_slow_env_ = 1.0e-6;
        air_mid_env_ = air_side_env_ = 1.0e-6;
        skin_env_ = 1.0e-6;
        air_permission_ = skin_permission_ = 1.0;
        adaptive_air_need_ = adaptive_skin_need_ = 1.0;
        low_level_particle_support_ = 1.0;
        microdetail_air_articulation_ = microdetail_skin_articulation_ = 1.0;
        air_side_context_ = 0.0;
    }
    if ((presenceGroupActive || airGroupActive)
        && !(previousPresenceGroup || previousAirGroup)) {
        high_input_fast_env_ = 1.0e-6;
        high_input_env_ = 1.0e-6;
        high_analysis_phase_ = false;
        high_control_counter_ = 0;
    }

    runtime.compensation = 1.0 / (1.0 + mix * (drive / 12.2) * 0.006
        + harmonic * mix * 0.0012 + runtime.side_air_wet * 0.004
        + runtime.mid_wet * 0.0035 + runtime.low_body_wet * 0.003

        + runtime.air_wet * 0.0025);
    runtime.dry_keep = clamp(1.0 + mix * (0.075 + vocal_presence * 0.055),
                             1.010, 1.155);
}

void ArSonKuPikEngine::rebuild_width_runtime()
{
    width_runtime_ = {};
    const auto& w = working_.width;
    if (!w.enabled || w.mix <= 0.0)
        return;

    auto& runtime = width_runtime_;
    runtime.active = true;
    runtime.narrow = w.width < 99.99;
    runtime.side_scale = clamp(w.width / 100.0, 0.0, 1.0);
    if (runtime.narrow)
        return;

    runtime.width_mix = clamp01(w.mix / 100.0);
    runtime.source_protect = clamp01(w.source_protect / 100.0);
    const double master_expand = clamp((w.width - 100.0) / 100.0, 0.0, 1.0);
    auto additive = [&](double percent, double weight, double linked, double max_value)
    {
        const double band_expand = clamp((percent - 100.0) / 100.0, 0.0, 1.0);
        return clamp(band_expand * weight + master_expand * linked, 0.0, max_value)
             * runtime.width_mix;
    };

    // P3 Stereo Vitality: original Side remains the primary widening source.
    // Coefficients are raised only in body/projection lanes; high/air retain
    // the previous conservative contour. Band-context guards below prevent
    // the extra authority from widening already-excessive or anti-phase mixes.
    runtime.body_base = additive(w.low_mid_width, 0.120, 0.040, 0.160);
    runtime.mid_base = additive(w.mid_width, 0.180, 0.070, 0.250);
    runtime.high_base = additive(w.high_width, 0.215, 0.088, 0.285);
    runtime.air_base = additive(w.high_width, 0.145, 0.064, 0.210);
    runtime.gen_lowmid_base = additive(w.low_mid_width, 0.022, 0.008, 0.035);
    runtime.gen_mid_base = additive(w.mid_width, 0.038, 0.015, 0.060);
    runtime.gen_air_base = additive(w.high_width, 0.052, 0.020, 0.075);
    // Below -54 dB of side contribution the branch is visually measurable but
    // not musically useful; avoid two filters for that near-zero filler.
    runtime.gen_lowmid_active = runtime.gen_lowmid_base >= 0.0020;
    runtime.gen_mid_active = runtime.gen_mid_base >= 1.0e-6;
    runtime.gen_air_active = runtime.gen_air_base >= 1.0e-6;
    runtime.side_air = make_saturation_runtime(
        0.68 + runtime.width_mix * 0.12 + std::max(0.0, w.side_tone) * 0.008);
}

float ArSonKuPikEngine::soft_saturate(float x,
                                      const SaturationRuntime& runtime) const
{
    // Drives at or below unity have an exact zero wet mix. Avoiding nonlinear
    // work in that common clean lane is both faster and numerically identical.
    if (runtime.wet <= 0.0)
        return sane(x);

    const float shaped = fast_tanh_color(
        x * static_cast<float>(runtime.drive))
        * static_cast<float>(runtime.inverse_drive);
    const float y = x * static_cast<float>(runtime.dry)
                  + shaped * static_cast<float>(runtime.wet);
    return sane(y);
}

float ArSonKuPikEngine::harmonic_residue(
    float x, const SaturationRuntime& runtime) const
{
    if (runtime.drive <= 1.000001)
        return 0.0f;
    const float shaped = fast_tanh_color(
        x * static_cast<float>(runtime.drive))
        * static_cast<float>(runtime.inverse_drive);
    return sane(shaped - x);
}

void ArSonKuPikEngine::harmonic_residue_pair(
    float& left, float& right, const SaturationRuntime& runtime) const
{
    if (runtime.drive <= 1.000001) {
        left = 0.0f;
        right = 0.0f;
        return;
    }
#if ASKP_DSP_HAS_SSE2
    const __m128 input = _mm_set_ps(0.0f, 0.0f, right, left);
    const __m128 driven = _mm_mul_ps(
        input, _mm_set1_ps(static_cast<float>(runtime.drive)));
    const __m128 shaped = _mm_mul_ps(
        fast_tanh_color_ps(driven),
        _mm_set1_ps(static_cast<float>(runtime.inverse_drive)));
    const __m128 residue = _mm_sub_ps(shaped, input);
    alignas(16) float values[4];
    _mm_store_ps(values, residue);
    left = sane(values[0]);
    right = sane(values[1]);
#else
    left = harmonic_residue(left, runtime);
    right = harmonic_residue(right, runtime);
#endif
}

void ArSonKuPikEngine::soft_saturate_pair(
    float& left, float& right, const SaturationRuntime& runtime) const
{
    if (runtime.wet <= 0.0) {
        left = sane(left);
        right = sane(right);
        return;
    }

#if ASKP_DSP_HAS_SSE2
    // L/R color lanes are independent and share identical runtime constants.
    // Packing them together reduces function/libm overhead and lets x64 use one
    // SIMD rational approximation for both channels. The upper two lanes are
    // zero and discarded after the operation.
    const __m128 input = _mm_set_ps(0.0f, 0.0f, right, left);
    const __m128 driven = _mm_mul_ps(input,
        _mm_set1_ps(static_cast<float>(runtime.drive)));
    const __m128 shaped = _mm_mul_ps(fast_tanh_color_ps(driven),
        _mm_set1_ps(static_cast<float>(runtime.inverse_drive)));
    const __m128 output = _mm_add_ps(
        _mm_mul_ps(input, _mm_set1_ps(static_cast<float>(runtime.dry))),
        _mm_mul_ps(shaped, _mm_set1_ps(static_cast<float>(runtime.wet))));
    alignas(16) float values[4];
    _mm_store_ps(values, output);
    left = sane(values[0]);
    right = sane(values[1]);
#else
    left = soft_saturate(left, runtime);
    right = soft_saturate(right, runtime);
#endif
}

float ArSonKuPikEngine::process_channel_eq(std::size_t ch, float x)
{
    for (auto& filter : eq_filters_) {
        x = filter[ch].process(x);
    }
    return x;
}

std::pair<float,float> ArSonKuPikEngine::process_color(float l, float r)
{
    const auto& runtime = color_runtime_;
    if (!runtime.active) return {l, r};

    const float dry_l = l;
    const float dry_r = r;
    float add_l = 0.0f;
    float add_r = 0.0f;
    const float mid = 0.5f * (l + r);
    const float side = 0.5f * (l - r);

    float warm_l = 0.0f;
    float warm_r = 0.0f;
    float low_body = 0.0f;
    float body_out_l = 0.0f;
    float body_out_r = 0.0f;
    float warm_particle = 0.0f;
    float lowmid_particle = 0.0f;
    float upper_body_out_l = 0.0f;
    float upper_body_out_r = 0.0f;
    float vocal_body_companion_source = 0.0f;
    float male_vocal_body_source = 0.0f;
    float core_out_l = 0.0f;
    float core_out_r = 0.0f;
    double core_wet = 0.0;
    double body_wet = 0.0;
    double upper_body_wet = 0.0;

    if (runtime.smart_warm_active) {
        warm_l = warmth_lp_[0].process(warmth_hp_[0].process(l));
        warm_r = warmth_lp_[1].process(warmth_hp_[1].process(r));
        low_body = low_body_lp_.process(low_body_hp_.process(mid));
        float low_body_side = 0.0f;
        if (runtime.body_side_active)
            low_body_side = low_body_side_lp_.process(low_body_side_hp_.process(side));
        body_out_l = low_body + static_cast<float>(runtime.body_side_mix) * low_body_side;
        body_out_r = low_body - static_cast<float>(runtime.body_side_mix) * low_body_side;
        float mud = 0.0f;
        if (runtime.upper_body_active) {
            const float upper_l = upper_body_lp_[0].process(upper_body_hp_[0].process(l));
            const float upper_r = upper_body_lp_[1].process(upper_body_hp_[1].process(r));
            const float upper_mid = 0.5f * (upper_l + upper_r);
            const float upper_side = 0.5f * (upper_l - upper_r);
            upper_body_out_l = upper_mid
                + static_cast<float>(runtime.upper_body_side_mix) * upper_side;
            upper_body_out_r = upper_mid
                - static_cast<float>(runtime.upper_body_side_mix) * upper_side;
            const double upperBodyNow = std::abs(static_cast<double>(upper_mid));
            warm_upper_body_env_ += warm_slow_alpha_
                                  * (upperBodyNow - warm_upper_body_env_);
            // Detector observes the real 240-950 Hz bridge only. The narrow
            // density subtraction below is reserved for the audible companion.
            const float vocalBodyDetector = vocal_body_detector_lp_.process(
                upper_mid);
            const double detectorNow = std::abs(
                static_cast<double>(vocalBodyDetector));
            warm_upper_body_fast_env_ += warm_fast_alpha_
                * (detectorNow - warm_upper_body_fast_env_);
            // Upper Body is already high-passed at 240 Hz; the existing
            // 250-360 Hz density detector is sufficient in the flagship path.
            mud = warm_box_lp_.process(upper_mid);
            vocal_body_companion_source = upper_mid - 0.35f * mud;
            // Reuse the centred 140-300 Hz chest and 240-1100 Hz throat buses.
            // A bounded density subtraction prevents the added body from becoming
            // a static 315 Hz box lift. No new filter or analysis lane is added.
            male_vocal_body_source = 0.68f * low_body
                                   + 0.32f * upper_mid
                                   - 0.18f * mud;
        } else {
            mud = warm_box_lp_.process(warm_box_hp_.process(mid));
        }

        const float core_mid = 0.5f * (warm_l + warm_r);
        const float core_side = 0.5f * (warm_l - warm_r);
        core_out_l = core_mid + 0.12f * core_side;
        core_out_r = core_mid - 0.12f * core_side;

        const double core_now = std::abs(static_cast<double>(core_mid));
        const double body_now = std::abs(static_cast<double>(low_body));
        const double mud_now = std::abs(static_cast<double>(mud));
        const double input_now = 0.5 * (std::abs(static_cast<double>(l))
                                      + std::abs(static_cast<double>(r)));

        warm_core_fast_env_ += warm_fast_alpha_
                             * (core_now - warm_core_fast_env_);
        warm_core_slow_env_ += warm_slow_alpha_
                             * (core_now - warm_core_slow_env_);
        warm_core_side_env_ += warm_slow_alpha_
                             * (std::abs(static_cast<double>(core_side))
                                - warm_core_side_env_);
        warm_lowmid_side_env_ += warm_slow_alpha_
                             * (std::abs(static_cast<double>(low_body_side))
                                - warm_lowmid_side_env_);
        warm_living_body_env_ += warm_slow_alpha_
                             * (body_now - warm_living_body_env_);
        warm_mud_env_ += warm_slow_alpha_ * (mud_now - warm_mud_env_);
        warm_input_env_ += warm_slow_alpha_ * (input_now - warm_input_env_);

        if (++warm_control_counter_ >= 32) {
            warm_control_counter_ = 0;

            auto smoothstep = [](double edge0, double edge1, double value)
            {
                const double x = clamp((value - edge0) / (edge1 - edge0),
                                       0.0, 1.0);
                return x * x * (3.0 - 2.0 * x);
            };
            auto movePermission = [](double current, double target,
                                     double reduceAlpha, double restoreAlpha)
            {
                const double alpha = target < current ? reduceAlpha : restoreAlpha;
                return current + alpha * (target - current);
            };

            double coreTarget = 0.75;
            double bodyTarget = 0.75;
            double particleTarget = 1.0;
            double lowmidParticleTarget = 0.75;
            double upperTarget = 0.75;
            double bassExtensionTarget = 1.0;
            const bool signalActive = warm_input_env_ > 2.0e-5;
            double vocalBodyCompanionTarget = 0.0;
            bool vocalBodyMaterialReject = false;
            double maleVocalBodyTarget = 0.0;
            bool maleVocalBodyMaterialReject = false;
            if (signalActive) {
                const double core = std::max(1.0e-9, warm_core_slow_env_);
                const double body = std::max(1.0e-9, warm_living_body_env_);
                const double coreShare = core / (core + 0.85 * body + 1.0e-9);
                const double mudEnvelope = std::max(1.0e-9, warm_mud_env_);
                const double upper = std::max(1.0e-9, warm_upper_body_env_);
                // P5 separates narrow approximately 240-360 Hz density from musical body.
                // Subtracting only a bounded part of the mud envelope leaves
                // 360-1100 Hz vocal throat, cabinet texture and power-chord
                // growl available to the context engine instead of treating it
                // as a reason to switch the entire low-mid extension off.
                const double musicalBody = std::max(
                    1.0e-9, upper - 0.82 * mudEnvelope);
                const double mudShare = mudEnvelope
                    / (mudEnvelope + 0.72 * musicalBody
                       + 0.45 * body + 1.0e-9);
                const double coreRisk = smoothstep(0.62, 0.78, coreShare);
                const double mudRisk = smoothstep(0.48, 0.70, mudShare);
                const double musicalShare = musicalBody
                    / (musicalBody + 0.72 * body + 1.0e-9);
                const double upperRisk = smoothstep(0.76, 0.94, musicalShare);
                const double upperNeed = 1.0
                    - smoothstep(0.34, 0.62, musicalShare);
                const double pShareForBody = presence_slow_env_
                    / (high_input_env_ + 1.0e-9);
                const double bodyBridgeShare = musicalBody
                    / (warm_input_env_ + 1.0e-9);
                const double fastBridgeShare = warm_upper_body_fast_env_
                    / (high_input_fast_env_ + 1.0e-9);
                const double presenceDominance = smoothstep(
                    0.48, 0.68, pShareForBody);
                const double bridgeLean = 1.0 - smoothstep(
                    0.34, 0.44, bodyBridgeShare);
                const double bridgeExists = smoothstep(
                    0.12, 0.25, bodyBridgeShare);
                const double freshBridge = smoothstep(
                    0.10, 0.24, fastBridgeShare);
                const double mudSafe = 1.0 - smoothstep(
                    0.35, 0.48, mudShare);
                const double presenceTransientRatio = presence_fast_env_
                    / (presence_slow_env_ + 1.0e-9);
                const double sustained = 1.0 - smoothstep(
                    1.25, 1.75, presenceTransientRatio);
                const double presenceSideRatio = presence_side_env_
                    / (presence_mid_env_ + 1.0e-9);
                const double centerConfidence = 1.0 - smoothstep(
                    0.30, 0.78, presenceSideRatio);
                const double signalConfidence = smoothstep(
                    3.0e-4, 2.0e-3, warm_input_env_);
                vocalBodyCompanionTarget = clamp(
                    presenceDominance * bridgeLean * bridgeExists
                    * freshBridge * mudSafe * sustained * centerConfidence
                    * signalConfidence,
                    0.0, 1.0);
                vocalBodyMaterialReject = presenceDominance < 0.16
                    || mudSafe < 0.18 || freshBridge < 0.12
                    || centerConfidence < 0.18;

                // P1B Male Vocal Body -----------------------------------------
                // P1 intentionally remains unchanged. This parallel detector
                // favours centred chest + throat material whose low-mid energy is
                // stronger than its Presence band. Core dominance rejects kick and
                // bass; Side energy rejects guitars/piano; mud and transient gates
                // prevent a boomy or breathing result.
                const double bodyPresenceBalance = body
                    / (body + 0.72 * presence_slow_env_ + 1.0e-9);
                const double throatPresenceBalance = musicalBody
                    / (musicalBody + 0.82 * presence_slow_env_ + 1.0e-9);
                const double maleChestConfidence = smoothstep(
                    0.30, 0.58, bodyPresenceBalance);
                const double maleThroatConfidence = smoothstep(
                    0.20, 0.50, throatPresenceBalance);
                const double malePresenceFloor = smoothstep(
                    0.045, 0.18, pShareForBody);
                const double bodySideRatio = warm_lowmid_side_env_
                    / (body + mudEnvelope + 1.0e-9);
                const double bodyCenterConfidence = 1.0 - smoothstep(
                    0.05, 0.18, bodySideRatio);
                const double maleCoreSafe = 1.0 - smoothstep(
                    0.66, 0.84, coreShare);
                const double maleMudSafe = 1.0 - smoothstep(
                    0.42, 0.58, mudShare);
                maleVocalBodyTarget = clamp(
                    maleChestConfidence * maleThroatConfidence
                    * malePresenceFloor * bodyCenterConfidence
                    * centerConfidence * maleCoreSafe * maleMudSafe
                    * sustained * signalConfidence,
                    0.0, 1.0);
                maleVocalBodyMaterialReject = maleChestConfidence < 0.10
                    || maleThroatConfidence < 0.08
                    || malePresenceFloor < 0.08
                    || bodyCenterConfidence < 0.18
                    || centerConfidence < 0.18
                    || maleCoreSafe < 0.15 || maleMudSafe < 0.15;
                adaptive_state_.male_vocal_chest_confidence =
                    maleChestConfidence * maleThroatConfidence;
                adaptive_state_.male_vocal_center_confidence =
                    bodyCenterConfidence * centerConfidence;

                adaptive_state_.vocal_body_fast_bridge = fastBridgeShare;
                adaptive_state_.vocal_body_center_confidence = centerConfidence;
                coreTarget = 1.0 - runtime.warm_core_guard * coreRisk;
                // Preserve a meaningful context floor. Maximum wet is unchanged;
                // only the old near-off behaviour is removed.
                bodyTarget = 0.36 + 0.64
                    * (1.0 - runtime.warm_box_guard * mudRisk);
                upperTarget = 0.38 + 0.62 * upperNeed
                            - runtime.warm_upper_guard * 0.24 * upperRisk
                            - 0.14 * mudRisk;

                const double transientRatio = warm_core_fast_env_
                                            / (core + 1.0e-9);
                const double transientRisk = smoothstep(1.30, 2.10,
                                                        transientRatio);
                // Smart Bass Harmonic need: if the 72-150 Hz core is strong
                // but its 140-300 Hz body is comparatively weak, permit more
                // even-harmonic particle. Mud and sharp core transients reduce
                // it, preserving a round/soft rather than buzzy bass texture.
                const double bodyToCore = body / (core + 1.0e-9);
                const double harmonicNeed = 1.0
                    - smoothstep(0.72, 1.65, bodyToCore);
                particleTarget = (0.50 + 0.50 * harmonicNeed)
                               * (1.0 - runtime.warm_transient_guard * transientRisk)
                               * (1.0 - 0.30 * mudRisk);
                const double lowmidToBody = musicalBody / (body + 1.0e-9);
                const double lowmidNeed = 1.0
                    - smoothstep(0.62, 1.48, lowmidToBody);
                lowmidParticleTarget = (0.42 + 0.58 * lowmidNeed)
                    * (1.0 - 0.48 * transientRisk)
                    * (1.0 - 0.50 * mudRisk);

                // P7D: keep the P7A/P7B additive-bass recipe at unity for
                // ordinary material. Only a clearly low-heavy programme is
                // trimmed, while lean material may receive a tiny lift. Core
                // and Body linear foundations remain fixed.
                const double lowShare = (core + 0.72 * body)
                    / (warm_input_env_ + 1.0e-9);
                adaptive_state_.low_density = lowShare;
                const double bassLean = 1.0 - smoothstep(0.30, 0.55, lowShare);
                const double bassHeavy = smoothstep(0.82, 0.98, lowShare);
                bassExtensionTarget = 1.0 + 0.045 * bassLean
                    - 0.040 * bassHeavy
                    - 0.010 * bassHeavy * mudRisk
                    - 0.006 * bassHeavy * transientRisk;
                const double coreSideRatio = warm_core_side_env_ / (core + 1.0e-9);
                const double sideExists = smoothstep(0.025, 0.22, coreSideRatio);
                const double sideRisk = smoothstep(0.58, 1.05, coreSideRatio);
                warm_particle_side_context_ = sideExists * (1.0 - sideRisk);
                const double lowmidSideRatio = warm_lowmid_side_env_
                    / (body + mud + 1.0e-9);
                const double lowmidSideExists = smoothstep(0.018, 0.16, lowmidSideRatio);
                const double lowmidSideRisk = smoothstep(0.44, 0.86, lowmidSideRatio);
                warm_lowmid_side_context_ = lowmidSideExists
                    * (1.0 - lowmidSideRisk);
            } else {
                warm_particle_side_context_ = 0.0;
                warm_lowmid_side_context_ = 0.0;
            }

            double vocalBodyCompanionAlpha =
                vocalBodyCompanionTarget > vocal_body_companion_permission_
                    ? vocal_body_companion_attack_alpha_
                    : vocal_body_companion_release_alpha_;
            if (vocalBodyCompanionTarget < vocal_body_companion_permission_
                && vocalBodyMaterialReject)
                vocalBodyCompanionAlpha = vocal_body_companion_reject_alpha_;
            vocal_body_companion_permission_ += vocalBodyCompanionAlpha
                * (vocalBodyCompanionTarget
                   - vocal_body_companion_permission_);
            adaptive_state_.vocal_body_companion =
                vocal_body_companion_permission_;

            double maleVocalBodyAlpha =
                maleVocalBodyTarget > male_vocal_body_permission_
                    ? male_vocal_body_companion_attack_alpha_
                    : male_vocal_body_companion_release_alpha_;
            if (maleVocalBodyTarget < male_vocal_body_permission_
                && maleVocalBodyMaterialReject)
                maleVocalBodyAlpha = male_vocal_body_companion_reject_alpha_;
            male_vocal_body_permission_ += maleVocalBodyAlpha
                * (maleVocalBodyTarget - male_vocal_body_permission_);
            adaptive_state_.male_vocal_body_companion =
                male_vocal_body_permission_;

            warm_core_permission_ = movePermission(
                warm_core_permission_, clamp(coreTarget, 0.20, 1.0),
                warm_control_attack_alpha_, warm_control_release_alpha_);
            warm_body_permission_ = movePermission(
                warm_body_permission_, clamp(bodyTarget, 0.34, 1.0),
                warm_control_attack_alpha_, warm_control_release_alpha_);
            warm_upper_permission_ = movePermission(
                warm_upper_permission_, clamp(upperTarget, 0.36, 1.0),
                warm_control_attack_alpha_, warm_control_release_alpha_);
            warm_particle_permission_ = movePermission(
                warm_particle_permission_, clamp(particleTarget, 0.28, 1.0),
                warm_particle_attack_alpha_, warm_particle_release_alpha_);
            warm_lowmid_particle_permission_ = movePermission(
                warm_lowmid_particle_permission_,
                clamp(lowmidParticleTarget, 0.30, 1.0),
                warm_particle_attack_alpha_, warm_particle_release_alpha_);
            adaptive_bass_permission_ = movePermission(
                adaptive_bass_permission_, clamp(bassExtensionTarget, 0.92, 1.045),
                adaptive_content_reduce_alpha_, adaptive_content_restore_alpha_);
            adaptive_state_.bass_extension = adaptive_bass_permission_;
            adaptive_state_.warm_particles = warm_particle_permission_;
            adaptive_state_.lowmid_particles = warm_lowmid_particle_permission_;
        }

        core_wet = runtime.core_foundation_wet
                 + runtime.core_extension_wet * warm_core_permission_;
        body_wet = runtime.body_foundation_wet
                 + runtime.body_extension_wet * warm_body_permission_;
        upper_body_wet = runtime.upper_body_foundation_wet
                       + runtime.upper_body_extension_wet
                       * warm_upper_permission_;

        // Core-only even-harmonic tickle. Restricting the source to 72-150 Hz
        // places the useful second harmonic mainly in 144-300 Hz, where it
        // improves audibility without broadband saturation or oversampling.
        const double core = std::max(1.0e-9, warm_core_slow_env_);
        const double particle_scale = std::max(8.0e-4, core * 2.20);
        const double normalised_particle = clamp(
            static_cast<double>(core_mid) / particle_scale, -1.15, 1.15);
        const float even_residue = static_cast<float>(
            normalised_particle * normalised_particle * particle_scale);
        warm_particle = warm_particle_post_lp_.process(
            warm_particle_dc_hp_.process(even_residue));

        const double sideScale = std::max(4.0e-4, warm_core_side_env_ * 2.20);
        const double normalisedSide = clamp(
            static_cast<double>(core_side) / sideScale, -1.15, 1.15);
        const float evenSideResidue = static_cast<float>(
            normalisedSide * normalisedSide * sideScale);
        const float warmParticleSide = static_cast<float>(
            warm_particle_side_hp_coeff_
            * (static_cast<double>(warm_particle_side_hp_y1_)
               + static_cast<double>(evenSideResidue - warm_particle_side_hp_x1_)));
        warm_particle_side_hp_x1_ = evenSideResidue;
        warm_particle_side_hp_y1_ = warmParticleSide;
        const float sideContribution = harmonic_ceiling(
            warmParticleSide * static_cast<float>(runtime.warm_particle_wet
                * runtime.warm_particle_side_mix * warm_particle_permission_
                * warm_particle_side_context_),
            warm_core_side_env_, 0.0090);
        warm_particle_side_cached_ = sideContribution;

        if (runtime.lowmid_particle_wet > 0.0) {
            // Reuse existing analysis/audio buses. The 140-300 Hz body plus
            // narrow 250-360 Hz density source produces useful 280-720 Hz H2.
            const float lowmidSource = 0.72f * low_body + 0.28f * mud;
            const double lowmidEnv = std::max(1.0e-9,
                0.72 * warm_living_body_env_ + 0.28 * warm_mud_env_);
            const float rawLowmid = even_harmonic_particle(lowmidSource, lowmidEnv);
            lowmid_particle = static_cast<float>(
                warm_lowmid_particle_hp_coeff_
                * (static_cast<double>(warm_lowmid_particle_hp_y1_)
                   + static_cast<double>(rawLowmid - warm_lowmid_particle_hp_x1_)));
            warm_lowmid_particle_hp_x1_ = rawLowmid;
            warm_lowmid_particle_hp_y1_ = lowmid_particle;

            const float rawLowmidSide = even_harmonic_particle(
                low_body_side, warm_lowmid_side_env_);
            const float lowmidSideParticle = static_cast<float>(
                warm_lowmid_particle_hp_coeff_
                * (static_cast<double>(warm_lowmid_particle_side_hp_y1_)
                   + static_cast<double>(rawLowmidSide
                                         - warm_lowmid_particle_side_hp_x1_)));
            warm_lowmid_particle_side_hp_x1_ = rawLowmidSide;
            warm_lowmid_particle_side_hp_y1_ = lowmidSideParticle;
            warm_lowmid_particle_side_cached_ = harmonic_ceiling(
                lowmidSideParticle * static_cast<float>(
                    runtime.lowmid_particle_wet
                    * runtime.lowmid_particle_side_mix
                    * warm_lowmid_particle_permission_
                    * warm_lowmid_side_context_),
                warm_lowmid_side_env_, 0.0048);
        } else {
            warm_lowmid_particle_side_cached_ = 0.0f;
        }
    }

    if (runtime.bass_active) {
        float bass_l = l;
        float bass_r = r;
        soft_saturate_pair(bass_l, bass_r, runtime.bass);
        bass_l = bass_post_lp_[0].process(bass_post_hp_[0].process(bass_l));
        bass_r = bass_post_lp_[1].process(bass_post_hp_[1].process(bass_r));
        add_l += static_cast<float>(bass_l * runtime.bass_wet
                                  * adaptive_bass_permission_);
        add_r += static_cast<float>(bass_r * runtime.bass_wet
                                  * adaptive_bass_permission_);
    }

    if (runtime.warm_active) {
        add_l += static_cast<float>(core_out_l * core_wet);
        add_r += static_cast<float>(core_out_r * core_wet);
    }

    if (runtime.upper_body_active) {
        add_l += static_cast<float>(upper_body_out_l * upper_body_wet);
        add_r += static_cast<float>(upper_body_out_r * upper_body_wet);
        const float vocalBodyCompanion = vocal_body_companion_source
            * static_cast<float>(runtime.vocal_body_companion_wet
                * vocal_body_companion_permission_ * scene_body_);
        add_l += vocalBodyCompanion;
        add_r += vocalBodyCompanion;

#if defined(ASKP_ENGINEERING_TEST_HOOKS)
        const double maleVocalBodyTestScale =
            engineering_male_vocal_body_enabled_ ? 1.0 : 0.0;
#else
        constexpr double maleVocalBodyTestScale = 1.0;
#endif
        const float maleVocalBodyCompanion = male_vocal_body_source
            * static_cast<float>(runtime.male_vocal_body_wet
                * male_vocal_body_permission_ * scene_body_
                * maleVocalBodyTestScale);
        add_l += maleVocalBodyCompanion;
        add_r += maleVocalBodyCompanion;
    }

    if (runtime.smart_warm_active && runtime.warm_particle_wet > 0.0) {
        // Sparse-program support is deliberately Mid-only. It keeps bass and
        // low-mid audibility particles useful in an intro without widening the
        // lows or changing the stable linear Core/Body foundation.
        const double quietBodySupport =
            1.0 + 0.48 * (low_level_particle_support_ - 1.0);
        const float contribution = harmonic_ceiling(
            warm_particle * static_cast<float>(runtime.warm_particle_wet
                           * warm_particle_permission_
                           * quietBodySupport
                           * scene_body_),
            warm_core_slow_env_, 0.0680);
        add_l += contribution
            + warm_particle_side_cached_ * static_cast<float>(scene_space_);
        add_r += contribution
            - warm_particle_side_cached_ * static_cast<float>(scene_space_);

        if (runtime.lowmid_particle_wet > 0.0) {
            const double lowmidEnv = std::max(1.0e-9,
                0.72 * warm_living_body_env_ + 0.28 * warm_mud_env_);
            const float lowmidContribution = harmonic_ceiling(
                lowmid_particle * static_cast<float>(
                    runtime.lowmid_particle_wet
                    * warm_lowmid_particle_permission_
                    * quietBodySupport
                    * scene_body_),
                lowmidEnv, 0.0120);
            add_l += lowmidContribution
                + warm_lowmid_particle_side_cached_
                    * static_cast<float>(scene_space_);
            add_r += lowmidContribution
                - warm_lowmid_particle_side_cached_
                    * static_cast<float>(scene_space_);
        }
    }

    float presence_source_l = 0.0f;
    float presence_source_r = 0.0f;
    float air_source_l = 0.0f;
    float air_source_r = 0.0f;
    float tickle_mid = 0.0f;
    float tickle_side = 0.0f;
    float skin_mid = 0.0f;
    float skin_side = 0.0f;
    const bool needsPresenceSource = runtime.presence_active || runtime.tickle_active;
    const bool needsAirSource = runtime.air_active || runtime.skin_active;
    if (needsPresenceSource) {
        presence_source_l = presence_pre_lp_[0].process(presence_pre_[0].process(l));
        presence_source_r = presence_pre_lp_[1].process(presence_pre_[1].process(r));
        if (runtime.tickle_active) {
            tickle_mid = vocal_tickle_lp_.process(
                0.5f * (presence_source_l + presence_source_r));
            // Side Tickle reuses the already band-limited Presence Side bus.
            tickle_side = 0.5f * (presence_source_l - presence_source_r);
        }
    }
    if (needsAirSource) {
        air_source_l = air_pre_lp_[0].process(air_pre_[0].process(l));
        air_source_r = air_pre_lp_[1].process(air_pre_[1].process(r));
        if (runtime.skin_active) {
            skin_mid = treble_skin_bp_.process(
                0.5f * (air_source_l + air_source_r));
            // Side Skin reuses Air Side; its context/ceiling keeps the broader
            // source from becoming fizzy.
            skin_side = 0.5f * (air_source_l - air_source_r);
        }
    }

    // P4: each high family is measured on its own band-limited Mid/Side source.
    // A cymbal-heavy Air event no longer mutes Presence, and a vocal consonant
    // no longer removes the stable Air polish. Context is updated at 1/32 rate.
    high_analysis_phase_ = !high_analysis_phase_;
    if ((needsPresenceSource || needsAirSource) && high_analysis_phase_) {
        const double pMid = 0.5 * static_cast<double>(presence_source_l + presence_source_r);
        const double pSide = 0.5 * static_cast<double>(presence_source_l - presence_source_r);
        const double aMid = 0.5 * static_cast<double>(air_source_l + air_source_r);
        const double aSide = 0.5 * static_cast<double>(air_source_l - air_source_r);
        const double pNow = std::abs(pMid) + 0.62 * std::abs(pSide);
        const double aNow = std::abs(aMid) + 0.66 * std::abs(aSide);
        const double inputNow = 0.5 * (std::abs(static_cast<double>(l))
                                     + std::abs(static_cast<double>(r)));

        presence_fast_env_ += high_fast_alpha_ * (pNow - presence_fast_env_);
        presence_slow_env_ += high_slow_alpha_ * (pNow - presence_slow_env_);
        presence_mid_env_ += high_slow_alpha_ * (std::abs(pMid) - presence_mid_env_);
        presence_side_env_ += high_slow_alpha_ * (std::abs(pSide) - presence_side_env_);
        tickle_env_ += high_slow_alpha_ * (std::abs(static_cast<double>(tickle_mid))
                                           + 0.55 * std::abs(static_cast<double>(tickle_side))
                                           - tickle_env_);
        air_fast_env_ += high_fast_alpha_ * (aNow - air_fast_env_);
        air_slow_env_ += high_slow_alpha_ * (aNow - air_slow_env_);
        air_mid_env_ += high_slow_alpha_ * (std::abs(aMid) - air_mid_env_);
        air_side_env_ += high_slow_alpha_ * (std::abs(aSide) - air_side_env_);
        skin_env_ += high_slow_alpha_ * (std::abs(static_cast<double>(skin_mid))
                                        + 0.55 * std::abs(static_cast<double>(skin_side))
                                        - skin_env_);
        high_input_fast_env_ += high_fast_alpha_
                              * (inputNow - high_input_fast_env_);
        high_input_env_ += high_slow_alpha_ * (inputNow - high_input_env_);

        if (++high_control_counter_ >= 16) {
            high_control_counter_ = 0;
            auto smoothstep = [](double edge0, double edge1, double value) {
                const double x = clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
                return x * x * (3.0 - 2.0 * x);
            };
            auto movePermission = [&](double current, double target) {
                const double alpha = target < current
                    ? high_control_attack_alpha_ : high_control_release_alpha_;
                return current + alpha * (target - current);
            };

            const double pTransient = smoothstep(1.50, 3.00,
                presence_fast_env_ / (presence_slow_env_ + 1.0e-9));
            const double pDensity = smoothstep(0.16, 0.38,
                presence_slow_env_ / (high_input_env_ + 1.0e-9));
            const double tickleDensity = smoothstep(0.38, 0.95,
                tickle_env_ / (presence_slow_env_ + 1.0e-9));
            const double aTransient = smoothstep(1.42, 2.75,
                air_fast_env_ / (air_slow_env_ + 1.0e-9));
            const double aDensity = smoothstep(0.13, 0.34,
                air_slow_env_ / (high_input_env_ + 1.0e-9));
            const double skinDensity = smoothstep(0.42, 1.02,
                skin_env_ / (air_slow_env_ + 1.0e-9));

            const double presenceRisk = clamp(0.48 * pTransient
                                             + 0.31 * pDensity
                                             + 0.10 * skinDensity, 0.0, 1.0);
            const double tickleRisk = clamp(0.56 * pTransient
                                           + 0.33 * tickleDensity
                                           + 0.18 * skinDensity, 0.0, 1.0);
            const double airRisk = clamp(0.22 * pTransient
                                        + 0.36 * aTransient
                                        + 0.38 * aDensity
                                        + 0.18 * skinDensity, 0.0, 1.0);
            const double skinRisk = clamp(0.28 * aTransient
                                         + 0.30 * aDensity
                                         + 0.58 * skinDensity, 0.0, 1.0);

            presence_permission_ = movePermission(presence_permission_,
                clamp(1.0 - runtime.high_guard_depth * 0.78 * presenceRisk, 0.18, 1.0));
            tickle_permission_ = movePermission(tickle_permission_,
                clamp(1.0 - runtime.high_guard_depth * 0.94 * tickleRisk, 0.12, 1.0));
            air_permission_ = movePermission(air_permission_,
                clamp(1.0 - runtime.high_guard_depth * 0.88 * airRisk, 0.14, 1.0));
            skin_permission_ = movePermission(skin_permission_,
                clamp(1.0 - runtime.high_guard_depth * skinRisk, 0.10, 1.0));

            // P7D Signature vitality recovery. The P4 fast permissions above
            // are already the transient/harshness safety layer. These slower
            // scalars therefore keep normal material at the approved P7A/P7B
            // recipe and move below unity only on sustained spectral excess.
            // This avoids the P7C double-restraint that made balanced material
            // slightly less alive. Sparse lifts remain Mid-focused.
            const double pShare = presence_slow_env_ / (high_input_env_ + 1.0e-9);
            const double aShare = air_slow_env_ / (high_input_env_ + 1.0e-9);
            adaptive_state_.presence_density = pShare;
            adaptive_state_.air_density = aShare;
            const double presenceNeed = 1.0 - smoothstep(0.20, 0.42, pShare);
            const double presenceExcess = smoothstep(0.60, 0.82, pShare);
            const double airNeed = 1.0 - smoothstep(0.16, 0.34, aShare);
            const double airExcess = smoothstep(0.48, 0.65, aShare);
            const double tickleExcess = smoothstep(0.72, 0.98, tickleDensity);
            const double skinExcess = smoothstep(0.72, 0.98, skinDensity);
            // Preset density tolerance adjusts only the slow excess decision.
            // Fast transient/harshness guards remain fully authoritative.
            const double densityScale = 1.0 / std::max(0.82, scene_density_);
            const double profiledPresenceExcess =
                clamp(presenceExcess * densityScale, 0.0, 1.0);
            const double profiledAirExcess =
                clamp(airExcess * densityScale, 0.0, 1.0);
            const double profiledTickleExcess =
                clamp(tickleExcess * densityScale, 0.0, 1.0);
            const double profiledSkinExcess =
                clamp(skinExcess * densityScale, 0.0, 1.0);
            // P7F low-level vitality consistency. Nonlinear residue naturally
            // falls faster than dry programme level, which made sparse/quiet
            // passages sound almost unchanged while loud choruses came alive.
            // Reuse the existing broadband envelope and control cadence: this
            // adds no FFT, filter, allocation or per-sample transcendental math.
            // Support falls rapidly when level rises and restores slowly so
            // phrase gaps cannot make the tone flutter.
            const bool audibleProgram = high_input_env_ > 3.0e-4;
            const double programmeLevel = std::max(
                high_input_env_, 0.65 * high_input_fast_env_);
            const double quietProgramNeed = audibleProgram
                ? 1.0 - smoothstep(0.018, 0.090, programmeLevel)
                : 0.0;
            const double spectralExcess = std::max(
                profiledPresenceExcess, profiledAirExcess);
            const double lowLevelSupportTarget = 1.0
                + 0.34 * quietProgramNeed * (1.0 - 0.55 * spectralExcess);
            const double lowLevelAlpha =
                lowLevelSupportTarget < low_level_particle_support_
                    ? low_level_support_reduce_alpha_
                    : adaptive_content_restore_alpha_;
            low_level_particle_support_ += lowLevelAlpha
                * (lowLevelSupportTarget - low_level_particle_support_);
            adaptive_state_.low_level_support =
                low_level_particle_support_;
            const double presenceNeedTarget = clamp(
                1.0 + 0.040 * presenceNeed
                    - 0.105 * profiledPresenceExcess,
                0.895, 1.04);
            const double tickleNeedTarget = clamp(
                1.028 + 0.037 * presenceNeed
                    - 0.105 * profiledPresenceExcess
                    - 0.025 * profiledTickleExcess
                    - 0.012 * profiledSkinExcess,
                0.88, 1.06);
            const double airNeedTarget = clamp(
                1.0 + 0.035 * airNeed
                    - 0.105 * profiledAirExcess
                    - 0.015 * profiledSkinExcess,
                0.88, 1.035);
            const double skinNeedTarget = clamp(
                1.0 + 0.020 * airNeed
                    - 0.115 * profiledAirExcess
                    - 0.035 * profiledSkinExcess,
                0.85, 1.02);
            auto moveContent = [&](double current, double target) {
                const double alpha = target < current
                    ? adaptive_content_reduce_alpha_
                    : adaptive_content_restore_alpha_;
                return current + alpha * (target - current);
            };
            adaptive_presence_need_ = moveContent(
                adaptive_presence_need_, presenceNeedTarget);
            adaptive_tickle_need_ = moveContent(
                adaptive_tickle_need_, tickleNeedTarget);
            adaptive_air_need_ = moveContent(
                adaptive_air_need_, airNeedTarget);
            adaptive_skin_need_ = moveContent(
                adaptive_skin_need_, skinNeedTarget);
            adaptive_state_.presence_particles = adaptive_presence_need_;
            adaptive_state_.tickle_particles = adaptive_tickle_need_;
            adaptive_state_.air_particles = adaptive_air_need_;
            adaptive_state_.skin_particles = adaptive_skin_need_;

            // P7E Safe Particle Articulation. The detector deliberately selects
            // the middle of the transient range: steady passages never receive
            // lift, while extreme events stay under the existing P4 guards.
            // Density/excess gates collapse every target to unity before a
            // bright or crowded texture can be exaggerated.
            const double presenceTransientWindow =
                smoothstep(0.025, 0.18, pTransient)
                * (1.0 - smoothstep(0.78, 0.99, pTransient));
            const double airTransientWindow =
                smoothstep(0.06, 0.28, aTransient)
                * (1.0 - smoothstep(0.64, 0.96, aTransient));
            const double presenceArticulation =
                presenceTransientWindow
                * (1.0 - 0.50 * pDensity)
                * (1.0 - profiledPresenceExcess)
                * (1.0 - 0.55 * profiledTickleExcess)
                * (1.0 - 0.30 * profiledSkinExcess);
            const double airArticulation =
                airTransientWindow
                * (1.0 - 0.70 * aDensity)
                * (1.0 - profiledAirExcess)
                * (1.0 - 0.62 * profiledSkinExcess);
            auto moveArticulation = [&](double current, double target) {
                const double alpha = target > current
                    ? microdetail_articulation_attack_alpha_
                    : microdetail_articulation_release_alpha_;
                return current + alpha * (target - current);
            };
            microdetail_presence_articulation_ = moveArticulation(
                microdetail_presence_articulation_,
                1.0 + 0.052 * presenceArticulation * scene_articulation_);
            microdetail_tickle_articulation_ = moveArticulation(
                microdetail_tickle_articulation_,
                1.0 + 0.078 * presenceArticulation * scene_articulation_);
            microdetail_air_articulation_ = moveArticulation(
                microdetail_air_articulation_,
                1.0 + 0.024 * airArticulation * scene_articulation_);
            microdetail_skin_articulation_ = moveArticulation(
                microdetail_skin_articulation_,
                1.0 + 0.030 * airArticulation * scene_articulation_);
            adaptive_state_.presence_articulation =
                microdetail_presence_articulation_;
            adaptive_state_.tickle_articulation =
                microdetail_tickle_articulation_;
            adaptive_state_.air_articulation =
                microdetail_air_articulation_;
            adaptive_state_.skin_articulation =
                microdetail_skin_articulation_;

            const double pRatio = presence_side_env_ / (presence_mid_env_ + 1.0e-9);
            const double aRatio = air_side_env_ / (air_mid_env_ + 1.0e-9);
            presence_side_context_ = smoothstep(0.035, 0.30, pRatio)
                * (1.0 - smoothstep(0.82, 1.30, pRatio));
            air_side_context_ = smoothstep(0.030, 0.26, aRatio)
                * (1.0 - smoothstep(0.90, 1.42, aRatio));
        }
    }

    float high_add_l = 0.0f;
    float high_add_r = 0.0f;
    if (needsPresenceSource) {
        const float pMid = 0.5f * (presence_source_l + presence_source_r);
        const float pSide = 0.5f * (presence_source_l - presence_source_r);
        float residueMid = 0.0f;
        float residueSide = 0.0f;
        float rawPresenceSide = 0.0f;

        if (runtime.presence_active) {
            float rawPresenceMid = pMid;
            rawPresenceSide = pSide;
            harmonic_residue_pair(rawPresenceMid, rawPresenceSide,
                                  runtime.presence);
            rawPresenceMid += even_harmonic_particle(pMid, presence_mid_env_);
            rawPresenceSide += even_harmonic_particle(pSide, presence_side_env_);
            const double permission = runtime.presence_floor
                + (1.0 - runtime.presence_floor) * presence_permission_;
            residueMid += rawPresenceMid
                * static_cast<float>(runtime.presence_wet * 1.22 * permission
                    * adaptive_presence_need_
                    * low_level_particle_support_
                    * microdetail_presence_articulation_
                    * scene_detail_);
            residueSide += rawPresenceSide
                * static_cast<float>(runtime.presence_wet * 1.22 * permission
                    * std::min(1.0, adaptive_presence_need_)
                    * runtime.presence_side_mix * presence_side_context_
                    * scene_space_);
        }

        if (runtime.tickle_active) {
            const float tickleMidResidue = harmonic_residue(tickle_mid, runtime.tickle)
                + even_harmonic_particle(tickle_mid, tickle_env_);
            // Default presets already run Presence. Reusing that Side residue
            // removes another nonlinear call while retaining a distinct Tickle
            // amount. The fallback covers unusual Tickle-only custom states.
            const float tickleSideResidue = runtime.presence_active
                ? rawPresenceSide
                : harmonic_residue(tickle_side, runtime.tickle);
            const double permission = runtime.tickle_floor
                + (1.0 - runtime.tickle_floor) * tickle_permission_;
            residueMid += tickleMidResidue
                * static_cast<float>(runtime.tickle_wet * 0.62 * permission
                    * adaptive_tickle_need_
                    * low_level_particle_support_
                    * microdetail_tickle_articulation_
                    * scene_detail_);
            residueSide += tickleSideResidue
                * static_cast<float>(runtime.tickle_wet * 0.62 * permission
                    * std::min(1.0, adaptive_tickle_need_)
                    * runtime.tickle_side_mix * presence_side_context_
                    * scene_space_);
        }

        residueMid = harmonic_ceiling(residueMid, presence_mid_env_, 0.0300);
        residueSide = harmonic_ceiling(residueSide, presence_side_env_, 0.0058);
        float residueL = residueMid + residueSide;
        float residueR = residueMid - residueSide;
        residueL = presence_post_lp_[0].process(presence_post_hp_[0].process(residueL));
        residueR = presence_post_lp_[1].process(presence_post_hp_[1].process(residueR));
        high_add_l += residueL;
        high_add_r += residueR;
    }

    if (needsAirSource) {
        const float aMid = 0.5f * (air_source_l + air_source_r);
        const float aSide = 0.5f * (air_source_l - air_source_r);
        float residueMid = 0.0f;
        float residueSide = 0.0f;
        float rawAirSide = 0.0f;

        if (runtime.air_active) {
            float rawAirMid = aMid;
            rawAirSide = aSide;
            harmonic_residue_pair(rawAirMid, rawAirSide, runtime.air);
            rawAirMid += even_harmonic_particle(aMid, air_mid_env_);
            rawAirSide += even_harmonic_particle(aSide, air_side_env_);
            const double permission = runtime.air_floor
                + (1.0 - runtime.air_floor) * air_permission_;
            residueMid += rawAirMid
                * static_cast<float>(runtime.air_wet * 1.48 * permission
                    * adaptive_air_need_
                    * (1.0 + 0.72 * (low_level_particle_support_ - 1.0))
                    * microdetail_air_articulation_
                    * scene_detail_);
            residueSide += rawAirSide
                * static_cast<float>(runtime.air_wet * 1.48 * permission
                    * std::min(1.0, adaptive_air_need_)
                    * runtime.air_side_mix * air_side_context_
                    * scene_space_);
        }

        if (runtime.skin_active) {
            const float skinMidResidue = harmonic_residue(skin_mid, runtime.skin)
                + even_harmonic_particle(skin_mid, skin_env_);
            const float skinSideResidue = runtime.air_active
                ? rawAirSide : harmonic_residue(skin_side, runtime.skin);
            const double permission = runtime.skin_floor
                + (1.0 - runtime.skin_floor) * skin_permission_;
            residueMid += skinMidResidue
                * static_cast<float>(runtime.skin_wet * 1.38 * permission
                    * adaptive_skin_need_
                    * (1.0 + 0.58 * (low_level_particle_support_ - 1.0))
                    * microdetail_skin_articulation_
                    * scene_detail_);
            residueSide += skinSideResidue
                * static_cast<float>(runtime.skin_wet * 1.38 * permission
                    * std::min(1.0, adaptive_skin_need_)
                    * runtime.skin_side_mix * air_side_context_
                    * scene_space_);
        }

        residueMid = harmonic_ceiling(residueMid, air_mid_env_, 0.0300);
        residueSide = harmonic_ceiling(residueSide, air_side_env_, 0.0068);
        float residueL = residueMid + residueSide;
        float residueR = residueMid - residueSide;
        residueL = air_post_lp_[0].process(air_post_hp_[0].process(residueL));
        residueR = air_post_lp_[1].process(air_post_hp_[1].process(residueR));
        high_add_l += residueL;
        high_add_r += residueR;
    }

    // De-harsh only the generated high residue. The dry programme remains
    // untouched, preventing the old failure mode where protection outlived the
    // particles and made a bright mix sound darker or less polished.
    if (needsPresenceSource || needsAirSource) {
        high_add_l = deharsh_[0].process(high_add_l);
        high_add_r = deharsh_[1].process(high_add_r);
        add_l += high_add_l;
        add_r += high_add_r;
    }

    const bool needsMidSide = runtime.side_air_active || runtime.mid_active
        || runtime.low_body_active;
    if (needsMidSide) {
        if (runtime.side_air_active) {
            float side_air = side_air_tone_.process(side_air_hp_.process(side));
            side_air = soft_saturate(side_air, runtime.side_air);
            const float contribution = static_cast<float>(side_air * runtime.side_air_wet
                * (runtime.air_floor + (1.0 - runtime.air_floor) * air_permission_)
                * std::min(1.0, adaptive_air_need_)
                * (0.55 + 0.45 * air_side_context_));
            add_l += contribution;
            add_r -= contribution;
        }
        if (runtime.mid_active) {
            float mid_anchor = mid_anchor_bp2_.process(mid_anchor_bp1_.process(mid));
            mid_anchor = soft_saturate(mid_anchor, runtime.mid_anchor);
            const float contribution = static_cast<float>(mid_anchor * runtime.mid_wet);
            add_l += contribution;
            add_r += contribution;
        }
        if (runtime.low_body_active) {
            add_l += static_cast<float>(body_out_l * body_wet);
            add_r += static_cast<float>(body_out_r * body_wet);
        }
    }

    l += add_l;
    r += add_r;

    l = static_cast<float>(l * runtime.compensation);
    r = static_cast<float>(r * runtime.compensation);

    // Keep direct musical texture alive, matching the extension's color dry gain policy.
    l = static_cast<float>(dry_l * runtime.dry_keep + (l - dry_l));
    r = static_cast<float>(dry_r * runtime.dry_keep + (r - dry_r));
    return {sane(l), sane(r)};
}

std::pair<float,float> ArSonKuPikEngine::process_width(float l, float r)
{
    const auto& runtime = width_runtime_;
    if (!runtime.active) return {l, r};

    const double input_mid = 0.5 * (static_cast<double>(l) + static_cast<double>(r));
    const double input_side = 0.5 * (static_cast<double>(l) - static_cast<double>(r));

    // Center-based Stereo knob: left of centre is a true narrowing path. At the
    // far-left endpoint, side scale reaches zero and the output becomes total mono.
    if (runtime.narrow) {
        const double out_l = input_mid + input_side * runtime.side_scale;
        const double out_r = input_mid - input_side * runtime.side_scale;
        return {sane(static_cast<float>(out_l)), sane(static_cast<float>(out_r))};
    }

    const double mid = input_mid;
    const double side = input_side;
    const double mid_abs = std::abs(mid);
    const double side_abs = std::abs(side);

    width_mid_env_ += width_slow_alpha_ * (mid_abs - width_mid_env_);
    width_side_env_ += width_slow_alpha_ * (side_abs - width_side_env_);
    width_side_fast_env_ += width_fast_alpha_ * (side_abs - width_side_fast_env_);
    width_side_slow_env_ += width_slow_alpha_ * (side_abs - width_side_slow_env_);

    const double ld = static_cast<double>(l);
    const double rd = static_cast<double>(r);
    width_corr_dot_accum_ += ld * rd;
    width_corr_l_energy_accum_ += ld * ld;
    width_corr_r_energy_accum_ += rd * rd;
    if (++width_corr_counter_ >= kFastControlInterval) {
        const double denom = std::sqrt(std::max(
            1.0e-24, width_corr_l_energy_accum_ * width_corr_r_energy_accum_));
        const double corr_now = denom > 0.0
                              ? clamp(width_corr_dot_accum_ / denom, -1.0, 1.0)
                              : 1.0;
        width_corr_env_ += width_corr_alpha_ * (corr_now - width_corr_env_);
        width_corr_dot_accum_ = 0.0;
        width_corr_l_energy_accum_ = 0.0;
        width_corr_r_energy_accum_ = 0.0;
        width_corr_counter_ = 0;
    }

    // Existing Side is split first. The same fixed filters also feed P3's
    // band-context detector; no analysis-only biquad or FFT is added.
    const float side_f = static_cast<float>(side);
    const float side_sub = width_side_sub_lp_.process(width_side_sub_hp_.process(side_f));
    const float side_body = width_side_body_lp_.process(width_side_body_hp_.process(side_f));
    const float side_mid = width_side_mid_lp_.process(width_side_mid_hp_.process(side_f));
    const float side_high = width_side_high_lp_.process(width_side_high_hp_.process(side_f));
    float side_air = width_side_air_tone_.process(width_side_air_hp_.process(side_f));
    side_air = soft_saturate(side_air, runtime.side_air);

    // Generated phase-safe filler from Mid. P3 extends the low-mid source to
    // 240-1100 Hz so vocal throat, guitar growl and first talk-box formants are
    // analysed as one continuous body bridge.
    const float generated = static_cast<float>(mid);
    float lowmid_gen = 0.0f;
    float mid_gen = 0.0f;
    float air_gen = 0.0f;
    if (runtime.gen_lowmid_active)
        lowmid_gen = width_lowmid_lp_.process(width_lowmid_hp_.process(generated));
    if (runtime.gen_mid_active)
        mid_gen = width_mid_lp_.process(width_mid_hp_.process(generated));
    if (runtime.gen_air_active)
        air_gen = width_air_tone_.process(width_high_hp_.process(generated));

    width_body_mid_env_ += width_slow_alpha_
        * (std::abs(static_cast<double>(lowmid_gen)) - width_body_mid_env_);
    width_body_side_env_ += width_slow_alpha_
        * (std::abs(static_cast<double>(side_body)) - width_body_side_env_);
    width_projection_mid_env_ += width_slow_alpha_
        * (std::abs(static_cast<double>(mid_gen)) - width_projection_mid_env_);
    width_projection_side_env_ += width_slow_alpha_
        * (std::abs(static_cast<double>(side_mid)) - width_projection_side_env_);

    // Ratios and context gains move far more slowly than the waveform. Update
    // them once per 16 samples and reuse the cached values inside the audio loop.
    if (width_context_counter_ == 0) {
        auto smoothstep = [](double edge0, double edge1, double value)
        {
            const double x = clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
            return x * x * (3.0 - 2.0 * x);
        };

        const double sideRatio = width_side_env_ / (width_mid_env_ + 1.0e-6);
        const double transientScore = clamp(
            (width_side_fast_env_ / (width_side_slow_env_ + 1.0e-6) - 1.0)
                * 0.48,
            0.0, 1.0);
        const double panEvent = clamp((sideRatio - 0.34) / 0.78, 0.0, 1.0)
                              * clamp((width_corr_env_ + 0.08) / 0.58,
                                      0.0, 1.0);
        const double antiPhase = clamp((0.18 - width_corr_env_) / 0.55,
                                       0.0, 1.0);
        const double corrGuard = clamp((width_corr_env_ + 0.12) / 0.52,
                                       0.0, 1.0);
        const double lowRisk = antiPhase * runtime.source_protect
                             * (1.0 - transientScore * 0.72)
                             * clamp((sideRatio - 0.20) / 0.55, 0.0, 1.0);
        width_low_reduce_cached_ = clamp(lowRisk * 0.68, 0.0, 0.68);

        // Smart Mid-Low Context states:
        // - narrowNeed: useful body exists in Mid but little original Side;
        // - existingStereo: double-tracked guitars/room width is healthy;
        // - sideExcess: Side dominates Mid and requires protection.
        const double bodySideRatio = width_body_side_env_
                                   / (width_body_mid_env_ + 1.0e-6);
        const double projectionSideRatio = width_projection_side_env_
                                         / (width_projection_mid_env_ + 1.0e-6);
        const double bodyNarrowNeed = 1.0
            - smoothstep(0.10, 0.42, bodySideRatio);
        const double bodyExistingStereo = smoothstep(0.12, 0.60, bodySideRatio)
            * (1.0 - smoothstep(1.18, 1.78, bodySideRatio));
        const double bodySideExcess = smoothstep(1.05, 1.78, bodySideRatio);
        const double projectionNarrowNeed = 1.0
            - smoothstep(0.10, 0.44, projectionSideRatio);
        const double projectionExistingStereo = smoothstep(
            0.14, 0.66, projectionSideRatio)
            * (1.0 - smoothstep(1.22, 1.88, projectionSideRatio));
        const double projectionSideExcess = smoothstep(
            1.08, 1.88, projectionSideRatio);
        const double bodyAntiPhaseRisk = antiPhase
            * smoothstep(0.55, 0.98, bodySideRatio);
        const double projectionAntiPhaseRisk = antiPhase
            * smoothstep(0.58, 1.02, projectionSideRatio);

        // Band-limited lead probability. Kick and bass no longer masquerade as
        // vocal and switch off guitar/talk-box widening.
        const double projectionCenterDominance = width_projection_mid_env_
            / (width_projection_side_env_ + 1.0e-6);
        const double leadProbability = smoothstep(
            1.85, 4.60, projectionCenterDominance)
            * (1.0 - transientScore * 0.35);

        const double bodyVitalityTarget = clamp(
            0.72 + 0.32 * bodyNarrowNeed + 0.18 * bodyExistingStereo
                 - 0.62 * bodySideExcess,
            0.24, 1.12);
        const double projectionVitalityTarget = clamp(
            0.72 + 0.30 * projectionNarrowNeed
                 + 0.16 * projectionExistingStereo
                 - 0.60 * projectionSideExcess,
            0.24, 1.10);
        // Context gain is already control-rate; use the equivalent 16-sample
        // smoothing coefficient so its 55 ms time constant remains unchanged.
        const double contextAlpha = 1.0
            - std::pow(1.0 - width_slow_alpha_,
                       static_cast<double>(kFastControlInterval));
        width_body_vitality_ += contextAlpha
                              * (bodyVitalityTarget - width_body_vitality_);
        width_projection_vitality_ += contextAlpha
                                    * (projectionVitalityTarget
                                       - width_projection_vitality_);

        const double bodyCorrGuard = (0.68 + 0.32 * corrGuard)
                                   * (1.0 - 0.96 * bodyAntiPhaseRisk);
        const double projectionCorrGuard = (0.70 + 0.30 * corrGuard)
                                         * (1.0 - 0.94
                                            * projectionAntiPhaseRisk);

        width_body_gain_cached_ = runtime.body_base
            * (0.88 + transientScore * 0.32 + bodyExistingStereo * 0.34)
            * (0.88 + width_body_vitality_ * 0.20)
            * bodyCorrGuard;
        width_mid_gain_cached_ = runtime.mid_base
            * (1.0 - leadProbability * 0.14)
            * (0.88 + projectionExistingStereo * 0.24)
            * (0.88 + width_projection_vitality_ * 0.20)
            * projectionCorrGuard;
        width_high_gain_cached_ = runtime.high_base
            * (1.0 - leadProbability * 0.10)
            * (0.55 + corrGuard * 0.45);
        width_air_gain_cached_ = runtime.air_base
            * (1.0 - antiPhase * 0.72)
            * (0.78 + panEvent * 0.20);
        const double generatedSideNeed = 0.52 * bodyNarrowNeed
                                       + 0.48 * projectionNarrowNeed;
        const double generatedSideHealthy = 0.52 * bodyExistingStereo
                                          + 0.48 * projectionExistingStereo;
        const double generatedSideRisk = std::max(
            {bodySideExcess, projectionSideExcess, antiPhase});
        adaptive_state_.stereo_risk = generatedSideRisk;
        const double generatedSideTarget = clamp(
            0.92 + 0.12 * generatedSideNeed
                 + 0.035 * generatedSideHealthy
                 - 0.34 * generatedSideRisk,
            0.55, 1.06);
        const double generatedSideAlpha = generatedSideTarget
                < adaptive_generated_side_permission_
            ? adaptive_content_reduce_alpha_
            : adaptive_content_restore_alpha_;
        adaptive_generated_side_permission_ += generatedSideAlpha
            * (generatedSideTarget - adaptive_generated_side_permission_);
        adaptive_state_.generated_side = adaptive_generated_side_permission_;

        width_gen_lowmid_gain_cached_ = runtime.gen_lowmid_base
            * adaptive_generated_side_permission_
            * scene_space_
            * (0.52 + 0.82 * bodyNarrowNeed
                    + 0.18 * bodyExistingStereo)
            * (1.0 - 0.72 * bodySideExcess)
            * (1.0 - panEvent * 0.34)
            * bodyCorrGuard;
        width_gen_mid_gain_cached_ = runtime.gen_mid_base
            * adaptive_generated_side_permission_
            * scene_space_
            * (0.50 + 0.78 * projectionNarrowNeed
                    + 0.16 * projectionExistingStereo)
            * (1.0 - leadProbability * 0.36)
            * (1.0 - 0.72 * projectionSideExcess)
            * (1.0 - panEvent * 0.42)
            * projectionCorrGuard;
        width_gen_air_gain_cached_ = runtime.gen_air_base
            * std::min(1.0, adaptive_generated_side_permission_)
            * scene_space_
            * (1.0 - leadProbability * 0.34)
            * (1.0 - antiPhase * 0.66);
        width_mid_anchor_gain_cached_ = 1.0
            + runtime.width_mix * leadProbability * 0.006
            + runtime.width_mix * runtime.source_protect * 0.0025;
    }
    if (++width_context_counter_ >= kFastControlInterval)
        width_context_counter_ = 0;

    double side_out = side;
    side_out -= static_cast<double>(side_sub) * width_low_reduce_cached_;
    side_out += static_cast<double>(side_body) * width_body_gain_cached_;
    side_out += static_cast<double>(side_mid) * width_mid_gain_cached_;
    side_out += static_cast<double>(side_high) * width_high_gain_cached_;
    side_out += static_cast<double>(side_air) * width_air_gain_cached_;
    side_out += static_cast<double>(lowmid_gen) * width_gen_lowmid_gain_cached_;
    side_out += static_cast<double>(mid_gen) * width_gen_mid_gain_cached_;
    side_out += static_cast<double>(air_gen) * width_gen_air_gain_cached_;

    // Center anchor is now tied to the projection-band lead detector, not to
    // full-band Mid dominance. Bass/kick therefore do not over-anchor the whole
    // mix and erase the perceived stereo enhancement.
    const double out_l = mid * width_mid_anchor_gain_cached_ + side_out;
    const double out_r = mid * width_mid_anchor_gain_cached_ - side_out;
    return {sane(static_cast<float>(out_l)), sane(static_cast<float>(out_r))};
}

void ArSonKuPikEngine::update_meters(double input_peak, double output_peak,
                                     double correlation, double gr_db,
                                     std::size_t frames)
{
    // Preserve the old per-sample smoothing response while doing expensive dB
    // conversions only once per block. This is metering-only and never feeds
    // the audio path.
    const double inputAlpha = 1.0 - std::pow(0.92, (double)frames);
    const double grAlpha = 1.0 - std::pow(0.88, (double)frames);
    const double corrAlpha = 1.0 - std::pow(0.995, (double)frames);
    const double previousInput = db_to_gain(meters_.input_peak_db);
    const double previousOutput = db_to_gain(meters_.output_peak_db);
    meters_.input_peak_db = gain_to_db(previousInput + inputAlpha * (input_peak - previousInput) + 1e-12);
    meters_.output_peak_db = gain_to_db(previousOutput + inputAlpha * (output_peak - previousOutput) + 1e-12);
    meters_.gain_reduction_db += grAlpha * (gr_db - meters_.gain_reduction_db);
    meters_.correlation += corrAlpha * (clamp(correlation, -1.0, 1.0) - meters_.correlation);
    meters_.clipping = output_peak >= 0.985 || gr_db > 8.0;
}

void ArSonKuPikEngine::sanitize_filter_states() noexcept
{
    for (auto& stage : eq_filters_)
        for (std::size_t channel = 0; channel < channels_; ++channel)
            stage[channel].sanitize_state();

    auto sanitizeStereo = [](auto& filters) {
        for (auto& filter : filters)
            filter.sanitize_state();
    };
    const auto& color = color_runtime_;
    if (color.bass_active) {
        sanitizeStereo(bass_post_hp_);
        sanitizeStereo(bass_post_lp_);
    }
    if (color.smart_warm_active) {
        sanitizeStereo(warmth_hp_);
        sanitizeStereo(warmth_lp_);
        low_body_hp_.sanitize_state(); low_body_lp_.sanitize_state();
        warm_box_hp_.sanitize_state(); warm_box_lp_.sanitize_state();
        warm_particle_dc_hp_.sanitize_state();
        warm_particle_post_lp_.sanitize_state();
        if (!std::isfinite(warm_particle_side_hp_x1_)
            || !std::isfinite(warm_particle_side_hp_y1_)) {
            warm_particle_side_hp_x1_ = 0.0f;
            warm_particle_side_hp_y1_ = 0.0f;
        }
        if (!std::isfinite(warm_lowmid_particle_hp_x1_)
            || !std::isfinite(warm_lowmid_particle_hp_y1_)
            || !std::isfinite(warm_lowmid_particle_side_hp_x1_)
            || !std::isfinite(warm_lowmid_particle_side_hp_y1_)) {
            warm_lowmid_particle_hp_x1_ = 0.0f;
            warm_lowmid_particle_hp_y1_ = 0.0f;
            warm_lowmid_particle_side_hp_x1_ = 0.0f;
            warm_lowmid_particle_side_hp_y1_ = 0.0f;
        }
    }
    if (color.upper_body_active) {
        sanitizeStereo(upper_body_hp_);
        sanitizeStereo(upper_body_lp_);
        vocal_body_detector_lp_.sanitize_state();
    }
    if (color.body_side_active) {
        low_body_side_hp_.sanitize_state();
        low_body_side_lp_.sanitize_state();
    }
    if (color.presence_active || color.tickle_active) {
        sanitizeStereo(presence_pre_);
        sanitizeStereo(presence_pre_lp_);
        sanitizeStereo(presence_post_hp_);
        sanitizeStereo(presence_post_lp_);
        if (color.tickle_active)
            vocal_tickle_lp_.sanitize_state();
    }
    if (color.air_active || color.skin_active) {
        sanitizeStereo(air_pre_);
        sanitizeStereo(air_pre_lp_);
        sanitizeStereo(air_post_hp_);
        sanitizeStereo(air_post_lp_);
        if (color.skin_active)
            treble_skin_bp_.sanitize_state();
    }
    if (color.active)
        sanitizeStereo(deharsh_);
    if (color.side_air_active) {
        side_air_hp_.sanitize_state(); side_air_tone_.sanitize_state();
    }
    if (color.mid_active) {
        mid_anchor_bp1_.sanitize_state(); mid_anchor_bp2_.sanitize_state();
    }

    if (width_runtime_.active && !width_runtime_.narrow) {
        for (auto* filter : { &width_side_sub_hp_, &width_side_sub_lp_,
                              &width_side_body_hp_, &width_side_body_lp_,
                              &width_side_mid_hp_, &width_side_mid_lp_,
                              &width_side_high_hp_, &width_side_high_lp_,
                              &width_side_air_hp_, &width_side_air_tone_ })
            filter->sanitize_state();
        if (width_runtime_.gen_lowmid_active) {
            width_lowmid_hp_.sanitize_state();
            width_lowmid_lp_.sanitize_state();
        }
        if (width_runtime_.gen_mid_active) {
            width_mid_hp_.sanitize_state();
            width_mid_lp_.sanitize_state();
        }
        if (width_runtime_.gen_air_active) {
            width_high_hp_.sanitize_state();
            width_air_tone_.sanitize_state();
        }
    }

    if (color.smart_warm_active) {
        for (double* envelope : { &warm_core_fast_env_, &warm_core_slow_env_,
                                  &warm_mud_env_, &warm_living_body_env_,
                                  &warm_upper_body_env_, &warm_input_env_,
                                  &warm_core_side_env_, &warm_lowmid_side_env_ }) {
            if (!std::isfinite(*envelope) || std::abs(*envelope) < 1.0e-18)
                *envelope = 1.0e-12;
        }
    }
    if (color.presence_active || color.air_active
        || color.tickle_active || color.skin_active) {
        for (double* envelope : { &presence_fast_env_, &presence_slow_env_,
                                  &presence_mid_env_, &presence_side_env_,
                                  &tickle_env_, &air_fast_env_, &air_slow_env_,
                                  &air_mid_env_, &air_side_env_, &skin_env_,
                                  &high_input_env_ }) {
            if (!std::isfinite(*envelope) || std::abs(*envelope) < 1.0e-18)
                *envelope = 1.0e-12;
        }
    }
    auto sanitizePermission = [](double& value, double fallback,
                                 double lo = 0.0, double hi = 1.0)
    {
        if (!std::isfinite(value))
            value = fallback;
        else
            value = clamp(value, lo, hi);
    };
    if (color.smart_warm_active) {
        sanitizePermission(warm_core_permission_, 0.75);
        sanitizePermission(warm_body_permission_, 0.75);
        sanitizePermission(warm_upper_permission_, 0.75);
        sanitizePermission(warm_particle_permission_, 1.0);
        sanitizePermission(warm_lowmid_particle_permission_, 0.75);
        sanitizePermission(vocal_body_companion_permission_, 0.0);
        sanitizePermission(adaptive_bass_permission_, 1.0, 0.92, 1.045);
        sanitizePermission(warm_lowmid_side_context_, 0.0);
    }
    if (color.presence_active || color.air_active
        || color.tickle_active || color.skin_active) {
        sanitizePermission(presence_permission_, 1.0);
        sanitizePermission(tickle_permission_, 1.0);
        sanitizePermission(air_permission_, 1.0);
        sanitizePermission(skin_permission_, 1.0);
        sanitizePermission(adaptive_presence_need_, 1.0, 0.82, 1.06);
        sanitizePermission(adaptive_tickle_need_, 1.0, 0.74, 1.08);
        sanitizePermission(adaptive_air_need_, 1.0, 0.74, 1.07);
        sanitizePermission(adaptive_skin_need_, 1.0, 0.66, 1.04);
        sanitizePermission(low_level_particle_support_, 1.0, 1.0, 1.34);
        sanitizePermission(microdetail_presence_articulation_, 1.0, 1.0, 1.063);
        sanitizePermission(microdetail_tickle_articulation_, 1.0, 1.0, 1.094);
        sanitizePermission(microdetail_air_articulation_, 1.0, 1.0, 1.029);
        sanitizePermission(microdetail_skin_articulation_, 1.0, 1.0, 1.036);
        sanitizePermission(presence_side_context_, 0.0);
        sanitizePermission(air_side_context_, 0.0);
    }
    sanitizePermission(adaptive_generated_side_permission_, 1.0, 0.55, 1.06);
    sanitizePermission(scene_detail_, scene_detail_target_, 0.82, 1.18);
    sanitizePermission(scene_articulation_, scene_articulation_target_, 0.80, 1.20);
    sanitizePermission(scene_density_, scene_density_target_, 0.82, 1.18);
    sanitizePermission(scene_body_, scene_body_target_, 0.84, 1.16);
    sanitizePermission(scene_space_, scene_space_target_, 0.82, 1.14);
}

void ArSonKuPikEngine::process(float** planes, std::size_t channels, std::size_t frames)
{
    if (!planes || channels == 0 || frames == 0) return;
    channels = std::min<std::size_t>(channels, channels_);
    if (channels == 0) return;

    const double input_gain = db_to_gain(working_.output.input_gain_db);
    const double creative_output_gain = db_to_gain(working_.output.output_gain_db);
    const double user_trim_gain = db_to_gain(params_.output_trim_db);
    const bool useInternalBypass = !params_.external_bypass;
    const double bypass_t = params_.bypass ? 1.0 : 0.0;

    // Use the same effective macro values that apply_macros() used. This keeps
    // the shared engine correct both for the VST wrapper (advanced override)
    // and for standalone/OBS callers that rely on factory preset macro values.
    const double runtime_enhance = params_.advanced_override ? params_.enhance : working_.macro.enhance;
    const double runtime_bass = params_.advanced_override ? params_.smart_bass : working_.macro.smart_bass;
    const double runtime_treble = params_.advanced_override ? params_.smart_treble : working_.macro.smart_treble;
    const double runtime_vocal = params_.advanced_override ? params_.vocal_body : working_.macro.vocal_body;
    const double runtime_stereo = params_.advanced_override ? params_.stereo_magic : working_.macro.stereo_magic;

    // Block-invariant control math belongs outside the sample loop. Keeping the
    // exact formulas while avoiding five pow() and several exp() calls per
    // sample materially lowers steady-state CPU without changing the recipe.
    const double enhance_risk = std::pow(clamp01(runtime_enhance / 100.0), 1.10);
    const double bass_risk = std::pow(clamp01((runtime_bass - 50.0) / 50.0), 1.25);
    const double treble_risk = std::pow(clamp01((runtime_treble - 50.0) / 50.0), 1.25);
    const double vocal_risk = std::pow(clamp01((runtime_vocal - 50.0) / 50.0), 1.20);
    const double stereo_risk = std::pow(clamp01((runtime_stereo - 50.0) / 50.0), 1.35);
    const double rack_risk = clamp01(0.46 * enhance_risk + 0.26 * bass_risk
                                   + 0.13 * treble_risk + 0.10 * vocal_risk
                                   + 0.05 * stereo_risk);
    const double rack_target_db = clamp(working_.policy.target_prechain_peak_db
                                      - 1.15 * rack_risk, -18.0, -10.0);
    const double rack_target_gain = db_to_gain(rack_target_db);
    const double minimum_headroom_gain = db_to_gain(-18.0);
    const double peak_release = 1.0 - std::exp(-1.0 / (sample_rate_ * 0.420));
    const double headroom_release_control = 1.0 - std::exp(
        -static_cast<double>(kFastControlInterval) / (sample_rate_ * 0.650));
    const double stage_control_alpha = 1.0 - std::exp(
        -static_cast<double>(kFastControlInterval) / (sample_rate_ * 0.260));
    const bool stableMusicalDynamics = preset_->policy.deterministic_level;
    const double fixedHeadroomDb = preset_->policy.fixed_headroom_db;
    const double fixedHeadroomGain = db_to_gain(fixedHeadroomDb);
    const double stableRestore = preset_->policy.fixed_restore_ratio;
    const double grBudget = std::max(0.25, working_.policy.max_limiter_gr_db);
    const double max_makeup_db = clamp(working_.policy.max_smart_makeup_db, 0.0, 10.0);
    double contextRestore = 0.58;
    switch (working_.policy.context) {
    case ProcessingContext::MixBus:    contextRestore = 0.84; break;
    case ProcessingContext::Track:     contextRestore = 0.82; break;
    case ProcessingContext::Master:    contextRestore = 0.72; break;
    case ProcessingContext::Signature: contextRestore = 0.60; break;
    case ProcessingContext::Creative:  contextRestore = 0.62; break;
    }
    const double restore_ratio = clamp(contextRestore - rack_risk * 0.10, 0.50, 0.86);

    // Deterministic presets have content-independent gains. Cache them once per
    // block rather than converting the same dB values for every sample.
    if (stableMusicalDynamics) {
        smart_headroom_db_ = fixedHeadroomDb;
        smart_headroom_gain_ = fixedHeadroomGain;
        smart_makeup_db_ = clamp((-fixedHeadroomDb) * stableRestore,
                                  0.0, max_makeup_db);
        smart_makeup_gain_ = db_to_gain(smart_makeup_db_);
    }

    double meterInputPeak = 0.0;
    double meterOutputPeak = 0.0;
    double meterDot = 0.0;
    double meterEnergyL = 0.0;
    double meterEnergyR = 0.0;
    double meterMaxGr = 0.0;

    // Mono sources are processed as dual-mono so color/limiter behavior remains stable.
    for (std::size_t i = 0; i < frames; ++i) {
        update_scene_controls();
        float original[kMaxChannels]{};
        for (std::size_t ch = 0; ch < channels; ++ch) original[ch] = planes[ch] ? sane(planes[ch][i]) : 0.0f;
        float l = original[0];
        float r = channels > 1 ? original[1] : original[0];
        const double in_l = l;
        const double in_r = r;

        // Smart pre-chain headroom guard (v0.5 context-aware).
        //
        // The previous detector noticed hot input in about 6 ms, but the actual
        // trim took ~180 ms to arrive. That allowed the first kick, snare, chorus
        // or track start to hit EQ -> compressor -> excitation -> width at almost
        // full scale. Once those nonlinear stages were overloaded, a limiter at
        // the end could only hide the peak; it could not restore clean tone.
        //
        // This guard attacks immediately and releases slowly. The rack target is
        // made slightly deeper for aggressive macro combinations, so every
        // preset receives enough clean working room before creative processing.
        const double pre_l = static_cast<double>(l) * input_gain;
        const double pre_r = static_cast<double>(r) * input_gain;
        const double input_peak_now = std::max(std::abs(pre_l), std::abs(pre_r));

        // v0.5 context policy: a Mix Bus or Track preset reserves more working
        // room and is not allowed to behave like a loud creative finalizer.
        // Signature/Creative presets remain expressive, but still receive extra
        // headroom as their macro risk rises.
        bool updateSmartGainControl = false;
        if (stableMusicalDynamics) {
            // Reserve the same headroom on every sample. This is intentionally
            // content-independent: a kick or sub note cannot pull the full mix
            // down and then release it upward. Final peak safety remains active.
            smart_input_peak_env_ = input_peak_now;
        } else {
            if (input_peak_now > smart_input_peak_env_)
                smart_input_peak_env_ = input_peak_now;
            else
                smart_input_peak_env_ += peak_release
                                       * (input_peak_now - smart_input_peak_env_);

            // Immediate linear-domain attack protects the creative rack without
            // a log10/pow round trip. Release and makeup are updated every 16
            // samples, well above control bandwidth but below audibility.
            if (input_peak_now * smart_headroom_gain_ > rack_target_gain) {
                smart_headroom_gain_ = clamp(
                    rack_target_gain / (input_peak_now + 1.0e-12),
                    minimum_headroom_gain, 1.0);
            }
            if (++smart_gain_control_counter_ >= kFastControlInterval) {
                smart_gain_control_counter_ = 0;
                updateSmartGainControl = true;
                const double target_headroom_gain = clamp(
                    rack_target_gain / (smart_input_peak_env_ + 1.0e-12),
                    minimum_headroom_gain, 1.0);
                if (target_headroom_gain < smart_headroom_gain_)
                    smart_headroom_gain_ = target_headroom_gain;
                else
                    smart_headroom_gain_ += headroom_release_control
                                          * (target_headroom_gain
                                             - smart_headroom_gain_);
                smart_headroom_db_ = gain_to_db(smart_headroom_gain_);
            }
        }

        l = sane(static_cast<float>(pre_l * smart_headroom_gain_));
        r = sane(static_cast<float>(pre_r * smart_headroom_gain_));

        l = process_channel_eq(0, l);
        r = process_channel_eq(1, r);

        auto comp = compressor_.process(l, r);
        l = comp.first; r = comp.second;

        auto col = process_color(l, r);
        l = col.first; r = col.second;

        auto wid = process_width(l, r);
        l = wid.first; r = wid.second;

        // Extension-style smart restore: the detector remains sample-accurate,
        // but expensive dB conversions and target calculation run at 1/16 rate.
        if (!stableMusicalDynamics) {
            const double prelim_peak_now = std::max(
                std::abs(static_cast<double>(l)),
                std::abs(static_cast<double>(r))) * creative_output_gain;
            if (prelim_peak_now > smart_prelim_peak_env_)
                smart_prelim_peak_env_ = prelim_peak_now;
            else
                smart_prelim_peak_env_ += peak_release
                                        * (prelim_peak_now - smart_prelim_peak_env_);

            if (updateSmartGainControl) {
                const double reserved_db = -smart_headroom_db_;
                const double prelim_db = gain_to_db(
                    smart_prelim_peak_env_ + 1.0e-12);
                const double peak_penalty = std::max(0.0, prelim_db + 5.0) * 0.95;
                const double limiter_penalty = std::max(
                    0.0, limiter_.gain_reduction_db() - grBudget * 0.55) * 0.92;
                const double target_makeup_db = clamp(
                    reserved_db * restore_ratio - peak_penalty - limiter_penalty,
                    0.0, max_makeup_db);
                smart_makeup_db_ += stage_control_alpha
                                  * (target_makeup_db - smart_makeup_db_);
                smart_makeup_gain_ = db_to_gain(smart_makeup_db_);
            }
        }
        const double total_output_gain = creative_output_gain
                                       * smart_makeup_gain_;

        l = sane(static_cast<float>(l * total_output_gain));
        r = sane(static_cast<float>(r * total_output_gain));

        // User Output Trim lives in the limiter/output stage, not inside the
        // creative macro engine. Negative trim lowers final level without making
        // Vocal/Enhance less active; positive trim is caught by the limiter.
        l = sane(static_cast<float>(l * user_trim_gain));
        r = sane(static_cast<float>(r * user_trim_gain));

        if (working_.output.limiter_enabled && !params_.external_peak_limiter) {
            auto lim = limiter_.process(l, r);
            l = lim.first; r = lim.second;
        }

        double bypass = 0.0;
        if (useInternalBypass) {
            bypass = bypass_smooth_.process(bypass_t);
            l = sane(static_cast<float>(l * (1.0 - bypass) + original[0] * bypass));
            r = sane(static_cast<float>(r * (1.0 - bypass)
                   + (channels > 1 ? original[1] : original[0]) * bypass));
        }

        if (planes[0]) planes[0][i] = l;
        if (channels > 1 && planes[1]) planes[1][i] = r;
        // Additional channels get the same core EQ/dynamics/output but no stereo magic, useful for 5.1/7.1 compatibility.
        for (std::size_t ch = 2; ch < channels; ++ch) {
            float x = sane(static_cast<float>(original[ch] * input_gain));
            x = process_channel_eq(ch, x);
            x = sane(static_cast<float>(x * creative_output_gain * user_trim_gain));
            if (useInternalBypass)
                x = sane(static_cast<float>(x * (1.0 - bypass) + original[ch] * bypass));
            if (planes[ch]) planes[ch][i] = x;
        }

        const double limiterGr = params_.external_peak_limiter
                               ? 0.0 : limiter_.gain_reduction_db();
        const double gr = std::max(compressor_.gain_reduction_db(), limiterGr);
        meterInputPeak = std::max(meterInputPeak, std::max(std::abs(in_l), std::abs(in_r)));
        meterOutputPeak = std::max(meterOutputPeak,
                                   std::max(std::abs((double)l), std::abs((double)r)));
        meterDot += in_l * in_r;
        meterEnergyL += in_l * in_l;
        meterEnergyR += in_r * in_r;
        meterMaxGr = std::max(meterMaxGr, gr);
    }

    sanitize_filter_states();

    const double meterDenominator = std::sqrt(
        std::max(1.0e-24, meterEnergyL * meterEnergyR));
    const double meterCorrelation = meterDenominator > 0.0
                                  ? meterDot / meterDenominator
                                  : 1.0;
    update_meters(meterInputPeak, meterOutputPeak, meterCorrelation,
                  meterMaxGr, frames);
}

} // namespace arsonkupik
