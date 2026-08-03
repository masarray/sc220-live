#pragma once

#include "MusicalNoiseClean.h"
#include "EnhancerEngineBridge.h"
#include "PcAudioLoopbackCapture.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <array>
#include <memory>

namespace askp::live {

class LiveAudioEngine final : public juce::AudioIODeviceCallback,
                              private juce::ChangeListener
{
public:
    struct Status
    {
        bool running = false;
        bool deviceOpen = false;
        bool outputIsVirtualCable = false;
        double sampleRate = 0.0;
        int bufferSize = 0;
        float cpuPercent = 0.0f;
        float callbackPeakPercent = 0.0f;
        float inputPeakDbL = -120.0f;
        float inputPeakDbR = -120.0f;
        float pcPeakDbL = -120.0f;
        float pcPeakDbR = -120.0f;
        float outputPeakDbL = -120.0f;
        float outputPeakDbR = -120.0f;
        float gainReductionDb = 0.0f;
        float expanderReductionDb = 0.0f;
        float duckGainDb = 0.0f;
        float loudnessLufs = -120.0f;
        bool pcLoopbackRunning = false;
        int pcUnderflows = 0;
        int pcOverflows = 0;
        int dropouts = 0;
        unsigned int visualDrops = 0;
        int totalLatencySamples = 0;
        juce::String inputDevice;
        juce::String outputDevice;
        juce::String lastError;
    };

    LiveAudioEngine();
    ~LiveAudioEngine() override;

    bool initialise (const juce::XmlElement* savedDeviceState = nullptr);
    void shutdown();

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    askp::Processor& getAskpProcessor() noexcept { return enhancerBridge.processor(); }
    const askp::Processor& getAskpProcessor() const noexcept { return enhancerBridge.processor(); }
    EnhancerEngineBridge& getEnhancerBridge() noexcept { return enhancerBridge; }
    askp::LicenseManager& getLicenseManager() noexcept { return enhancerBridge.licenseManager(); }
    const askp::LicenseManager& getLicenseManager() const noexcept { return enhancerBridge.licenseManager(); }
    bool fullFeaturesAllowed() const noexcept { return enhancerBridge.editingAllowed(); }

    juce::StringArray getInputDeviceNames();
    juce::StringArray getOutputDeviceNames();
    juce::Array<double> getAvailableSampleRates() const;
    juce::Array<int> getAvailableBufferSizes() const;

    juce::String selectInputDevice (const juce::String& name);
    juce::String selectOutputDevice (const juce::String& name);
    juce::String setSampleRate (double value);
    juce::String setBufferSize (int value);
    void setInputChannelMode (int channels) noexcept { inputChannelMode.store (channels <= 1 ? 1 : 2); }
    int getInputChannelMode() const noexcept { return inputChannelMode.load(); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

    void setInputTrimDb (float value) noexcept;
    float getInputTrimDb() const noexcept { return inputTrimDb.load(); }
    void setSc220FaderDb (float value) noexcept;
    float getSc220FaderDb() const noexcept { return sc220FaderDb.load(); }
    void setSc220Muted (bool value) noexcept { sc220Muted.store (value); }
    bool isSc220Muted() const noexcept { return sc220Muted.load(); }
    void setOutputTrimDb (float value) noexcept;
    float getOutputTrimDb() const noexcept { return outputTrimDb.load(); }
    void setInputTrimBypassed (bool value) noexcept;
    bool isInputTrimBypassed() const noexcept { return inputTrimBypassed.load(); }
    void setOutputTrimBypassed (bool value) noexcept;
    bool isOutputTrimBypassed() const noexcept { return outputTrimBypassed.load(); }

    void setPcAudioEnabled (bool value) noexcept { pcAudioEnabled.store (value); }
    bool isPcAudioEnabled() const noexcept { return pcAudioEnabled.load(); }
    void setPcAudioGainDb (float value) noexcept;
    float getPcAudioGainDb() const noexcept { return pcAudioGainDb.load(); }
    void setPcAudioMuted (bool value) noexcept { pcAudioMuted.store (value); }
    bool isPcAudioMuted() const noexcept { return pcAudioMuted.load(); }
    void setDuckingEnabled (bool value) noexcept { duckingEnabled.store (value); }
    bool isDuckingEnabled() const noexcept { return duckingEnabled.load(); }
    void setDuckingDepthDb (float value) noexcept;
    float getDuckingDepthDb() const noexcept { return duckingDepthDb.load(); }

    void setEngineEnabled (bool value) noexcept { engineEnabled.store (value); }
    bool isEngineEnabled() const noexcept { return engineEnabled.load(); }
    void setBypassed (bool value) noexcept;
    bool isBypassed() const noexcept { return enhancerBypassed.load(); }
    void setOutputMuted (bool value) noexcept { outputMuted.store (value); }
    bool isOutputMuted() const noexcept { return outputMuted.load(); }
    void restartAudio();
    void applyVstReferenceConditions() noexcept;

    void setPresetIndex (int index);
    int getPresetIndex() const noexcept;
    juce::StringArray getPresetNames() const;

    void setEnhance (float value) noexcept;
    void setSmartBass (float value) noexcept;
    void setSmartTreble (float value) noexcept;
    void setVocalBody (float value) noexcept;
    void setStereoMagic (float value) noexcept;

    float getEnhance() const noexcept;
    float getSmartBass() const noexcept;
    float getSmartTreble() const noexcept;
    float getVocalBody() const noexcept;
    float getStereoMagic() const noexcept;

    MusicalNoiseClean& getNoiseClean() noexcept { return noiseClean; }
    SmoothDownwardExpander& getExpander() noexcept { return expander; }

    Status getStatus() const;
    std::unique_ptr<juce::XmlElement> createDeviceState() const;

    void setVisualTapEnabled (bool enabled) noexcept
    {
        enhancerBridge.setVisualTapEnabled (enabled);
    }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void clearOutputs (float* const* outputChannelData, int channels, int samples) noexcept;
    void updateMeters (const juce::AudioBuffer<float>& finalOutput) noexcept;
    void refreshDeviceStatus();
    juce::String applyDeviceSelection (const juce::String* inputName,
                                       const juce::String* outputName);
    void updateInputTrimRouting() noexcept;
    void updateOutputTrimRouting() noexcept;
    static bool looksLikeVirtualCable (const juce::String& name);

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> processBuffer;
    juce::AudioBuffer<float> detectorBuffer;
    juce::AudioBuffer<float> pcAudioBuffer;
    juce::MidiBuffer emptyMidi;

    MusicalNoiseClean noiseClean;
    SmoothDownwardExpander expander;
    EnhancerEngineBridge enhancerBridge;
    PcAudioLoopbackCapture pcLoopback;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainLinear;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sc220MixGainLinear;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainLinear;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pcGainLinear;

    std::atomic<bool> engineEnabled { true };
    std::atomic<float> inputTrimDb { 0.0f };
    std::atomic<float> sc220FaderDb { 0.0f };
    std::atomic<float> outputTrimDb { 0.0f };
    std::atomic<bool> inputTrimBypassed { false };
    std::atomic<bool> outputTrimBypassed { false };
    std::atomic<int> inputChannelMode { 2 };
    std::atomic<bool> enhancerBypassed { false };
    std::atomic<bool> outputMuted { true };
    std::atomic<bool> sc220Muted { false };
    std::atomic<bool> pcAudioEnabled { true };
    std::atomic<float> pcAudioGainDb { -10.0f };
    std::atomic<bool> pcAudioMuted { false };
    std::atomic<bool> duckingEnabled { true };
    std::atomic<float> duckingDepthDb { -12.0f };
    std::atomic<float> duckGainDb { 0.0f };
    float duckGainLinear = 1.0f;
    std::array<juce::dsp::IIR::Filter<float>, 2> loudnessShelf;
    std::array<juce::dsp::IIR::Filter<float>, 2> loudnessHighPass;
    double loudnessMeanSquare = 0.0;
    std::atomic<float> loudnessLufs { -120.0f };

    std::atomic<bool> callbackRegistered { false };
    std::atomic<bool> running { false };
    std::atomic<bool> deviceOpen { false };
    std::atomic<bool> outputCable { false };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBufferSize { 0 };
    std::atomic<float> cpuPercent { 0.0f };
    std::atomic<float> callbackPeakPercent { 0.0f };
    std::atomic<float> inputPeakL { -120.0f };
    std::atomic<float> inputPeakR { -120.0f };
    std::atomic<float> outputPeakL { -120.0f };
    std::atomic<float> outputPeakR { -120.0f };
    std::atomic<float> gainReduction { 0.0f };
    std::atomic<int> dropoutCount { 0 };
    std::atomic<int> totalLatencySamples { 0 };

    mutable juce::CriticalSection statusLock;
    juce::String currentInputName;
    juce::String currentOutputName;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveAudioEngine)
};

} // namespace askp::live
