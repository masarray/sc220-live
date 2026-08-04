#include "EnhancerEngineBridge.h"

#include <cmath>

namespace askp::live {
namespace {
constexpr auto inputGainId = "input_gain_db";
constexpr auto finalOutputGainId = "final_output_gain_db";
constexpr auto enhanceId = "enhance";
constexpr auto smartBassId = "smart_bass";
constexpr auto smartTrebleId = "smart_treble";
constexpr auto vocalBodyId = "vocal_body";
constexpr auto stereoMagicId = "stereo_magic";
constexpr auto bypassId = "bypass";
constexpr auto gainMatchId = "gain_match";
} // namespace

EnhancerEngineBridge::EnhancerEngineBridge()
{
    static_assert (ASKP_DSP_HEADER_REVISION == 2026073102,
                   "Unexpected ArSonKuPik engine revision");
    jassert (processor_.presetNames().size() == factoryProfileCount);
    processor_.scopeTap.setEnabled (false);
    analyzerTap_.setEnabled (false);
    setGainMatchEnabled (false);
}

void EnhancerEngineBridge::prepare (double sampleRate, int maximumBlockSize)
{
    processor_.setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
    processor_.prepareToPlay (sampleRate, maximumBlockSize);
    processor_.scopeTap.setEnabled (false);
    analyzerTap_.setEnabled (visualTapEnabled_.load (std::memory_order_acquire));
}

void EnhancerEngineBridge::release()
{
    processor_.scopeTap.setEnabled (false);
    analyzerTap_.setEnabled (false);
    processor_.releaseResources();
}

void EnhancerEngineBridge::process (juce::AudioBuffer<float>& audio,
                                    juce::MidiBuffer& midi)
{
    // ASK-P's editor tap remains disabled. The bridge publishes one clean
    // post-Signature block to its own queue after DSP, with no per-block state
    // toggling and no allocation or locking on the audio thread.
    processor_.processBlock (audio, midi);

    if (! visualTapEnabled_.load (std::memory_order_acquire)
        || audio.getNumChannels() <= 0
        || audio.getNumSamples() <= 0)
        return;

    const auto* left = audio.getReadPointer (0);
    const auto* right = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : left;
    analyzerTap_.push (left, right, audio.getNumSamples());
}

void EnhancerEngineBridge::setInputGainDb (float valueDb) noexcept
{
    if (! editingAllowed()) return;
    setParameter (inputGainId, juce::jlimit (-12.0f, 12.0f, valueDb));
}

void EnhancerEngineBridge::setFinalOutputGainDb (float valueDb) noexcept
{
    if (! editingAllowed()) return;
    setParameter (finalOutputGainId, juce::jlimit (-12.0f, 12.0f, valueDb));
}

void EnhancerEngineBridge::setBypassed (bool bypassed) noexcept
{
    setParameter (bypassId, bypassed ? 1.0f : 0.0f);
}

bool EnhancerEngineBridge::isBypassed() const noexcept
{
    return getParameter (bypassId) > 0.5f;
}

void EnhancerEngineBridge::setGainMatchEnabled (bool enabled) noexcept
{
    setParameter (gainMatchId, enabled ? 1.0f : 0.0f);
}

bool EnhancerEngineBridge::isGainMatchEnabled() const noexcept
{
    return getParameter (gainMatchId) > 0.5f;
}

void EnhancerEngineBridge::setPresetIndex (int index)
{
    if (! editingAllowed()) return;
    const auto names = processor_.presetNames();
    if (names.isEmpty())
        return;

    index = juce::jlimit (0, names.size() - 1, index);
    // The public authority method performs the complete v0.5.23 profile handoff:
    // scene dimensions, five visible macros, internal output trim and protect.
    processor_.applyPresetMacroToParameters (index);
}

int EnhancerEngineBridge::presetIndex() const noexcept
{
    const int count = processor_.presetNames().size();
    if (count <= 0)
        return 0;
    return juce::jlimit (0, count - 1, processor_.currentFactoryPresetIndex());
}

juce::StringArray EnhancerEngineBridge::presetNames() const
{
    return processor_.presetNames();
}

juce::String EnhancerEngineBridge::presetCategory (int index) const
{
    const int count = processor_.presetNames().size();
    return count > 0 ? processor_.presetCategory (juce::jlimit (0, count - 1, index))
                     : juce::String();
}

juce::String EnhancerEngineBridge::presetDescription (int index) const
{
    const int count = processor_.presetNames().size();
    return count > 0 ? processor_.presetDescription (juce::jlimit (0, count - 1, index))
                     : juce::String();
}

void EnhancerEngineBridge::setEnhance (float value) noexcept
{
    if (! editingAllowed()) return;
    setParameter (enhanceId, juce::jlimit (0.0f, 100.0f, value));
}

void EnhancerEngineBridge::setSmartBass (float value) noexcept
{
    if (! editingAllowed()) return;
    setParameter (smartBassId, juce::jlimit (0.0f, 100.0f, value));
}

void EnhancerEngineBridge::setSmartTreble (float value) noexcept
{
    if (! editingAllowed()) return;
    setParameter (smartTrebleId, juce::jlimit (0.0f, 100.0f, value));
}

void EnhancerEngineBridge::setVocalBody (float value) noexcept
{
    if (! editingAllowed()) return;
    setParameter (vocalBodyId, juce::jlimit (0.0f, 100.0f, value));
}

void EnhancerEngineBridge::setStereoMagic (float value) noexcept
{
    if (! editingAllowed()) return;
    setParameter (stereoMagicId, juce::jlimit (0.0f, 100.0f, value));
}

float EnhancerEngineBridge::enhance() const noexcept
{
    return getParameter (enhanceId, 78.0f);
}

float EnhancerEngineBridge::smartBass() const noexcept
{
    return getParameter (smartBassId, 68.0f);
}

float EnhancerEngineBridge::smartTreble() const noexcept
{
    return getParameter (smartTrebleId, 80.0f);
}

float EnhancerEngineBridge::vocalBody() const noexcept
{
    return getParameter (vocalBodyId, 76.0f);
}

float EnhancerEngineBridge::stereoMagic() const noexcept
{
    return getParameter (stereoMagicId, 88.0f);
}

EnhancerEngineBridge::MeterSnapshot EnhancerEngineBridge::meters() const noexcept
{
    MeterSnapshot result;
    result.inputPeakDbL = processor_.meters.inputPeakDbL.load (std::memory_order_relaxed);
    result.inputPeakDbR = processor_.meters.inputPeakDbR.load (std::memory_order_relaxed);
    result.outputPeakDbL = processor_.meters.outputPeakDbL.load (std::memory_order_relaxed);
    result.outputPeakDbR = processor_.meters.outputPeakDbR.load (std::memory_order_relaxed);
    result.gainReductionDb = processor_.meters.gainReductionDb.load (std::memory_order_relaxed);
    result.safetyGainReductionDb = processor_.meters.safetyGainReductionDb.load (std::memory_order_relaxed);
    result.preSafetyPeakDbL = processor_.meters.preSafetyPeakDbL.load (std::memory_order_relaxed);
    result.preSafetyPeakDbR = processor_.meters.preSafetyPeakDbR.load (std::memory_order_relaxed);
    result.inputOver = processor_.meters.inputOver.load (std::memory_order_relaxed);
    result.preSafetyOver = processor_.meters.preSafetyOver.load (std::memory_order_relaxed);
    result.outputOver = processor_.meters.outputOver.load (std::memory_order_relaxed);
    result.gainMatchDb = processor_.meters.gainMatchDb.load (std::memory_order_relaxed);
    result.gainMatchReady = processor_.meters.gainMatchReady.load (std::memory_order_relaxed);
    result.correlation = processor_.meters.correlation.load (std::memory_order_relaxed);
    result.clipping = processor_.meters.clipping.load (std::memory_order_relaxed);
    return result;
}

void EnhancerEngineBridge::setParameter (const char* parameterId,
                                         float plainValue) noexcept
{
    if (auto* parameter = processor_.apvts.getParameter (parameterId))
    {
        const float normalised = parameter->convertTo0to1 (plainValue);
        if (std::abs (parameter->getValue() - normalised) > 0.000001f)
            parameter->setValueNotifyingHost (normalised);
    }
}

float EnhancerEngineBridge::getParameter (const char* parameterId,
                                          float fallback) const noexcept
{
    if (const auto* value = processor_.apvts.getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);
    return fallback;
}

} // namespace askp::live
