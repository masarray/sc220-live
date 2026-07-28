#include "PluginProcessor.h"
// Sonkupik Live headless port: VST editor intentionally omitted.

#include <algorithm>
#include <cmath>

namespace askp {

namespace ids {
static constexpr auto preset       = "preset";
static constexpr auto inputGain    = "input_gain_db";
static constexpr auto finalOutGain = "final_output_gain_db";
static constexpr auto enhance      = "enhance";
static constexpr auto smartBass    = "smart_bass";
static constexpr auto smartTreble  = "smart_treble";
static constexpr auto vocalBody    = "vocal_body";
static constexpr auto stereoMagic  = "stereo_magic";
static constexpr auto outputTrim   = "output_trim_db";
static constexpr auto outputTrimLock = "output_trim_lock";
static constexpr auto smartProtect = "smart_protect";
static constexpr auto bypass       = "bypass";
static constexpr auto gainMatch    = "gain_match";
} // namespace ids

const juce::StringArray& Processor::presetIds()
{
    static juce::StringArray list = [] {
        juce::StringArray s;
        for (const auto& p : arsonkupik::factory_preset_profiles())
            s.add(juce::String(p.id));
        return s;
    }();
    return list;
}

const juce::StringArray& Processor::presetNames()
{
    static juce::StringArray list = [] {
        juce::StringArray s;
        for (const auto& p : arsonkupik::factory_preset_profiles())
            s.add(juce::String(p.name));
        return s;
    }();
    return list;
}

juce::String Processor::presetDescription(int index)
{
    const auto& presets = arsonkupik::factory_preset_profiles();
    if (index < 0 || index >= (int)presets.size())
        return {};
    return juce::String(presets[(size_t)index].description);
}

juce::String Processor::presetCategory(int index)
{
    const auto& presets = arsonkupik::factory_preset_profiles();
    if (index < 0 || index >= (int)presets.size())
        return {};
    return juce::String(presets[(size_t)index].category);
}

juce::AudioProcessorValueTreeState::ParameterLayout Processor::createLayout()
{
    using P  = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;
    using PC = juce::AudioParameterChoice;

    const auto& defaults = arsonkupik::default_preset_profile().macro;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // v0.5.19 exposes curated scenes while retaining one immutable Signature DSP
    // topology. The version hint changes because the choice list expanded from
    // the P7A compatibility shim.
    layout.add(std::make_unique<PC>(juce::ParameterID{ids::preset, 5},
                                    "Factory Preset", presetNames(), 0));

    auto pct = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
    // Footer I/O faucets are symmetric manual trims: 0 dB sits exactly at
    // 12 o'clock; turn right to add, left to subtract. Version hint bumped to 2
    // because the normalised range changed (old sessions must not recall a
    // stale normalised position as the wrong dB value).
    layout.add(std::make_unique<P>(juce::ParameterID{ids::inputGain, 2},
                                   "Input Gain",
                                   juce::NormalisableRange<float>(-12.0f, 12.0f, 0.05f),
                                   0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::finalOutGain, 2},
                                   "Final Output Gain",
                                   juce::NormalisableRange<float>(-12.0f, 12.0f, 0.05f),
                                   0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::enhance, 1},
                                   "Enhance", pct, (float)defaults.enhance));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::smartBass, 1},
                                   "Smart Bass", pct, (float)defaults.smart_bass));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::smartTreble, 1},
                                   "Smart Treble", pct, (float)defaults.smart_treble));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::vocalBody, 1},
                                   "Vocal Body", pct, (float)defaults.vocal_body));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::stereoMagic, 1},
                                   "Stereo Magic", pct, (float)defaults.stereo_magic));
    layout.add(std::make_unique<P>(juce::ParameterID{ids::outputTrim, 1},
                                   "Output Trim",
                                   juce::NormalisableRange<float>(-6.0f, 6.0f, 0.05f),
                                   0.0f));
    layout.add(std::make_unique<PB>(juce::ParameterID{ids::outputTrimLock, 1},
                                    "Output Trim Lock", false));
    layout.add(std::make_unique<PB>(juce::ParameterID{ids::smartProtect, 1},
                                    "Smart Protect", true));
    layout.add(std::make_unique<PB>(juce::ParameterID{ids::bypass, 1},
                                    "Bypass", false));
    layout.add(std::make_unique<PB>(juce::ParameterID{ids::gainMatch, 1},
                                    "Gain Match", false));
    return layout;
}

Processor::Processor()
    : juce::AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ASKP_STATE", createLayout())
{
    for (auto* id : { ids::preset, ids::inputGain, ids::finalOutGain, ids::enhance, ids::smartBass, ids::smartTreble,
                      ids::vocalBody, ids::stereoMagic, ids::outputTrim,
                      ids::outputTrimLock, ids::smartProtect, ids::bypass,
                      ids::gainMatch })
        apvts.addParameterListener(id, this);
}

Processor::~Processor()
{
    for (auto* id : { ids::preset, ids::inputGain, ids::finalOutGain, ids::enhance, ids::smartBass, ids::smartTreble,
                      ids::vocalBody, ids::stereoMagic, ids::outputTrim,
                      ids::outputTrimLock, ids::smartProtect, ids::bypass,
                      ids::gainMatch })
        apvts.removeParameterListener(id, this);
}

void Processor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);
    const bool creative = parameterID == ids::preset
        || parameterID == ids::enhance
        || parameterID == ids::smartBass
        || parameterID == ids::smartTreble
        || parameterID == ids::vocalBody
        || parameterID == ids::stereoMagic
        || parameterID == ids::smartProtect;
    if (creative)
        creativeParamsDirty_.store(true, std::memory_order_release);
    else if (parameterID == ids::outputTrim)
        utilityParamsDirty_.store(true, std::memory_order_release);

    const bool presetContent = creative || parameterID == ids::outputTrim;
    if (presetContent
        && !presetRecallInProgress_.load(std::memory_order_acquire))
        presetModified_.store(true, std::memory_order_release);
}

void Processor::applyPresetMacroToParameters(int index)
{
    const auto& profiles = arsonkupik::factory_preset_profiles();
    if (profiles.empty())
        return;
    index = juce::jlimit(0, (int)profiles.size() - 1, index);
    const auto& p = profiles[(size_t)index];
    presetRecallInProgress_.store(true, std::memory_order_release);
    clearActiveUserPreset();

    auto setFloat = [this](const char* id, float value)
    {
        if (auto* param = apvts.getParameter(id)) {
            const float normalized = param->convertTo0to1(value);
            if (std::abs(param->getValue() - normalized) > 0.00001f) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(normalized);
                param->endChangeGesture();
            }
        }
    };

    auto setBool = [this](const char* id, bool value)
    {
        if (auto* param = apvts.getParameter(id)) {
            const float normalized = value ? 1.0f : 0.0f;
            if (std::abs(param->getValue() - normalized) > 0.00001f) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(normalized);
                param->endChangeGesture();
            }
        }
    };

    setFloat(ids::preset,      (float)index);
    setFloat(ids::enhance,     (float)p.macro.enhance);
    setFloat(ids::smartBass,   (float)p.macro.smart_bass);
    setFloat(ids::smartTreble, (float)p.macro.smart_treble);
    setFloat(ids::vocalBody,   (float)p.macro.vocal_body);
    setFloat(ids::stereoMagic, (float)p.macro.stereo_magic);

    // Output Trim follows the preset unless the user explicitly locks it.
    const bool trimLocked = apvts.getRawParameterValue(ids::outputTrimLock)->load() > 0.5f;
    if (!trimLocked)
        setFloat(ids::outputTrim, (float)p.output_trim_db);
    // Footer INPUT/OUTPUT utilities and automatic Gain Match are NEVER touched
    // by A/B switching.
    setBool(ids::smartProtect, p.macro.smart_protect);

    creativeParamsDirty_.store(true, std::memory_order_release);
    utilityParamsDirty_.store(true, std::memory_order_release);
    presetModified_.store(false, std::memory_order_release);
    presetRecallInProgress_.store(false, std::memory_order_release);
}

void Processor::selectFactoryPreset(int index)
{
    if (!licenseManager.editingAllowed())
        return;
    applyPresetMacroToParameters(index);
}

int Processor::currentFactoryPresetIndex() const noexcept
{
    const auto count = (int)arsonkupik::factory_preset_profiles().size();
    if (count <= 0)
        return 0;
    return juce::jlimit(0, count - 1,
        (int)std::lround(apvts.getRawParameterValue(ids::preset)->load()));
}

void Processor::clearActiveUserPreset()
{
    const juce::ScopedLock lock(presetStateLock_);
    activeUserPresetId_.clear();
    activeUserPresetName_.clear();
    activeUserPresetCategory_.clear();
    activeUserPresetDescription_.clear();
    activeUserPresetFile_ = {};
}

bool Processor::currentPresetIsUser() const
{
    const juce::ScopedLock lock(presetStateLock_);
    return activeUserPresetId_.isNotEmpty();
}

juce::String Processor::currentPresetName() const
{
    {
        const juce::ScopedLock lock(presetStateLock_);
        if (activeUserPresetName_.isNotEmpty())
            return activeUserPresetName_;
    }
    return presetNames()[currentFactoryPresetIndex()];
}

juce::String Processor::currentPresetCategory() const
{
    {
        const juce::ScopedLock lock(presetStateLock_);
        if (activeUserPresetId_.isNotEmpty())
            return activeUserPresetCategory_.isNotEmpty()
                ? activeUserPresetCategory_ : juce::String("User");
    }
    return presetCategory(currentFactoryPresetIndex());
}

juce::String Processor::currentPresetDescription() const
{
    {
        const juce::ScopedLock lock(presetStateLock_);
        if (activeUserPresetId_.isNotEmpty())
            return activeUserPresetDescription_;
    }
    return presetDescription(currentFactoryPresetIndex());
}

juce::File Processor::userPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("MasArray")
        .getChildFile("ArSonKuPik")
        .getChildFile("Presets");
}

