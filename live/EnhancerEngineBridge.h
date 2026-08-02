#pragma once

#include "PluginProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace askp::live {

// Stable boundary between the standalone live rack and the embedded
// ArSonKuPik Signature Engine. LiveAudioEngine never reaches into APVTS or
// hard-codes engine internals directly; future engine ports are confined here.
class EnhancerEngineBridge final
{
public:
    struct MeterSnapshot
    {
        float inputPeakDbL = -120.0f;
        float inputPeakDbR = -120.0f;
        float outputPeakDbL = -120.0f;
        float outputPeakDbR = -120.0f;
        float gainReductionDb = 0.0f;
        float safetyGainReductionDb = 0.0f;
        float preSafetyPeakDbL = -120.0f;
        float preSafetyPeakDbR = -120.0f;
        bool inputOver = false;
        bool preSafetyOver = false;
        bool outputOver = false;
        float gainMatchDb = 0.0f;
        bool gainMatchReady = false;
        float correlation = 1.0f;
        bool clipping = false;
    };

    static constexpr const char* engineVersion = "0.5.23";
    static constexpr int engineRevision = ASKP_DSP_HEADER_REVISION;
    static constexpr int factoryProfileCount = 13;

    EnhancerEngineBridge();

    void prepare (double sampleRate, int maximumBlockSize);
    void release();
    void process (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi);

    int latencySamples() const noexcept { return processor_.getLatencySamples(); }

    void setInputGainDb (float valueDb) noexcept;
    void setFinalOutputGainDb (float valueDb) noexcept;
    void setBypassed (bool bypassed) noexcept;
    bool isBypassed() const noexcept;
    void setGainMatchEnabled (bool enabled) noexcept;
    bool isGainMatchEnabled() const noexcept;

    askp::LicenseManager& licenseManager() noexcept { return processor_.licenseManager; }
    const askp::LicenseManager& licenseManager() const noexcept { return processor_.licenseManager; }
    bool editingAllowed() const noexcept { return processor_.licenseManager.editingAllowed(); }
    bool isLicensed() const noexcept { return processor_.licenseManager.isLicensed(); }

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

    void setVisualTapEnabled (bool enabled) noexcept
    {
        processor_.scopeTap.setEnabled (enabled);
    }

    askp::Processor& processor() noexcept { return processor_; }
    const askp::Processor& processor() const noexcept { return processor_; }

private:
    void setParameter (const char* parameterId, float plainValue) noexcept;
    float getParameter (const char* parameterId, float fallback = 0.0f) const noexcept;

    askp::Processor processor_;
};

} // namespace askp::live
