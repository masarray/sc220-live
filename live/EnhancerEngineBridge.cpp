#include "EnhancerEngineBridge.h"
#include <cmath>

namespace askp::live {
namespace {
const juce::StringArray& names()
{
    static const juce::StringArray list {
        "Signature Balanced", "Transparent Polish", "Open & Detailed", "Warm Glue",
        "Dense Mix Clarity", "Dark Mix Lift", "Bright Mix Safe", "Punch & Separation",
        "Vocal Forward & Silky", "Guitar Definition & Body", "Bass Authority",
        "Drums Punch & Skin", "Acoustic Natural Air"
    };
    return list;
}
float peakDb (const juce::AudioBuffer<float>& audio, int channel) noexcept
{
    if (audio.getNumChannels() == 0 || audio.getNumSamples() == 0)
        return -120.0f;
    const auto peak = audio.getMagnitude (juce::jmin (channel, audio.getNumChannels()-1), 0, audio.getNumSamples());
    return juce::Decibels::gainToDecibels (peak, -120.0f);
}
}
juce::String EnhancerEngineBridge::loadBundled (const juce::File& file, double rate, int block)
{
    InputVst3Rack::PreparedInstance candidate;
    const auto error = InputVst3Rack::createPrepared (file, rate, block, candidate);
    if (error.isNotEmpty()) return error;
    rack.install (std::move (candidate), rate, block);
    bindParameters();
    for (auto* parameter : parameters)
    {
        if (parameter == nullptr)
        {
            rack.clear();
            parameters.fill (nullptr);
            return "The bundled ArSonKuPik VST3 parameter contract changed. "
                   "Keep the pinned v0.5.26 plugin installed alongside SC220 Live.";
        }
    }
    setGainMatchEnabled (false);
    return {};
}
void EnhancerEngineBridge::bindParameters() noexcept
{
    parameters.fill (nullptr);
    if (auto* plugin = rack.getProcessor())
    {
        const auto& all = plugin->getParameters();
        auto match = [&all] (const juce::String& name)
        {
            for (auto* p : all)
                if (p != nullptr && p->getName (128).equalsIgnoreCase (name))
                    return p;
            return static_cast<juce::AudioProcessorParameter*> (nullptr);
        };
        parameters[preset]=match ("Factory Preset");
        parameters[inputGain]=match ("Input Gain");
        parameters[finalGain]=match ("Final Output Gain");
        parameters[enhanceParam]=match ("Enhance");
        parameters[bass]=match ("Smart Bass");
        parameters[treble]=match ("Smart Treble");
        parameters[vocal]=match ("Vocal Body");
        parameters[stereo]=match ("Stereo Magic");
        parameters[gainMatch]=match ("Gain Match");
    }
}
void EnhancerEngineBridge::prepare (double rate, int maximumBlockSize)
{
    if (rack.isLoaded()) rack.prepare (rate, maximumBlockSize);
}
void EnhancerEngineBridge::release() { rack.release(); }
void EnhancerEngineBridge::process (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) noexcept
{
    inputL.store (peakDb (audio,0), std::memory_order_relaxed);
    inputR.store (peakDb (audio,1), std::memory_order_relaxed);
    rack.process (audio,midi);
    outputL.store (peakDb (audio,0), std::memory_order_relaxed);
    outputR.store (peakDb (audio,1), std::memory_order_relaxed);
    if (visualTapEnabled.load (std::memory_order_relaxed) && audio.getNumChannels()>0)
        analyzerTap.push (audio.getReadPointer (0),
                          audio.getReadPointer (juce::jmin (1,audio.getNumChannels()-1)),
                          audio.getNumSamples());
}
void EnhancerEngineBridge::setPlain (Parameter id, float value, float minimum, float maximum) noexcept
{
    if (auto* p=parameters[id])
    {
        const float normalised=juce::jlimit (0.0f,1.0f,(value-minimum)/(maximum-minimum));
        if (std::abs (p->getValue()-normalised)>0.00001f)
            p->setValueNotifyingHost (normalised);
    }
}
float EnhancerEngineBridge::readPlain (Parameter id,float minimum,float maximum,float fallback) const noexcept
{
    if (auto* p=parameters[id]) return minimum+(maximum-minimum)*p->getValue();
    return fallback;
}
void EnhancerEngineBridge::setInputGainDb(float v) noexcept
{ if(editingAllowed())setPlain(inputGain,v,-12,12); }
void EnhancerEngineBridge::setFinalOutputGainDb(float v) noexcept
{ if(editingAllowed())setPlain(finalGain,v,-12,12); }
void EnhancerEngineBridge::setGainMatchEnabled(bool v) noexcept { setPlain(gainMatch,v?1.0f:0.0f,0,1); }
bool EnhancerEngineBridge::isGainMatchEnabled() const noexcept
{ return readPlain(gainMatch,0,1,0)>0.5f; }
void EnhancerEngineBridge::setPresetIndex(int i)
{ if (editingAllowed()) setPlain(preset,static_cast<float>(juce::jlimit(0,factoryProfileCount-1,i)),0,static_cast<float>(factoryProfileCount-1)); }
int EnhancerEngineBridge::presetIndex() const noexcept
{ return juce::jlimit(0,factoryProfileCount-1,juce::roundToInt(readPlain(preset,0,static_cast<float>(factoryProfileCount-1),0))); }
juce::StringArray EnhancerEngineBridge::presetNames() const { return names(); }
juce::String EnhancerEngineBridge::presetCategory(int) const { return "ArSonKuPik"; }
juce::String EnhancerEngineBridge::presetDescription(int index) const
{ return names()[juce::jlimit(0,factoryProfileCount-1,index)]; }
void EnhancerEngineBridge::setEnhance(float v) noexcept { if(editingAllowed())setPlain(enhanceParam,v,0,100); }
void EnhancerEngineBridge::setSmartBass(float v) noexcept { if(editingAllowed())setPlain(bass,v,0,100); }
void EnhancerEngineBridge::setSmartTreble(float v) noexcept { if(editingAllowed())setPlain(treble,v,0,100); }
void EnhancerEngineBridge::setVocalBody(float v) noexcept { if(editingAllowed())setPlain(vocal,v,0,100); }
void EnhancerEngineBridge::setStereoMagic(float v) noexcept { if(editingAllowed())setPlain(stereo,v,0,100); }
float EnhancerEngineBridge::enhance() const noexcept { return readPlain(enhanceParam,0,100,78); }
float EnhancerEngineBridge::smartBass() const noexcept { return readPlain(bass,0,100,70); }
float EnhancerEngineBridge::smartTreble() const noexcept { return readPlain(treble,0,100,80); }
float EnhancerEngineBridge::vocalBody() const noexcept { return readPlain(vocal,0,100,76); }
float EnhancerEngineBridge::stereoMagic() const noexcept { return readPlain(stereo,0,100,88); }
EnhancerEngineBridge::MeterSnapshot EnhancerEngineBridge::meters() const noexcept
{
    MeterSnapshot m;
    m.inputPeakDbL=inputL.load(std::memory_order_relaxed);
    m.inputPeakDbR=inputR.load(std::memory_order_relaxed);
    m.outputPeakDbL=outputL.load(std::memory_order_relaxed);
    m.outputPeakDbR=outputR.load(std::memory_order_relaxed);
    m.preSafetyPeakDbL=m.outputPeakDbL;
    m.preSafetyPeakDbR=m.outputPeakDbR;
    m.inputOver=m.inputPeakDbL>=0||m.inputPeakDbR>=0;
    m.outputOver=m.outputPeakDbL>=0||m.outputPeakDbR>=0;
    m.preSafetyOver=m.outputOver;
    m.clipping=m.outputOver;
    // VST3 has no portable side-channel for the plugin's private gain reduction.
    // Report zero rather than fabricating compressor telemetry from peak delta.
    return m;
}
} // namespace askp::live