std::vector<Processor::UserPresetInfo> Processor::userPresets() const
{
    std::vector<UserPresetInfo> result;
    const auto directory = userPresetDirectory();
    if (!directory.isDirectory())
        return result;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.askpreset");
    result.reserve((size_t)files.size());

    for (const auto& file : files) {
        if (file.getSize() <= 0 || file.getSize() > 128 * 1024)
            continue;

        juce::var parsed;
        if (juce::JSON::parse(file.loadFileAsString(), parsed).failed())
            continue;
        auto* object = parsed.getDynamicObject();
        if (object == nullptr
            || object->getProperty("engine").toString() != "mas-ari-signature")
            continue;

        UserPresetInfo info;
        info.id = object->getProperty("id").toString();
        info.name = object->getProperty("name").toString();
        info.category = object->getProperty("category").toString();
        info.description = object->getProperty("description").toString();
        if (info.id.isNotEmpty() && info.name.isNotEmpty())
            result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(),
        [](const UserPresetInfo& a, const UserPresetInfo& b)
        {
            return a.name.compareNatural(b.name) < 0;
        });
    return result;
}

juce::Result Processor::writeUserPreset(const juce::String& id,
                                        const juce::String& name,
                                        const juce::File& target)
{
    auto* root = new juce::DynamicObject();
    juce::var document(root);
    root->setProperty("schema", 1);
    root->setProperty("engine", "mas-ari-signature");
    root->setProperty("engineRevision", ASKP_DSP_HEADER_REVISION);
    root->setProperty("id", id);
    root->setProperty("name", name);
    root->setProperty("category", "User");
    root->setProperty("description", "Saved ArSonKuPik settings");
    root->setProperty("baseProfileId",
        presetIds()[currentFactoryPresetIndex()]);

    auto* values = new juce::DynamicObject();
    root->setProperty("values", juce::var(values));
    auto saveFloat = [this, values](const char* idToSave)
    {
        values->setProperty(idToSave,
            (double)apvts.getRawParameterValue(idToSave)->load());
    };
    for (auto* idToSave : { ids::enhance, ids::smartBass, ids::smartTreble,
                            ids::vocalBody, ids::stereoMagic, ids::outputTrim })
        saveFloat(idToSave);
    values->setProperty(ids::smartProtect,
        apvts.getRawParameterValue(ids::smartProtect)->load() > 0.5f);

    const auto directory = target.getParentDirectory();
    const auto directoryResult = directory.createDirectory();
    if (directoryResult.failed())
        return juce::Result::fail(
            "Could not create the user preset folder: "
            + directoryResult.getErrorMessage());

    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(
            juce::JSON::toString(document, true), false, false, "\n"))
        return juce::Result::fail("Could not write the temporary preset file.");
    if (!temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not atomically replace the preset file.");
    return juce::Result::ok();
}

juce::Result Processor::saveUserPresetAs(const juce::String& requestedName)
{
    if (!licenseManager.editingAllowed())
        return juce::Result::fail("Unlock ArSonKuPik before saving presets.");

    const auto name = requestedName.trim().substring(0, 64);
    if (name.isEmpty())
        return juce::Result::fail("Enter a preset name.");

    const auto id = juce::Uuid().toString().removeCharacters("-");
    const auto target = userPresetDirectory().getChildFile(id + ".askpreset");
    const auto writeResult = writeUserPreset(id, name, target);
    if (writeResult.failed())
        return writeResult;

    {
        const juce::ScopedLock lock(presetStateLock_);
        activeUserPresetId_ = id;
        activeUserPresetName_ = name;
        activeUserPresetCategory_ = "User";
        activeUserPresetDescription_ = "Saved ArSonKuPik settings";
        activeUserPresetFile_ = target;
    }
    presetModified_.store(false, std::memory_order_release);
    return juce::Result::ok();
}

juce::Result Processor::saveCurrentUserPreset()
{
    juce::String id;
    juce::String name;
    juce::File target;
    {
        const juce::ScopedLock lock(presetStateLock_);
        id = activeUserPresetId_;
        name = activeUserPresetName_;
        target = activeUserPresetFile_;
    }
    if (id.isEmpty() || target == juce::File())
        return juce::Result::fail("Choose Save As for a new user preset.");

    const auto result = writeUserPreset(id, name, target);
    if (result.wasOk())
        presetModified_.store(false, std::memory_order_release);
    return result;
}

juce::Result Processor::loadUserPreset(const juce::String& requestedId)
{
    if (!licenseManager.editingAllowed())
        return juce::Result::fail("Unlock ArSonKuPik before loading presets.");

    const auto directory = userPresetDirectory();
    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.askpreset");

    juce::File source;
    juce::var parsed;
    for (const auto& file : files) {
        if (file.getSize() <= 0 || file.getSize() > 128 * 1024)
            continue;
        juce::var candidate;
        if (juce::JSON::parse(file.loadFileAsString(), candidate).failed())
            continue;
        if (auto* object = candidate.getDynamicObject();
            object != nullptr
            && object->getProperty("id").toString() == requestedId) {
            source = file;
            parsed = std::move(candidate);
            break;
        }
    }
    auto* object = parsed.getDynamicObject();
    if (source == juce::File() || object == nullptr)
        return juce::Result::fail("The user preset could not be found.");
    if (object->getProperty("engine").toString() != "mas-ari-signature")
        return juce::Result::fail("This preset targets a different engine.");

    const auto baseId = object->getProperty("baseProfileId").toString();
    int baseIndex = presetIds().indexOf(baseId);
    if (baseIndex < 0)
        baseIndex = 0;
    applyPresetMacroToParameters(baseIndex);

    auto* values = object->getProperty("values").getDynamicObject();
    if (values == nullptr)
        return juce::Result::fail("The preset has no parameter values.");

    presetRecallInProgress_.store(true, std::memory_order_release);
    auto setFloat = [this, values](const char* parameterId)
    {
        if (!values->hasProperty(parameterId))
            return;
        if (auto* parameter = apvts.getParameter(parameterId)) {
            const float plain = (float)(double)values->getProperty(parameterId);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
        }
    };
    for (auto* parameterId : { ids::enhance, ids::smartBass, ids::smartTreble,
                               ids::vocalBody, ids::stereoMagic })
        setFloat(parameterId);
    if (apvts.getRawParameterValue(ids::outputTrimLock)->load() <= 0.5f)
        setFloat(ids::outputTrim);
    if (values->hasProperty(ids::smartProtect))
        if (auto* parameter = apvts.getParameter(ids::smartProtect))
            parameter->setValueNotifyingHost(
                (bool)values->getProperty(ids::smartProtect) ? 1.0f : 0.0f);

    {
        const juce::ScopedLock lock(presetStateLock_);
        activeUserPresetId_ = object->getProperty("id").toString();
        activeUserPresetName_ = object->getProperty("name").toString();
        activeUserPresetCategory_ = object->getProperty("category").toString();
        activeUserPresetDescription_ = object->getProperty("description").toString();
        activeUserPresetFile_ = source;
    }
    creativeParamsDirty_.store(true, std::memory_order_release);
    utilityParamsDirty_.store(true, std::memory_order_release);
    presetModified_.store(false, std::memory_order_release);
    presetRecallInProgress_.store(false, std::memory_order_release);
    return juce::Result::ok();
}

juce::Result Processor::deleteCurrentUserPreset()
{
    juce::File target;
    {
        const juce::ScopedLock lock(presetStateLock_);
        target = activeUserPresetFile_;
    }
    if (target == juce::File())
        return juce::Result::fail("No user preset is selected.");
    if (target.existsAsFile() && !target.deleteFile())
        return juce::Result::fail("Could not delete the user preset.");

    clearActiveUserPreset();
    presetModified_.store(false, std::memory_order_release);
    return juce::Result::ok();
}

void Processor::buildRuntimeParams(arsonkupik::RuntimeParams& out,
                                   float enhance, float bass, float treble,
                                   float vocal, float stereo, float trim,
                                   bool smartProtect) const
{
    const auto& profiles = arsonkupik::factory_preset_profiles();
    const int profileIndex = profiles.empty() ? 0 : juce::jlimit(
        0, (int)profiles.size() - 1, currentFactoryPresetIndex());
    const auto& profile = profiles.empty()
        ? arsonkupik::default_preset_profile()
        : profiles[(size_t)profileIndex];

    // The engine topology remains the sole Signature entry at index zero. The
    // selected profile contributes only bounded scene scalars and macro values.
    out.preset_index = 0;
    out.preset_id = "default";
    out.enhance        = enhance;
    out.smart_bass     = bass;
    out.smart_treble   = treble;
    out.vocal_body     = vocal;
    out.stereo_magic   = stereo;
    out.output_trim_db = trim;
    out.detail_focus = profile.detail_focus;
    out.articulation_focus = profile.articulation_focus;
    out.density_tolerance = profile.density_tolerance;
    out.body_focus = profile.body_focus;
    out.space_focus = profile.space_focus;
    out.smart_protect  = smartProtect;
    // Wrapper-level internal bypass owns the reference-path crossfade. Keeping
    // the shared engine wet avoids a second nested bypass ramp after Input/WOW/Output.
    out.bypass         = false;
    out.external_bypass = true;
    out.external_peak_limiter = true;
    out.advanced_override = true;
}

void Processor::resetSignatureWow() noexcept
{
    wowLockMeasureSamples_ = 0;
    wowMeasurePrimed_ = false;
    wowDryPowerEnv_ = 0.0f;
    wowWetPowerEnv_ = 0.0f;
    wowWetPeakEnv_ = 0.0f;
    wowLockedDb_ = 0.0f;
    wowCompDb_ = 0.0f;
    wowCompPrimed_ = false;
    wowGainLocked_ = false;
    wowHeadroomGuard_.reset(0.0f);
    // Signature uses deterministic reserve/restore and should begin its one-time
    // loudness calibration immediately after prepare or a full engine reset.
    wowFreezeSamples_ = 0;
}

void Processor::resetGainMatchMeasurement(bool keepTarget) noexcept
{
    gainMatchRawLowState_.fill(0.0f);
    gainMatchRawBodyState_.fill(0.0f);
    gainMatchWetLowState_.fill(0.0f);
    gainMatchWetBodyState_.fill(0.0f);
    gainMatchRawBlockPowerSum_.fill(0.0);
    gainMatchRawTotalBlockPowerSum_ = 0.0;
    gainMatchRawPowerEnv_.fill(0.0f);
    gainMatchWetPowerEnv_.fill(0.0f);
    gainMatchRawTotalPowerEnv_ = 0.0f;
    gainMatchWetTotalPowerEnv_ = 0.0f;
    gainMatchRawLowBlockPeak_ = 0.0f;
    gainMatchWetLowBlockPeak_ = 0.0f;
    gainMatchRawLowTransientEnv_ = 0.0f;
    gainMatchWetLowTransientEnv_ = 0.0f;
    gainMatchSignalSamples_ = 0;
    gainMatchMeasurePrimed_ = false;
    if (!keepTarget) {
        gainMatchTargetBandDb_.fill(0.0f);
        gainMatchTargetDb_ = 0.0f;
        gainMatchReady_ = false;
        gainMatchTargetIsSeed_ = false;
    }
}

void Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = (std::size_t)juce::jmax(1, getTotalNumOutputChannels());
    const auto preparedChannels = juce::jmin<std::size_t>(channels, arsonkupik::kMaxChannels);
    engine_.prepare(sampleRate, preparedChannels);

    // Allocate once outside the callback. P7A needs one warm-up scratch block
    // plus the latency-aligned dry reference; the removed standby engine and
    // preset-crossfade buffer no longer consume per-instance memory.
    realtimeScratchSamples_ = juce::jmax(kRealtimeScratchFloorSamples, samplesPerBlock);
    warmupScratch_.setSize((int)preparedChannels, realtimeScratchSamples_, false, true, true);
    bypassDry_.setSize((int)preparedChannels, realtimeScratchSamples_, false, true, true);

    // One millisecond of fixed lookahead remains identical to P6.1. Both wet
    // and Reference Bypass paths carry the same reported latency and ceiling.
    lookaheadSamples_ = juce::jmax(1, (int)std::ceil(sampleRate * kLookaheadSeconds));
    lookaheadBufferSamples_ = lookaheadSamples_ + 1;
    wetLookaheadDelay_.setSize((int)preparedChannels, lookaheadBufferSamples_,
                               false, true, true);
    dryLatencyDelay_.setSize((int)preparedChannels, lookaheadBufferSamples_,
                             false, true, true);
    wetLookaheadDelay_.clear();
    dryLatencyDelay_.clear();
    wetDelayWrite_ = 0;
    dryDelayWrite_ = 0;
    lookaheadGainHistory_.assign((size_t)lookaheadSamples_, 1.0f);
    dryReferenceGainHistory_.assign((size_t)lookaheadSamples_, 1.0f);
    lookaheadReleaseAlphaFast_ = 1.0f - std::exp(
        -1.0f / ((float)juce::jmax(1.0, sampleRate) * arsonkupik::WowHeadroomGuard::kShallowLimiterReleaseSeconds));
    lookaheadReleaseAlphaSlow_ = 1.0f - std::exp(
        -1.0f / ((float)juce::jmax(1.0, sampleRate) * arsonkupik::WowHeadroomGuard::kDeepLimiterReleaseSeconds));
    resetLookaheadGain();
    resetDryReferenceGain();
    setLatencySamples(lookaheadSamples_);

    const double ramp = 0.08; // 80 ms knob glide
    auto init = [&](Smooth& smoother, const char* id) {
        smoother.reset(sampleRate, ramp);
        smoother.setCurrentAndTargetValue(apvts.getRawParameterValue(id)->load());
    };
    init(smInput_,    ids::inputGain);
    init(smFinalOut_, ids::finalOutGain);
    init(smEnhance_,  ids::enhance);
    init(smBass_,     ids::smartBass);
    init(smTreble_,   ids::smartTreble);
    init(smVocal_,    ids::vocalBody);
    init(smStereo_,   ids::stereoMagic);
    init(smTrim_,     ids::outputTrim);
    smBypass_.reset(sampleRate, 0.012);
    lastBypassTarget_ = apvts.getRawParameterValue(ids::bypass)->load() > 0.5f;
    smBypass_.setCurrentAndTargetValue(lastBypassTarget_ ? 1.0f : 0.0f);

    resetSignatureWow();
    safetyGrDb_ = 0.0f;
    inputAppliedGain_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue(ids::inputGain)->load());
    wowAppliedGain_ = 1.0f;
    finalOutAppliedGain_ = juce::Decibels::decibelsToGain(
        apvts.getRawParameterValue(ids::finalOutGain)->load());

    gainMatchLowAlpha_ = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * 160.0f
        / (float)juce::jmax(1.0, sampleRate));
    gainMatchBodyAlpha_ = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * 800.0f
        / (float)juce::jmax(1.0, sampleRate));
    gainMatchTargetBandDb_.fill(0.0f);
    gainMatchAppliedBandDb_.fill(0.0f);
    gainMatchTargetDb_ = 0.0f;
    gainMatchAppliedDb_ = 0.0f;
    gainMatchTargetIsSeed_ = false;
    gainMatchSlowEngagement_ = false;
    gainMatchBypassSettledSamples_ = 0;
    gainMatchTargetStale_ = false;
    lastGainMatchEnabled_ = apvts.getRawParameterValue(ids::gainMatch)->load() > 0.5f;
    gainMatchReady_ = false;
    resetGainMatchMeasurement(false);
    meters.gainMatchDb.store(0.0f, std::memory_order_relaxed);
    meters.gainMatchReady.store(false, std::memory_order_relaxed);

    activeSmartProtect_ = apvts.getRawParameterValue(ids::smartProtect)->load() > 0.5f;
    arsonkupik::RuntimeParams initialParams;
    buildRuntimeParams(initialParams,
                       apvts.getRawParameterValue(ids::enhance)->load(),
                       apvts.getRawParameterValue(ids::smartBass)->load(),
                       apvts.getRawParameterValue(ids::smartTreble)->load(),
                       apvts.getRawParameterValue(ids::vocalBody)->load(),
                       apvts.getRawParameterValue(ids::stereoMagic)->load(),
                       apvts.getRawParameterValue(ids::outputTrim)->load(),
                       activeSmartProtect_);
    engine_.set_runtime_params(initialParams);
    engine_.reset();

    // Cold-start crackle guard: exercise the one Signature engine and wrapper
    // safety path outside realtime, then reset all histories. The steady-state
    // P6.1 Signature samples remain unchanged.
    const int warmBlock = juce::jlimit(32, 256, realtimeScratchSamples_);
    const int warmSamples = juce::jmax(
        warmBlock, (int)std::round(sampleRate * 0.060));
    std::array<float*, arsonkupik::kMaxChannels> warmPlanes{};
    for (std::size_t ch = 0; ch < preparedChannels; ++ch)
        warmPlanes[ch] = warmupScratch_.getWritePointer((int)ch);

    int remaining = warmSamples;
    while (remaining > 0) {
        const int count = juce::jmin(warmBlock, remaining);
        for (std::size_t ch = 0; ch < preparedChannels; ++ch)
            warmupScratch_.clear((int)ch, 0, count);
        engine_.process(warmPlanes.data(), preparedChannels, (std::size_t)count);
        remaining -= count;
    }
    engine_.reset();

    for (std::size_t ch = 0; ch < preparedChannels; ++ch)
        warmupScratch_.clear((int)ch, 0, warmBlock);
    float warmPeakL = 0.0f, warmPeakR = 0.0f;
    std::array<double, kGainMatchBandCount> warmPower{};
    double warmTotalPower = 0.0;
    const std::array<float, kGainMatchBandCount> unityMatch{1.0f, 1.0f, 1.0f};
    processLookaheadSafety(warmupScratch_, preparedChannels, warmBlock,
                           1.0f, 1.0f, 1.0f, 1.0f,
                           unityMatch, unityMatch,
                           warmPower, warmTotalPower, warmPeakL, warmPeakR);

    wetLookaheadDelay_.clear();
    dryLatencyDelay_.clear();
    wetDelayWrite_ = 0;
    dryDelayWrite_ = 0;
    resetLookaheadGain();
    resetDryReferenceGain();
    gainMatchRawLowState_.fill(0.0f);
    gainMatchRawBodyState_.fill(0.0f);
    gainMatchWetLowState_.fill(0.0f);
    gainMatchWetBodyState_.fill(0.0f);
    gainMatchRawBlockPowerSum_.fill(0.0);
    gainMatchRawTotalBlockPowerSum_ = 0.0;
    safetyGrDb_ = 0.0f;

    startupFadeSamplesTotal_ = juce::jmax(1,
        (int)std::round(sampleRate * 0.060));
    startupFadeSamplesRemaining_ = startupFadeSamplesTotal_;

    creativeRetuneIntervalSamples_ = juce::jmax(1,
        (int)std::round(sampleRate / kCreativeRetuneHz));
    creativeRetuneCountdownSamples_ = creativeRetuneIntervalSamples_;
    lastEngineEnhance_ = static_cast<float>(initialParams.enhance);
    lastEngineBass_ = static_cast<float>(initialParams.smart_bass);
    lastEngineTreble_ = static_cast<float>(initialParams.smart_treble);
    lastEngineVocal_ = static_cast<float>(initialParams.vocal_body);
    lastEngineStereo_ = static_cast<float>(initialParams.stereo_magic);
    lastEngineTrim_ = static_cast<float>(initialParams.output_trim_db);
    lastEngineSmartProtect_ = initialParams.smart_protect;
    creativeParamsDirty_.store(false, std::memory_order_release);
    utilityParamsDirty_.store(false, std::memory_order_release);
}

