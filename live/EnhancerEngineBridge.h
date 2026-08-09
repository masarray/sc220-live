#pragma once

#include "PluginProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

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

    static constexpr const char* engineVersion = "0.5.24";
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

    // Physical SC220 ownership is its own entitlement. The software-only
    // licence timer may expire, but a currently enumerated SC220 must keep the
    // creative controls available without asking the hardware owner to pay for
    // a second software entitlement.
    void setHardwareEntitled (bool entitled) noexcept
    {
        hardwareEntitled_.store (entitled, std::memory_order_release);
    }
    bool isHardwareEntitled() const noexcept
    {
        return hardwareEntitled_.load (std::memory_order_acquire);
    }
    bool editingAllowed() const noexcept
    {
        return isHardwareEntitled() || processor_.licenseManager.editingAllowed();
    }
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

    // SC220 Live owns a dedicated lock-free analyzer tap. The embedded ASK-P
    // editor tap stays disabled permanently, so no per-block enable/disable
    // race can starve the message-thread spectrum reader.
    void setVisualTapEnabled (bool enabled) noexcept
    {
        visualTapEnabled_.store (enabled, std::memory_order_release);
        analyzerTap_.setEnabled (enabled);
        processor_.scopeTap.setEnabled (false);
    }

    askp::ScopeTap& analysisTap() noexcept { return analyzerTap_; }
    const askp::ScopeTap& analysisTap() const noexcept { return analyzerTap_; }

    askp::Processor& processor() noexcept { return processor_; }
    const askp::Processor& processor() const noexcept { return processor_; }

private:
    void setParameter (const char* parameterId, float plainValue) noexcept;
    float getParameter (const char* parameterId, float fallback = 0.0f) const noexcept;

    askp::Processor processor_;
    askp::ScopeTap analyzerTap_;
    std::atomic<bool> visualTapEnabled_ { false };
    // Start permissive so persisted creative settings can be restored before
    // the Windows endpoint watcher performs its first authoritative hardware
    // scan. SC220DeviceWatcher replaces this value immediately when the main UI
    // is constructed.
    std::atomic<bool> hardwareEntitled_ { true };
};

} // namespace askp::live
