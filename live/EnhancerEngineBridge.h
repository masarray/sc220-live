#pragma once
#include "InputVst3Rack.h"
#include "LicenseManager.h"
#include "ScopeTap.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace askp::live {
// One SC220-owned control facade around the version-pinned public ArSonKuPik
// VST3 binary. No private DSP headers, compile-time processor or source patches.
class EnhancerEngineBridge final
{
public:
    struct MeterSnapshot
    {
        float inputPeakDbL=-120.0f,inputPeakDbR=-120.0f;
        float outputPeakDbL=-120.0f,outputPeakDbR=-120.0f;
        float gainReductionDb=0.0f,safetyGainReductionDb=0.0f;
        float preSafetyPeakDbL=-120.0f,preSafetyPeakDbR=-120.0f;
        bool inputOver=false,preSafetyOver=false,outputOver=false;
        float gainMatchDb=0.0f,correlation=1.0f;
        bool gainMatchReady=false,clipping=false;
    };
    static constexpr const char* engineVersion="0.5.26";
    static constexpr int factoryProfileCount=13;

    EnhancerEngineBridge() = default;
    juce::String loadBundled (const juce::File& bundle, double sampleRate, int blockSize);
    bool isLoaded() const noexcept { return rack.isLoaded(); }
    void prepare (double sampleRate, int maximumBlockSize);
    void release();
    void process (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) noexcept;
    int latencySamples() const noexcept { return rack.getLatencySamples(); }

    void setInputGainDb (float valueDb) noexcept;
    void setFinalOutputGainDb (float valueDb) noexcept;
    void setBypassed (bool value) noexcept { rack.setBypassed (value); }
    bool isBypassed() const noexcept { return rack.isBypassed(); }
    void setGainMatchEnabled (bool value) noexcept;
    bool isGainMatchEnabled() const noexcept;
    askp::LicenseManager& licenseManager() noexcept { return license; }
    const askp::LicenseManager& licenseManager() const noexcept { return license; }
    void setHardwareEntitled (bool value) noexcept
    { hardwareEntitled.store (value, std::memory_order_release); }
    bool isHardwareEntitled() const noexcept
    { return hardwareEntitled.load (std::memory_order_acquire); }
    bool editingAllowed() const noexcept
    { return isHardwareEntitled() || license.editingAllowed(); }
    bool isLicensed() const noexcept { return license.isLicensed(); }

    void setPresetIndex (int index);
    int presetIndex() const noexcept;
    juce::StringArray presetNames() const;
    juce::String presetCategory (int index) const;
    juce::String presetDescription (int index) const;
    void setEnhance (float value) noexcept;
    void setSmartBass (float value) noexcept;
    void setSmartTreble (float value) noexcept;
    void setVocalBody (float value) noexcept;
    void setStereoMagic (float value) noexcept;
    float enhance() const noexcept;
    float smartBass() const noexcept;
    float smartTreble() const noexcept;
    float vocalBody() const noexcept;
    float stereoMagic() const noexcept;
    MeterSnapshot meters() const noexcept;
    void setVisualTapEnabled (bool enable) noexcept
    {
        visualTapEnabled.store (enable, std::memory_order_release);
        analyzerTap.setEnabled (enable);
    }
    askp::ScopeTap& analysisTap() noexcept { return analyzerTap; }
    const askp::ScopeTap& analysisTap() const noexcept { return analyzerTap; }

private:
    enum Parameter { preset, inputGain, finalGain, enhanceParam, bass, treble, vocal, stereo, gainMatch, count };
    void bindParameters() noexcept;
    void setPlain (Parameter, float value, float minimum, float maximum) noexcept;
    float readPlain (Parameter, float minimum, float maximum, float fallback) const noexcept;
    InputVst3Rack rack;
    askp::LicenseManager license;
    askp::ScopeTap analyzerTap;
    std::array<juce::AudioProcessorParameter*, count> parameters{};
    std::atomic<bool> visualTapEnabled{false},hardwareEntitled{true};
    std::atomic<float> inputL{-120},inputR{-120},outputL{-120},outputR{-120};
};
} // namespace askp::live