void Processor::advanceDryDelay(juce::AudioBuffer<float>& buffer,
                                std::size_t channels, int numSamples,
                                bool storeScratch, bool writeOutput)
{
    std::array<float*, arsonkupik::kMaxChannels> audio{};
    std::array<float*, arsonkupik::kMaxChannels> delay{};
    std::array<float*, arsonkupik::kMaxChannels> scratch{};
    for (std::size_t ch = 0; ch < channels; ++ch) {
        audio[ch] = buffer.getWritePointer((int)ch);
        delay[ch] = dryLatencyDelay_.getWritePointer((int)ch);
        if (storeScratch)
            scratch[ch] = bypassDry_.getWritePointer((int)ch);
    }

    // P5.1 Reference Bypass -------------------------------------------------
    // The plugin's own BYPASS control is an honest listening reference rather
    // than a technical raw bypass: no creative/utility gain is applied, but a
    // continuously running lookahead ceiling matches the active -1 dBFS safety
    // contract. Samples at or below the ceiling remain exactly unity-gain.
    // Host/DAW bypass is not handled here and therefore remains raw.
    const float ceiling = juce::Decibels::decibelsToGain(kOutputCeilingDb);
    dryReferenceInputPeakL_ = 0.0f;
    dryReferenceInputPeakR_ = 0.0f;
    dryReferenceOutputPeakL_ = 0.0f;
    dryReferenceOutputPeakR_ = 0.0f;
    float maxGr = 0.0f;
    gainMatchRawBlockPowerSum_.fill(0.0);
    gainMatchRawTotalBlockPowerSum_ = 0.0;
    gainMatchRawLowBlockPeak_ = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        float inputPeak = 0.0f;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            const float current = audio[ch][i];
            inputPeak = juce::jmax(inputPeak, std::abs(current));
            if (ch == 0)
                dryReferenceInputPeakL_ = juce::jmax(dryReferenceInputPeakL_, std::abs(current));
            else if (ch == 1)
                dryReferenceInputPeakR_ = juce::jmax(dryReferenceInputPeakR_, std::abs(current));
        }

        const float requested = inputPeak > ceiling
                              ? ceiling / (inputPeak + 1.0e-9f) : 1.0f;
        if (requested < dryReferenceDetectorGain_) {
            dryReferenceDetectorGain_ = requested;
            dryReferenceDetectorHold_ = lookaheadSamples_;
        } else if (dryReferenceDetectorHold_ > 0) {
            --dryReferenceDetectorHold_;
        } else {
            const float releaseAlpha =
                dryReferenceDetectorGain_ >= arsonkupik::WowHeadroomGuard::kShallowLimiterGainThreshold
                    ? lookaheadReleaseAlphaFast_
                    : lookaheadReleaseAlphaSlow_;
            dryReferenceDetectorGain_ += releaseAlpha
                                       * (1.0f - dryReferenceDetectorGain_);
        }

        const float previousGain =
            dryReferenceGainHistory_[(size_t)dryReferenceGainWrite_];
        dryReferenceGainHistory_[(size_t)dryReferenceGainWrite_] =
            dryReferenceDetectorGain_;
        if (++dryReferenceGainWrite_ == lookaheadSamples_)
            dryReferenceGainWrite_ = 0;
        dryReferenceGainSum_ += (double)dryReferenceDetectorGain_ - previousGain;
        dryReferenceGain_ = (float)(dryReferenceGainSum_
                                  / (double)lookaheadSamples_);

        int readIndex = dryDelayWrite_ + 1;
        if (readIndex == lookaheadBufferSamples_)
            readIndex = 0;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            const float current = audio[ch][i];
            const float delayed = delay[ch][readIndex];
            delay[ch][dryDelayWrite_] = current;
            const float reference = delayed * dryReferenceGain_;

            // P6 keeps the complementary split for exact reconstruction and
            // transition compatibility, but learns loudness from the actual
            // full-band sample. The split no longer determines tonal correction.
            gainMatchRawTotalBlockPowerSum_ += (double)reference * reference;
            if (ch < gainMatchRawLowState_.size()) {
                auto& lowState = gainMatchRawLowState_[ch];
                auto& bodyState = gainMatchRawBodyState_[ch];
                lowState += gainMatchLowAlpha_ * (reference - lowState);
                bodyState += gainMatchBodyAlpha_ * (reference - bodyState);
                const float lowBand = lowState;
                gainMatchRawLowBlockPeak_ = juce::jmax(
                    gainMatchRawLowBlockPeak_, std::abs(lowBand));
                const float bodyBand = bodyState - lowState;
                const float upperBand = reference - bodyState;
                gainMatchRawBlockPowerSum_[kGainMatchLow] +=
                    (double)lowBand * lowBand;
                gainMatchRawBlockPowerSum_[kGainMatchBody] +=
                    (double)bodyBand * bodyBand;
                gainMatchRawBlockPowerSum_[kGainMatchUpper] +=
                    (double)upperBand * upperBand;
            }
            if (storeScratch)
                scratch[ch][i] = reference;
            if (writeOutput)
                audio[ch][i] = reference;

            if (ch == 0)
                dryReferenceOutputPeakL_ = juce::jmax(dryReferenceOutputPeakL_, std::abs(reference));
            else if (ch == 1)
                dryReferenceOutputPeakR_ = juce::jmax(dryReferenceOutputPeakR_, std::abs(reference));
        }
        dryDelayWrite_ = readIndex;

        if (dryReferenceGain_ < 1.0f)
            maxGr = juce::jmax(maxGr,
                -juce::Decibels::gainToDecibels(dryReferenceGain_, -120.0f));
    }

    if (channels == 1) {
        dryReferenceInputPeakR_ = dryReferenceInputPeakL_;
        dryReferenceOutputPeakR_ = dryReferenceOutputPeakL_;
    }
    dryReferenceGrDb_ = 0.88f * dryReferenceGrDb_ + 0.12f * maxGr;
}

void Processor::primeWetDelayFromDry() noexcept
{
    const int channels = juce::jmin(wetLookaheadDelay_.getNumChannels(),
                                    dryLatencyDelay_.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
        wetLookaheadDelay_.copyFrom(ch, 0, dryLatencyDelay_, ch, 0,
                                    lookaheadBufferSamples_);
    wetDelayWrite_ = dryDelayWrite_;
}

void Processor::resetLookaheadGain() noexcept
{
    std::fill(lookaheadGainHistory_.begin(), lookaheadGainHistory_.end(), 1.0f);
    lookaheadGainWrite_ = 0;
    lookaheadGainSum_ = (double)lookaheadGainHistory_.size();
    lookaheadDetectorGain_ = 1.0f;
    lookaheadGain_ = 1.0f;
    lookaheadDetectorHold_ = 0;
    safetyGrDb_ = 0.0f;
}

void Processor::resetDryReferenceGain() noexcept
{
    std::fill(dryReferenceGainHistory_.begin(),
              dryReferenceGainHistory_.end(), 1.0f);
    dryReferenceGainWrite_ = 0;
    dryReferenceGainSum_ = (double)dryReferenceGainHistory_.size();
    dryReferenceDetectorGain_ = 1.0f;
    dryReferenceGain_ = 1.0f;
    dryReferenceDetectorHold_ = 0;
    dryReferenceGrDb_ = 0.0f;
    dryReferenceInputPeakL_ = 0.0f;
    dryReferenceInputPeakR_ = 0.0f;
    dryReferenceOutputPeakL_ = 0.0f;
    dryReferenceOutputPeakR_ = 0.0f;
}

void Processor::processLookaheadSafety(juce::AudioBuffer<float>& buffer,
                                       std::size_t channels,
                                       int numSamples,
                                       float wowGainStart,
                                       float wowGainEnd,
                                       float finalGainStart,
                                       float finalGainEnd,
                                       const std::array<float, 3>& matchGainStart,
                                       const std::array<float, 3>& matchGainEnd,
                                       std::array<double, 3>& preMatchPowerSum,
                                       double& preMatchTotalPowerSum,
                                       float& outputPeakL,
                                       float& outputPeakR) noexcept
{
    std::array<float*, arsonkupik::kMaxChannels> audio{};
    std::array<float*, arsonkupik::kMaxChannels> delay{};
    for (std::size_t ch = 0; ch < channels; ++ch) {
        audio[ch] = buffer.getWritePointer((int)ch);
        delay[ch] = wetLookaheadDelay_.getWritePointer((int)ch);
    }

    outputPeakL = 0.0f;
    outputPeakR = 0.0f;
    const float ceiling = juce::Decibels::decibelsToGain(kOutputCeilingDb);
    const float denominator = (float)juce::jmax(1, numSamples);
    const bool fastWowReduction = wowGainEnd < wowGainStart;
    const int wowReductionSamples = juce::jmax(
        1, juce::jmin(numSamples, lookaheadSamples_));
    const float wowStep = (wowGainEnd - wowGainStart) / denominator;
    const float finalStep = (finalGainEnd - finalGainStart) / denominator;
    std::array<float, kGainMatchBandCount> matchGain = matchGainStart;
    std::array<float, kGainMatchBandCount> matchStep{};
    for (std::size_t band = 0; band < kGainMatchBandCount; ++band)
        matchStep[band] = (matchGainEnd[band] - matchGainStart[band]) / denominator;
    float wowGain = wowGainStart;
    float finalGain = finalGainStart;
    float maxGr = 0.0f;
    preMatchPowerSum.fill(0.0);
    preMatchTotalPowerSum = 0.0;
    gainMatchWetLowBlockPeak_ = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        if (fastWowReduction) {
            const float t = juce::jmin(
                1.0f, (float)(i + 1) / (float)wowReductionSamples);
            wowGain = wowGainStart + t * (wowGainEnd - wowGainStart);
        }
        const float combinedGain = wowGain * finalGain;

        float peak = 0.0f;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            const float gained = audio[ch][i] * combinedGain;
            audio[ch][i] = gained;
            peak = juce::jmax(peak, std::abs(gained));
        }

        const float requested = peak > ceiling
                              ? ceiling / (peak + 1.0e-9f) : 1.0f;
        if (requested < lookaheadDetectorGain_) {
            lookaheadDetectorGain_ = requested;
            lookaheadDetectorHold_ = lookaheadSamples_;
        } else if (lookaheadDetectorHold_ > 0) {
            --lookaheadDetectorHold_;
        } else {
            const float releaseAlpha =
                lookaheadDetectorGain_ >= arsonkupik::WowHeadroomGuard::kShallowLimiterGainThreshold
                    ? lookaheadReleaseAlphaFast_
                    : lookaheadReleaseAlphaSlow_;
            lookaheadDetectorGain_ += releaseAlpha
                                    * (1.0f - lookaheadDetectorGain_);
        }

        const float previousGain =
            lookaheadGainHistory_[(size_t)lookaheadGainWrite_];
        lookaheadGainHistory_[(size_t)lookaheadGainWrite_] =
            lookaheadDetectorGain_;
        if (++lookaheadGainWrite_ == lookaheadSamples_)
            lookaheadGainWrite_ = 0;
        lookaheadGainSum_ += (double)lookaheadDetectorGain_ - previousGain;
        lookaheadGain_ = (float)(lookaheadGainSum_
                               / (double)lookaheadSamples_);

        int readIndex = wetDelayWrite_ + 1;
        if (readIndex == lookaheadBufferSamples_)
            readIndex = 0;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            const float current = audio[ch][i];
            const float delayed = delay[ch][readIndex];
            delay[ch][wetDelayWrite_] = current;
            const float preMatch = delayed * lookaheadGain_;
            preMatchTotalPowerSum += (double)preMatch * preMatch;

            // P6 applies one identical attenuation value to all complementary
            // bands. Their sum is therefore exactly a scalar version of preMatch:
            // no spectral tilt, phase impression or particle masking is added by
            // Gain Match. The split remains only for smooth legacy handovers.
            auto& lowState = gainMatchWetLowState_[ch];
            auto& bodyState = gainMatchWetBodyState_[ch];
            lowState += gainMatchLowAlpha_ * (preMatch - lowState);
            bodyState += gainMatchBodyAlpha_ * (preMatch - bodyState);
            const float lowBand = lowState;
            gainMatchWetLowBlockPeak_ = juce::jmax(
                gainMatchWetLowBlockPeak_, std::abs(lowBand));
            const float bodyBand = bodyState - lowState;
            const float upperBand = preMatch - bodyState;
            preMatchPowerSum[kGainMatchLow] += (double)lowBand * lowBand;
            preMatchPowerSum[kGainMatchBody] += (double)bodyBand * bodyBand;
            preMatchPowerSum[kGainMatchUpper] += (double)upperBand * upperBand;

            const float output = lowBand * matchGain[kGainMatchLow]
                               + bodyBand * matchGain[kGainMatchBody]
                               + upperBand * matchGain[kGainMatchUpper];
            audio[ch][i] = output;
            if (ch == 0)
                outputPeakL = juce::jmax(outputPeakL, std::abs(output));
            else if (ch == 1)
                outputPeakR = juce::jmax(outputPeakR, std::abs(output));
        }
        wetDelayWrite_ = readIndex;
        if (!fastWowReduction)
            wowGain += wowStep;
        else if (i + 1 >= wowReductionSamples)
            wowGain = wowGainEnd;
        finalGain += finalStep;
        for (std::size_t band = 0; band < kGainMatchBandCount; ++band)
            matchGain[band] += matchStep[band];

        if (lookaheadGain_ < 1.0f)
            maxGr = juce::jmax(maxGr,
                -juce::Decibels::gainToDecibels(lookaheadGain_, -120.0f));
    }

    if (channels == 1)
        outputPeakR = outputPeakL;
    safetyGrDb_ = 0.88f * safetyGrDb_ + 0.12f * maxGr;
}

void Processor::updateGainMatchController(
    const std::array<double, 3>& preMatchPowerSum,
    double preMatchTotalPowerSum,
    std::size_t channels,
    int numSamples,
    bool allowLearning) noexcept
{
    if (!allowLearning || channels == 0 || numSamples <= 0)
        return;

    const double count = (double)channels * (double)numSamples;
    std::array<float, kGainMatchBandCount> rawPower{};
    std::array<float, kGainMatchBandCount> wetPower{};
    for (std::size_t band = 0; band < kGainMatchBandCount; ++band) {
        rawPower[band] = (float)(gainMatchRawBlockPowerSum_[band] / count);
        wetPower[band] = (float)(preMatchPowerSum[band] / count);
    }
    const float rawTotalPower = (float)(gainMatchRawTotalBlockPowerSum_ / count);
    const float wetTotalPower = (float)(preMatchTotalPowerSum / count);

    const float signalFloor =
        juce::Decibels::decibelsToGain(kGainMatchSignalFloorDb);
    if (std::sqrt(juce::jmax(rawTotalPower, 0.0f)) < signalFloor
        || std::sqrt(juce::jmax(wetTotalPower, 0.0f)) < signalFloor * 0.25f)
        return; // hold the last trustworthy estimate through silence

    const float envelopeAlpha = 1.0f - std::exp(
        -(float)numSamples / (0.85f * (float)getSampleRate()));
    const float transientReleaseAlpha = 1.0f - std::exp(
        -(float)numSamples
        / (kGainMatchLowTransientReleaseSeconds * (float)getSampleRate()));
    const bool firstMeasurement = !gainMatchMeasurePrimed_;
    if (firstMeasurement) {
        gainMatchRawPowerEnv_ = rawPower;
        gainMatchWetPowerEnv_ = wetPower;
        gainMatchRawTotalPowerEnv_ = rawTotalPower;
        gainMatchWetTotalPowerEnv_ = wetTotalPower;
        gainMatchRawLowTransientEnv_ = gainMatchRawLowBlockPeak_;
        gainMatchWetLowTransientEnv_ = gainMatchWetLowBlockPeak_;
        gainMatchMeasurePrimed_ = true;
    } else {
        for (std::size_t band = 0; band < kGainMatchBandCount; ++band) {
            gainMatchRawPowerEnv_[band] += envelopeAlpha
                * (rawPower[band] - gainMatchRawPowerEnv_[band]);
            gainMatchWetPowerEnv_[band] += envelopeAlpha
                * (wetPower[band] - gainMatchWetPowerEnv_[band]);
        }
        gainMatchRawTotalPowerEnv_ += envelopeAlpha
            * (rawTotalPower - gainMatchRawTotalPowerEnv_);
        gainMatchWetTotalPowerEnv_ += envelopeAlpha
            * (wetTotalPower - gainMatchWetTotalPowerEnv_);

        const auto followLowPeak = [transientReleaseAlpha](
            float envelope, float blockPeak) noexcept {
            return blockPeak >= envelope
                ? blockPeak
                : envelope + transientReleaseAlpha * (blockPeak - envelope);
        };
        gainMatchRawLowTransientEnv_ = followLowPeak(
            gainMatchRawLowTransientEnv_, gainMatchRawLowBlockPeak_);
        gainMatchWetLowTransientEnv_ = followLowPeak(
            gainMatchWetLowTransientEnv_, gainMatchWetLowBlockPeak_);
    }

    gainMatchSignalSamples_ = juce::jmin(
        (int)std::round(4.0 * getSampleRate()),
        gainMatchSignalSamples_ + numSamples);
    if (gainMatchSignalSamples_ < (int)std::round(3.0 * getSampleRate()))
        return;

    const float rawRms = std::sqrt(
        juce::jmax(gainMatchRawTotalPowerEnv_, 1.0e-12f));
    const float wetRms = std::sqrt(
        juce::jmax(gainMatchWetTotalPowerEnv_, 1.0e-12f));
    if (rawRms < signalFloor || wetRms < signalFloor * 0.25f)
        return;

    // P6.1 loudness-only correction. The active DSP may retain +0.70 dB so its
    // enhancement identity is not erased by the comparison controller. A low
    // transient guard may relax that same full-band attenuation by at most
    // 0.30 dB; it never applies a low-only gain and therefore cannot alter tone
    // or phase. Gain Match remains attenuation-only and never exceeds unity.
    const float loudnessScalarDb = juce::jlimit(
        kGainMatchMaxAttenuationDb, 0.0f,
        juce::Decibels::gainToDecibels(rawRms / wetRms, -120.0f)
            + kGainMatchEnhancementAllowanceDb);

    float transientReliefDb = 0.0f;
    if (gainMatchRawLowTransientEnv_ > signalFloor * 0.25f
        && gainMatchWetLowTransientEnv_ > signalFloor * 0.125f) {
        const float lowTransientFloorDb =
            juce::Decibels::gainToDecibels(
                gainMatchRawLowTransientEnv_
                    / gainMatchWetLowTransientEnv_,
                -120.0f)
            - kGainMatchLowTransientMaxDeficitDb;
        transientReliefDb = juce::jlimit(
            0.0f, kGainMatchLowTransientMaxReliefDb,
            lowTransientFloorDb - loudnessScalarDb);
    }

    const float scalarDb = juce::jlimit(
        kGainMatchMaxAttenuationDb, 0.0f,
        loudnessScalarDb + transientReliefDb);
    const GainMatchBands wanted{ scalarDb, scalarDb, scalarDb };

    if (!gainMatchReady_) {
        gainMatchTargetBandDb_ = wanted;
        gainMatchReady_ = true;
        gainMatchSlowEngagement_ = true; // first learned target: 500 ms handover
        gainMatchTargetIsSeed_ = false;
    } else {
        // A cached/policy seed or a stale post-bypass target stays audible while
        // fresh evidence is collected. The first trustworthy replacement then
        // glides slowly, avoiding a sudden monitoring-level jump.
        if (gainMatchTargetIsSeed_ || gainMatchTargetStale_) {
            gainMatchTargetBandDb_ = wanted;
            gainMatchTargetIsSeed_ = false;
            gainMatchTargetStale_ = false;
            gainMatchSlowEngagement_ = true;
        } else {
            const float current = gainMatchTargetBandDb_[kGainMatchBody];
            if (std::abs(scalarDb - current) > 0.08f) {
                const float tau = scalarDb < current ? 0.45f
                    : transientReliefDb > 0.02f ? 0.25f : 1.40f;
                const float alpha = 1.0f - std::exp(
                    -(float)numSamples / (tau * (float)getSampleRate()));
                const float next = current + alpha * (scalarDb - current);
                gainMatchTargetBandDb_.fill(next);
            } else {
                // Repair any stale unequal cache/transition value defensively.
                gainMatchTargetBandDb_.fill(current);
            }
        }
    }

    gainMatchTargetDb_ = gainMatchTargetBandDb_[kGainMatchBody];
    meters.gainMatchReady.store(gainMatchReady_, std::memory_order_relaxed);
}

bool Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    auto dbFromPeak = [](float peak)
    {
        return juce::Decibels::gainToDecibels(juce::jmax(peak, 1.0e-6f), -120.0f);
    };

    auto measurePeaks = [&](float& peakL, float& peakR, double* powerSum)
    {
        peakL = 0.0f;
        peakR = 0.0f;
        if (powerSum != nullptr)
            *powerSum = 0.0;
        if (numCh > 0) {
            const auto* l = buffer.getReadPointer(0);
            for (int i = 0; i < numSamples; ++i) {
                peakL = juce::jmax(peakL, std::abs(l[i]));
                if (powerSum != nullptr)
                    *powerSum += (double)l[i] * l[i];
            }
            if (numCh > 1) {
                const auto* r = buffer.getReadPointer(1);
                for (int i = 0; i < numSamples; ++i) {
                    peakR = juce::jmax(peakR, std::abs(r[i]));
                    if (powerSum != nullptr)
                        *powerSum += (double)r[i] * r[i];
                }
            } else {
                peakR = peakL;
            }
        }
    };

    auto publishReferenceBypassMeters = [&]
    {
        const float* l = buffer.getNumChannels() > 0 ? buffer.getReadPointer(0) : nullptr;
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;
        if (l != nullptr)
            scopeTap.push(l, r, numSamples);

        const float inputDbL = dbFromPeak(dryReferenceInputPeakL_);
        const float inputDbR = dbFromPeak(dryReferenceInputPeakR_);
        const float outputDbL = dbFromPeak(dryReferenceOutputPeakL_);
        const float outputDbR = dbFromPeak(dryReferenceOutputPeakR_);
        meters.inputPeakDbL.store(inputDbL, std::memory_order_relaxed);
        meters.inputPeakDbR.store(inputDbR, std::memory_order_relaxed);
        meters.outputPeakDbL.store(outputDbL, std::memory_order_relaxed);
        meters.outputPeakDbR.store(outputDbR, std::memory_order_relaxed);
        meters.inputPeakDb.store(juce::jmax(inputDbL, inputDbR), std::memory_order_relaxed);
        meters.outputPeakDb.store(juce::jmax(outputDbL, outputDbR), std::memory_order_relaxed);
        meters.gainReductionDb.store(dryReferenceGrDb_, std::memory_order_relaxed);
        meters.gainMatchDb.store(gainMatchReady_ ? gainMatchTargetDb_ : 0.0f,
                                 std::memory_order_relaxed);
        meters.gainMatchReady.store(gainMatchReady_, std::memory_order_relaxed);
        meters.correlation.store(1.0f, std::memory_order_relaxed);
        meters.clipping.store(dryReferenceInputPeakL_ >= 1.0f
                           || dryReferenceInputPeakR_ >= 1.0f,
                              std::memory_order_relaxed);
    };

    const auto channels = (std::size_t)juce::jmin(buffer.getNumChannels(),
                                                  (int)arsonkupik::kMaxChannels);
    const bool scratchFits = numSamples <= realtimeScratchSamples_;
    const bool bypassTarget = apvts.getRawParameterValue(ids::bypass)->load() > 0.5f;
    const bool wasSettledDry = !smBypass_.isSmoothing()
                            && smBypass_.getCurrentValue() >= 0.9999f;
    if (bypassTarget != lastBypassTarget_) {
        // The engine is intentionally suspended after the fade reaches the reference path.
        // Reset its histories before the return-to-wet ramp so stale envelopes
        // can never reappear as a click after a long bypass interval.
        if (!bypassTarget && wasSettledDry) {
            engine_.reset();
            if (gainMatchTargetStale_) {
                // Keep the last audible correction on re-enable, relearn in the
                // background, then glide to fresh values only when trustworthy.
                resetGainMatchMeasurement(true);
                gainMatchSlowEngagement_ = false;
            }
            gainMatchBypassSettledSamples_ = 0;
            // Seed the wet lookahead line from the continuously maintained dry
            // history. The first millisecond after re-enable is therefore
            // signal-continuous while the reset engine fills the line.
            primeWetDelayFromDry();
            resetLookaheadGain();
            wowMeasurePrimed_ = false;
            wowWetPeakEnv_ = 0.0f;
        }
        smBypass_.setTargetValue(bypassTarget ? 1.0f : 0.0f);
        lastBypassTarget_ = bypassTarget;
    }

    const bool bypassMixNeeded = smBypass_.isSmoothing()
                              || smBypass_.getCurrentValue() > 0.0001f;
    const bool startupMixNeeded = startupFadeSamplesRemaining_ > 0
                               && !bypassTarget;
    if ((bypassMixNeeded || startupMixNeeded) && !scratchFits) {
        // A host exceeded the maximum block size supplied to prepareToPlay.
        // Preserve the no-allocation guarantee; large offline blocks settle to
        // the requested side instead of growing scratch storage in the callback.
        smBypass_.setCurrentAndTargetValue(bypassTarget ? 1.0f : 0.0f);
        startupFadeSamplesRemaining_ = 0;
    }

    const bool settledDry = bypassTarget && !smBypass_.isSmoothing()
                         && smBypass_.getCurrentValue() >= 0.9999f;
    const bool hasBypassDry = (bypassMixNeeded || startupMixNeeded)
                           && scratchFits && !settledDry;

    // The dry delay runs continuously, even while active, so bypass remains
    // phase-aligned with the latency-compensated wet path at every toggle.
    // P5.1 also keeps its independent -1 dBFS reference limiter warm.
    advanceDryDelay(buffer, channels, numSamples, hasBypassDry, settledDry);

    // Settled internal bypass runs only the latency line, reference peak
    // containment, metering, and optional visual tap. No creative DSP runs.
    if (settledDry) {
        gainMatchBypassSettledSamples_ = juce::jmin(
            (int)std::round(4.0 * getSampleRate()),
            gainMatchBypassSettledSamples_ + numSamples);
        if (gainMatchBypassSettledSamples_ >= (int)std::round(1.5 * getSampleRate()))
            gainMatchTargetStale_ = true;
        publishReferenceBypassMeters();
        return;
    }

    // Input trim in the footer works pre-engine, like a true input gain stage.
    smInput_.setTargetValue(apvts.getRawParameterValue(ids::inputGain)->load());
    const float vInput = smInput_.skip(numSamples);
    const float inputLin = juce::Decibels::decibelsToGain(vInput);
    if (std::abs(inputLin - inputAppliedGain_) > 0.00001f)
        buffer.applyGainRamp(0, numSamples, inputAppliedGain_, inputLin);
    else if (std::abs(inputLin - 1.0f) > 0.00001f)
        buffer.applyGain(inputLin);
    inputAppliedGain_ = inputLin;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    double dryPowerSum = 0.0;
    measurePeaks(inPeakL, inPeakR, &dryPowerSum);

    // ---- zipper-free continuous Signature controls -------------------------
    smEnhance_.setTargetValue(apvts.getRawParameterValue(ids::enhance)->load());
    smBass_.setTargetValue(apvts.getRawParameterValue(ids::smartBass)->load());
    smTreble_.setTargetValue(apvts.getRawParameterValue(ids::smartTreble)->load());
    smVocal_.setTargetValue(apvts.getRawParameterValue(ids::vocalBody)->load());
    smStereo_.setTargetValue(apvts.getRawParameterValue(ids::stereoMagic)->load());
    smTrim_.setTargetValue(apvts.getRawParameterValue(ids::outputTrim)->load());
    smFinalOut_.setTargetValue(apvts.getRawParameterValue(ids::finalOutGain)->load());
    activeSmartProtect_ = apvts.getRawParameterValue(ids::smartProtect)->load() > 0.5f;

    const float vEnhance = smEnhance_.skip(numSamples);
    const float vBass    = smBass_.skip(numSamples);
    const float vTreble  = smTreble_.skip(numSamples);
    const float vVocal   = smVocal_.skip(numSamples);
    const float vStereo  = smStereo_.skip(numSamples);
    const float vTrim    = smTrim_.skip(numSamples);

    // Creative coefficient retuning is capped at 200 Hz. The 80 ms parameter
    // smoothers still advance every block; only the comparatively expensive
    // EQ/color/width coefficient rebuild is rate-limited. Utility Output Trim
    // follows every block through a scalar-only engine update.
    creativeRetuneCountdownSamples_ -= numSamples;
    const bool explicitCreativeDirty =
        creativeParamsDirty_.exchange(false, std::memory_order_acq_rel);
    const bool creativeValueChanged = explicitCreativeDirty
        || std::abs(vEnhance - lastEngineEnhance_) > 1.0e-4f
        || std::abs(vBass - lastEngineBass_) > 1.0e-4f
        || std::abs(vTreble - lastEngineTreble_) > 1.0e-4f
        || std::abs(vVocal - lastEngineVocal_) > 1.0e-4f
        || std::abs(vStereo - lastEngineStereo_) > 1.0e-4f
        || activeSmartProtect_ != lastEngineSmartProtect_;

    if (creativeValueChanged && creativeRetuneCountdownSamples_ <= 0) {
        arsonkupik::RuntimeParams rp;
        buildRuntimeParams(rp, vEnhance, vBass, vTreble, vVocal, vStereo, vTrim,
                           activeSmartProtect_);
        engine_.set_runtime_params(rp);
        lastEngineEnhance_ = vEnhance;
        lastEngineBass_ = vBass;
        lastEngineTreble_ = vTreble;
        lastEngineVocal_ = vVocal;
        lastEngineStereo_ = vStereo;
        lastEngineTrim_ = vTrim;
        lastEngineSmartProtect_ = activeSmartProtect_;
        utilityParamsDirty_.store(false, std::memory_order_release);
        creativeRetuneCountdownSamples_ = creativeRetuneIntervalSamples_;
    } else {
        const bool explicitUtilityDirty =
            utilityParamsDirty_.exchange(false, std::memory_order_acq_rel);
        if (explicitUtilityDirty || std::abs(vTrim - lastEngineTrim_) > 1.0e-4f) {
            engine_.set_output_trim_db(vTrim);
            lastEngineTrim_ = vTrim;
        }
    }

    std::array<float*, arsonkupik::kMaxChannels> planes{};
    for (std::size_t ch = 0; ch < channels; ++ch)
        planes[ch] = buffer.getWritePointer((int)ch);
    engine_.process(planes.data(), channels, (std::size_t)numSamples);

    float targetWowGain = wowAppliedGain_;

    // ---- P7G Floor-only Signature level management -------------------------
    // The tone engine remains the approved P6/P7A Signature, but final level is
    // no longer forced to +4 dB and then reduced by Gain Match. Gain Match OFF
    // also must not flatten programme to an exact loudness target. One clean
    // scalar therefore protects only a +0.90 dB minimum Active advantage. It
    // never attenuates creative DSP lift merely for being audible; only the
    // independent headroom/peak-safety path may reduce unsafe output.
    //
    // v0.4.0 regression audit: the servo once sat after the engine limiter and
    // could add excessive gain without protection. The retained fixes are:
    //   1. Headroom-aware clamp: the comp gain may never push the slow wet
    //      peak envelope past kSafeCeilingDb. On hot masters the lift settles
    //      below +4 dB instead of cracking — smart headroom, not blind gain.
    //   2. Measurement freeze support during startup/reset settling.
    //   3. A dedicated safety stage (peak limiter + tanh soft-clip) is the
    //      absolute last thing before audio leaves the plugin.
    {
        double dryAcc = 0.0, wetAcc = 0.0;
        float  wetPeak = 0.0f;
        const auto chs = juce::jmin(2, buffer.getNumChannels());
        for (int ch = 0; ch < chs; ++ch) {
            const auto* w = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                wetAcc += (double)w[i] * w[i];
                wetPeak = juce::jmax(wetPeak, std::abs(w[i]));
            }
        }
        dryAcc = dryPowerSum;
        const double n = (double)juce::jmax(1, numSamples * chs);
        const float blockDryPower = (float)(dryAcc / n);
        const float blockWetPower = (float)(wetAcc / n);

        // Slow wet-peak envelope: instant attack, ~400 ms release. This is the
        // headroom sensor for the comp clamp.
        const float relAlpha = 1.0f - std::exp(-(float)numSamples
                                               / (0.40f * (float)getSampleRate()));
        wowWetPeakEnv_ = wetPeak > wowWetPeakEnv_
                       ? wetPeak
                       : wowWetPeakEnv_ + relAlpha * (wetPeak - wowWetPeakEnv_);

        if (wowFreezeSamples_ > 0)
            wowFreezeSamples_ = juce::jmax(0, wowFreezeSamples_ - numSamples);

        // Compare long-term powers instead of one block against one block.
        // Per-block ratios were dominated by kick/snare/phase changes and made
        // the servo hold stale attenuation or pump after the new pre-chain guard
        // reacted. About 850 ms is slow enough to follow song sections without
        // following individual musical events.
        if (wowFreezeSamples_ == 0) {
            if (!wowMeasurePrimed_) {
                wowDryPowerEnv_ = blockDryPower;
                wowWetPowerEnv_ = blockWetPower;
                wowMeasurePrimed_ = true;
            } else {
                const auto& calibrationPreset = arsonkupik::default_preset();
                const bool fastLockedCalibration = calibrationPreset.policy.deterministic_level;
                // Signature calibrates once and then holds. A shorter
                // averaging window reaches the intended loudness before the lock,
                // while all continuously tracking contexts retain the slow servo.
                const float powerTau = fastLockedCalibration ? 0.18f : 0.85f;
                const float powerAlpha = 1.0f - std::exp(-(float)numSamples
                    / (powerTau * (float)getSampleRate()));
                wowDryPowerEnv_ += powerAlpha * (blockDryPower - wowDryPowerEnv_);
                wowWetPowerEnv_ += powerAlpha * (blockWetPower - wowWetPowerEnv_);
            }
        }

        const float dryRms = std::sqrt(juce::jmax(wowDryPowerEnv_, 0.0f));
        const float wetRms = std::sqrt(juce::jmax(wowWetPowerEnv_, 0.0f));
        const bool signalOk = dryRms > 1.0e-4f && wetRms > 1.0e-5f;
        const auto& activePreset = arsonkupik::default_preset();
        const bool stableMusicalWow = activePreset.policy.deterministic_level;

        if (signalOk && wowFreezeSamples_ == 0) {
            const float liftDb = juce::Decibels::gainToDecibels(
                wetRms / dryRms, -60.0f);
            const float minimumActiveLiftDb =
                (float)activePreset.policy.target_wow_lift_db;
            const float wantedDb = liftDb < minimumActiveLiftDb
                ? minimumActiveLiftDb - liftDb
                : 0.0f;

            const float protectedPeak = juce::jmax(wetPeak, wowWetPeakEnv_);
            const float wetPeakDb = juce::Decibels::gainToDecibels(
                juce::jmax(protectedPeak, 1.0e-5f), -80.0f);
            const float allowedDb = kSafeCeilingDb - wetPeakDb
                + arsonkupik::WowHeadroomGuard::kTransientLimiterBudgetDb;
            const float positiveLiftPermissionDb =
                (float)activePreset.policy.max_locked_wow_db;
            const float desiredDb = juce::jlimit(
                -12.0f,
                juce::jmin(positiveLiftPermissionDb, allowedDb),
                wantedDb);

            if (!wowCompPrimed_) {
                // The continuously prepared Signature engine has already settled and the
                // final sample-peak safety remains active. Start every incoming
                // context from its measured correction instead of forcing 0 dB
                // and creating a quiet-then-loud base-volume rise.
                wowCompDb_ = desiredDb;
                wowCompPrimed_ = true;
            } else if (!stableMusicalWow || !wowGainLocked_) {
                const float tau = desiredDb < wowCompDb_
                    ? (stableMusicalWow ? 0.070f : 0.06f)
                    : (stableMusicalWow ? 0.160f : 0.30f);
                const float alpha = 1.0f - std::exp(-(float)numSamples
                                                    / (tau * (float)getSampleRate()));
                wowCompDb_ += alpha * (desiredDb - wowCompDb_);
            }

            if (stableMusicalWow && !wowGainLocked_) {
                wowLockMeasureSamples_ += numSamples;
                // Lock only after the measurement window has averaged the
                // opening material. The floor-only correction and bounded
                // positive permission prevent a transient-biased estimate from
                // becoming either a loudness jump or an always-on matcher.
                if (wowLockMeasureSamples_ >= (int)std::round(0.70 * getSampleRate())) {
                    const float maxLockDb = (float)activePreset.policy.max_locked_wow_db;
                    wowLockedDb_ = juce::jlimit(-6.0f, maxLockDb, wowCompDb_);
                    wowCompDb_ = wowLockedDb_;
                    wowGainLocked_ = true;
                    wowHeadroomGuard_.reset(wowLockedDb_);
                }
            }

        }

        // Once measured, Signature holds one floor-only scalar. Safe creative
        // lift receives 0 dB correction; the final lookahead path remains
        // available for unsafe peaks without following later song sections.
        const float currentPeakDb = juce::Decibels::gainToDecibels(
                                        juce::jmax(wetPeak, 1.0e-5f), -80.0f);
        const float hardAllowedDb = kSafeCeilingDb - currentPeakDb;
        float appliedWowDb = 0.0f;
        if (stableMusicalWow && wowGainLocked_) {
            // P7B: isolated peaks may use at most the short natural transient
            // budget; sustained density reduces program gain before the limiter.
            // This protects kick/snare shape, stereo enhancement and mid-low body
            // while retaining the hard -1 dBFS ceiling.
            appliedWowDb = wowHeadroomGuard_.processLocked(
                wowLockedDb_, wetPeak, getSampleRate(), numSamples);
        } else {
            wowHeadroomGuard_.reset(wowCompDb_);
            appliedWowDb = juce::jmin(wowCompDb_, hardAllowedDb);
        }
        targetWowGain = juce::Decibels::decibelsToGain(appliedWowDb);
    }

    // Final Output Gain is outside the creative chain and outside the limiter/trim
    // macro path. It is the last output faucet before audio leaves the plugin,    // so it can tame a hot processed signal even when Output Trim is already low.
    const float vFinalOut = smFinalOut_.skip(numSamples);
    const float finalOutLin = juce::Decibels::decibelsToGain(vFinalOut);

    // Gain Match remains an optional monitoring comparator. P7G leaves every
    // safe creative lift audible; only this explicitly enabled stage targets
    // strict loudness equality. It never defines the Signature sound.
    const bool gainMatchEnabled =
        apvts.getRawParameterValue(ids::gainMatch)->load() > 0.5f;
    if (gainMatchEnabled != lastGainMatchEnabled_) {
        if (gainMatchEnabled && gainMatchReady_)
            gainMatchSlowEngagement_ = true;
        lastGainMatchEnabled_ = gainMatchEnabled;
    }
    const GainMatchBands requestedBandDb = gainMatchTargetBandDb_;

    float maxMatchDelta = 0.0f;
    for (std::size_t band = 0; band < kGainMatchBandCount; ++band) {
        const float requestedDb = gainMatchEnabled && gainMatchReady_
                                ? requestedBandDb[band] : 0.0f;
        maxMatchDelta = juce::jmax(maxMatchDelta,
            std::abs(requestedDb - gainMatchAppliedBandDb_[band]));
    }
    const float matchGlideSeconds = gainMatchSlowEngagement_ ? 0.500f
        : maxMatchDelta > 1.0f ? 0.180f : 0.080f;
    const float matchGlideAlpha = 1.0f - std::exp(
        -(float)numSamples / (matchGlideSeconds * (float)getSampleRate()));
    std::array<float, kGainMatchBandCount> matchGainStart{};
    std::array<float, kGainMatchBandCount> matchGainEnd{};
    for (std::size_t band = 0; band < kGainMatchBandCount; ++band) {
        const float requestedDb = gainMatchEnabled && gainMatchReady_
                                ? requestedBandDb[band] : 0.0f;
        const float nextDb = gainMatchAppliedBandDb_[band]
                           + matchGlideAlpha
                           * (requestedDb - gainMatchAppliedBandDb_[band]);
        matchGainStart[band] = juce::Decibels::decibelsToGain(
            gainMatchAppliedBandDb_[band]);
        matchGainEnd[band] = juce::Decibels::decibelsToGain(nextDb);
        gainMatchAppliedBandDb_[band] = nextDb;
    }
    if (gainMatchSlowEngagement_ && maxMatchDelta < 0.05f)
        gainMatchSlowEngagement_ = false;
    if (!gainMatchEnabled)
        gainMatchSlowEngagement_ = false;
    gainMatchAppliedDb_ = gainMatchAppliedBandDb_[kGainMatchBody];

    // ---- Latency-compensated lookahead peak safety -------------------------
    // WOW, Final Output and loudness-only monitoring correction are applied in
    // the same final loop, preserving bounded CPU and zero callback allocation.
    float outPeakL = 0.0f, outPeakR = 0.0f;
    std::array<double, kGainMatchBandCount> preMatchPowerSum{};
    double preMatchTotalPowerSum = 0.0;
    processLookaheadSafety(buffer, channels, numSamples,
                           wowAppliedGain_, targetWowGain,
                           finalOutAppliedGain_, finalOutLin,
                           matchGainStart, matchGainEnd,
                           preMatchPowerSum, preMatchTotalPowerSum,
                           outPeakL, outPeakR);
    wowAppliedGain_ = targetWowGain;
    finalOutAppliedGain_ = finalOutLin;
    updateGainMatchController(preMatchPowerSum, preMatchTotalPowerSum,
                              channels, numSamples, true);
    meters.gainMatchDb.store(gainMatchReady_ ? gainMatchAppliedDb_ : 0.0f,
                             std::memory_order_relaxed);
    meters.gainMatchReady.store(gainMatchReady_, std::memory_order_relaxed);

    // Wrapper-level Reference Bypass is the final handover, after every wet
    // gain and safety stage. It remains independent of all plugin controls, but
    // shares the -1 dBFS output ceiling for an honest internal A/B comparison.
    if (hasBypassDry) {
        std::array<float*, arsonkupik::kMaxChannels> wet{};
        std::array<const float*, arsonkupik::kMaxChannels> dry{};
        for (std::size_t ch = 0; ch < channels; ++ch) {
            wet[ch] = buffer.getWritePointer((int)ch);
            dry[ch] = bypassDry_.getReadPointer((int)ch);
        }

        outPeakL = 0.0f;
        outPeakR = 0.0f;
        const float startupCeiling =
            juce::Decibels::decibelsToGain(kOutputCeilingDb);
        const float startupUtilityGain = inputAppliedGain_
                                       * finalOutAppliedGain_;
        for (int i = 0; i < numSamples; ++i) {
            const float bypassDryMix = bypassMixNeeded
                                     ? smBypass_.getNextValue() : 0.0f;
            float startupDryMix = 0.0f;
            if (startupFadeSamplesRemaining_ > 0) {
                const float linear = (float)startupFadeSamplesRemaining_
                                   / (float)startupFadeSamplesTotal_;
                // Raised-cosine shape keeps both ends derivative-continuous.
                startupDryMix = 0.5f - 0.5f * std::cos(
                    juce::MathConstants<float>::pi * linear);
                --startupFadeSamplesRemaining_;
            }
            const float wetMix = (1.0f - bypassDryMix)
                               * (1.0f - startupDryMix);
            const float startupDryWeight = (1.0f - bypassDryMix)
                                         * startupDryMix;
            float startupDrySafetyGain = 1.0f;
            if (startupDryWeight > 0.0f) {
                float startupDryPeak = 0.0f;
                for (std::size_t ch = 0; ch < channels; ++ch)
                    startupDryPeak = juce::jmax(startupDryPeak,
                        std::abs(dry[ch][i] * startupUtilityGain));
                if (startupDryPeak > startupCeiling)
                    startupDrySafetyGain = startupCeiling / startupDryPeak;
            }
            for (std::size_t ch = 0; ch < channels; ++ch) {
                // Internal bypass uses the continuously prepared reference-limited
                // dry weight. Only the temporary first-load utility contribution
                // needs its separate containment during the cold-start handover.
                const float output = wet[ch][i] * wetMix
                                   + dry[ch][i] * bypassDryMix
                                   + dry[ch][i] * startupUtilityGain
                                                * startupDrySafetyGain
                                                * startupDryWeight;
                wet[ch][i] = output;
                if (ch == 0)
                    outPeakL = juce::jmax(outPeakL, std::abs(output));
                else if (ch == 1)
                    outPeakR = juce::jmax(outPeakR, std::abs(output));
            }
        }
        if (channels == 1)
            outPeakR = outPeakL;
    }

    const float* l = buffer.getReadPointer(0);
    const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr;
    scopeTap.push(l, r, numSamples);

    const auto& m = engine_.meters();
    meters.inputPeakDbL.store(dbFromPeak(inPeakL), std::memory_order_relaxed);
    meters.inputPeakDbR.store(dbFromPeak(inPeakR), std::memory_order_relaxed);
    meters.outputPeakDbL.store(dbFromPeak(outPeakL), std::memory_order_relaxed);
    meters.outputPeakDbR.store(dbFromPeak(outPeakR), std::memory_order_relaxed);
    meters.inputPeakDb.store(juce::jmax(dbFromPeak(inPeakL), dbFromPeak(inPeakR)), std::memory_order_relaxed);
    meters.outputPeakDb.store(juce::jmax(dbFromPeak(outPeakL), dbFromPeak(outPeakR)), std::memory_order_relaxed);
    meters.gainReductionDb.store(juce::jmax((float)m.gain_reduction_db, safetyGrDb_), std::memory_order_relaxed);
    meters.correlation.store((float)m.correlation, std::memory_order_relaxed);
    meters.clipping.store(m.clipping || safetyGrDb_ > 6.0f, std::memory_order_relaxed);
}


// ---------------------------------------------------------------------------
// A/B compare — creative settings only. Gain Match plus INPUT/OUTPUT
// utilities and BYPASS are deliberately excluded so level-matched monitoring
// survives every A/B flip. Message-thread only (driven by the editor switch).
// ---------------------------------------------------------------------------
void Processor::captureAb(AbSnapshot& s) const
{
    s.valid    = true;
    s.profile  = currentFactoryPresetIndex();
    s.enhance  = apvts.getRawParameterValue(ids::enhance)->load();
    s.bass     = apvts.getRawParameterValue(ids::smartBass)->load();
    s.treble   = apvts.getRawParameterValue(ids::smartTreble)->load();
    s.vocal    = apvts.getRawParameterValue(ids::vocalBody)->load();
    s.stereo   = apvts.getRawParameterValue(ids::stereoMagic)->load();
    s.trim     = apvts.getRawParameterValue(ids::outputTrim)->load();
    s.trimLock = apvts.getRawParameterValue(ids::outputTrimLock)->load() > 0.5f;
    s.protect  = apvts.getRawParameterValue(ids::smartProtect)->load() > 0.5f;
}

void Processor::applyAb(const AbSnapshot& s)
{
    auto setParam = [this](const char* id, float plainValue)
    {
        if (auto* param = apvts.getParameter(id)) {
            const float normalized = param->convertTo0to1(plainValue);
            if (std::abs(param->getValue() - normalized) > 0.00001f) {
                param->beginChangeGesture();
                param->setValueNotifyingHost(normalized);
                param->endChangeGesture();
            }
        }
    };

    presetRecallInProgress_.store(true, std::memory_order_release);
    clearActiveUserPreset();
    setParam(ids::preset,         (float)s.profile);
    setParam(ids::enhance,        s.enhance);
    setParam(ids::smartBass,      s.bass);
    setParam(ids::smartTreble,    s.treble);
    setParam(ids::vocalBody,      s.vocal);
    setParam(ids::stereoMagic,    s.stereo);
    setParam(ids::outputTrim,     s.trim);
    setParam(ids::outputTrimLock, s.trimLock ? 1.0f : 0.0f);
    setParam(ids::smartProtect,   s.protect  ? 1.0f : 0.0f);

    creativeParamsDirty_.store(true, std::memory_order_release);
    utilityParamsDirty_.store(true, std::memory_order_release);
    presetModified_.store(true, std::memory_order_release);
    presetRecallInProgress_.store(false, std::memory_order_release);
}

void Processor::selectAbSlot(int slot)
{
    if (!licenseManager.editingAllowed())
        return;

    slot = juce::jlimit(0, 1, slot);
    if (slot == abSlot_)
        return;

    captureAb(abSnaps_[(size_t)abSlot_]);

    if (abSnaps_[(size_t)slot].valid)
        applyAb(abSnaps_[(size_t)slot]);
    else
        abSnaps_[(size_t)slot] = abSnaps_[(size_t)abSlot_]; // first visit: mirror, no audible jump

    abSlot_ = slot;
}

void Processor::copyAbToOtherSlot()
{
    if (!licenseManager.editingAllowed())
        return;

    captureAb(abSnaps_[(size_t)abSlot_]);
    abSnaps_[(size_t)(1 - abSlot_)] = abSnaps_[(size_t)abSlot_];
}

void Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("schema", 2, nullptr);
    {
        const juce::ScopedLock lock(presetStateLock_);
        state.setProperty("activeUserPresetId", activeUserPresetId_, nullptr);
        state.setProperty("activeUserPresetName", activeUserPresetName_, nullptr);
        state.setProperty("activeUserPresetCategory", activeUserPresetCategory_, nullptr);
        state.setProperty("activeUserPresetDescription", activeUserPresetDescription_, nullptr);
        state.setProperty("activeUserPresetFile",
                          activeUserPresetFile_.getFullPathName(), nullptr);
    }
    state.setProperty("presetModified",
        presetModified_.load(std::memory_order_acquire), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void Processor::setStateInformation(const void* data, int sizeInBytes)
{
    presetRecallInProgress_.store(true, std::memory_order_release);
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType())) {
            auto restored = juce::ValueTree::fromXml(*xml);
            const int schema = (int)restored.getProperty("schema", 0);
            apvts.replaceState(restored);
            if (schema < 2) {
                // Old sessions may contain any former preset index. Preserve
                // their audible macro values but attach them to the new neutral
                // Signature Balanced scene.
                if (auto* presetParam = apvts.getParameter(ids::preset))
                    presetParam->setValueNotifyingHost(0.0f);
                clearActiveUserPreset();
                presetModified_.store(true, std::memory_order_release);
            } else {
                const juce::ScopedLock lock(presetStateLock_);
                activeUserPresetId_ =
                    restored.getProperty("activeUserPresetId").toString();
                activeUserPresetName_ =
                    restored.getProperty("activeUserPresetName").toString();
                activeUserPresetCategory_ =
                    restored.getProperty("activeUserPresetCategory").toString();
                activeUserPresetDescription_ =
                    restored.getProperty("activeUserPresetDescription").toString();
                activeUserPresetFile_ = juce::File(
                    restored.getProperty("activeUserPresetFile").toString());
                presetModified_.store(
                    (bool)restored.getProperty("presetModified", false),
                    std::memory_order_release);
            }
        }
    presetRecallInProgress_.store(false, std::memory_order_release);
    creativeParamsDirty_.store(true, std::memory_order_release);
    utilityParamsDirty_.store(true, std::memory_order_release);
}

juce::AudioProcessorEditor* Processor::createEditor()
{
    return nullptr;
}

} // namespace askp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new askp::Processor();
}
