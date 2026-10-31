#pragma once

#include "MusicalNoiseClean.h"
#include "EnhancerEngineBridge.h"
#include "InputVst3Rack.h"
#include "SmartPleasantPresence.h"
#include "MonitorOutputClient.h"
#include "PcAudioLoopbackCapture.h"
#include "ProgrammeLoudnessIntegrator.h"

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
    enum class MonitorSource
    {
        program = 0,
        musicCue = 1
    };

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
        float voiceGainReductionDb = 0.0f;
        float musicGainReductionDb = 0.0f;
        float masterGainReductionDb = 0.0f;
        float pleasantMidHighReductionDb = 0.0f;
        float deEssReductionDb = 0.0f;
        float expanderReductionDb = 0.0f;
        float duckGainDb = 0.0f;
        float loudnessLufs = -120.0f;
        float truePeakDbtp = -120.0f;
        float loudnessElapsedSeconds = 0.0f;
        bool loudnessReady = false;
        bool pcLoopbackRunning = false;
        bool pcAudioLoopBlocked = false;
        int pcUnderflows = 0;
        int pcOverflows = 0;
        bool monitorRunning = false;
        bool monitorPrimed = false;
        int monitorBufferedSamples = 0;
        int monitorUnderflows = 0;
        int monitorOverflows = 0;
        int dropouts = 0;
        unsigned int visualDrops = 0;
        int totalLatencySamples = 0;
        int voiceLatencySamples = 0;
        int musicDspLatencySamples = 0;
        juce::String inputDevice;
        juce::String outputDevice;
        juce::String pcAudioDevice;
        juce::String pcAudioEndpointId;
        juce::String pcAudioError;
        juce::String monitorDevice;
        juce::String monitorError;
        juce::String lastError;
    };

    LiveAudioEngine();
    ~LiveAudioEngine() override;

    bool initialise (const juce::XmlElement* savedDeviceState = nullptr);
    void shutdown();

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    // UI/message-thread only; the callback is detached during a plugin swap.
    juce::String loadVoiceVst3 (const juce::File& pluginFile);
    void removeVoiceVst3();
    void setVoiceVst3Bypassed (bool bypass) noexcept { voiceVst3.setBypassed (bypass); }
    bool isVoiceVst3Bypassed() const noexcept { return voiceVst3.isBypassed(); }
    bool hasVoiceVst3() const noexcept { return voiceVst3.isLoaded(); }
    juce::String getVoiceVst3Name() const { return voiceVst3.getName(); }
    juce::AudioPluginInstance* getVoiceVst3Processor() const noexcept { return voiceVst3.getProcessor(); }
    askp::Processor& getAskpProcessor() noexcept { return voiceEnhancerBridge.processor(); }
    const askp::Processor& getAskpProcessor() const noexcept { return voiceEnhancerBridge.processor(); }
    EnhancerEngineBridge& getEnhancerBridge() noexcept { return voiceEnhancerBridge; }
    EnhancerEngineBridge& getVoiceEnhancerBridge() noexcept { return voiceEnhancerBridge; }
    EnhancerEngineBridge& getMusicEnhancerBridge() noexcept { return musicEnhancerBridge; }
    askp::ScopeTap& programmeAnalyzerTap() noexcept { return programmeAnalyzer; }
    void setProgrammeAnalyzerEnabled (bool enabled) noexcept
    {
        programmeAnalyzer.setEnabled (enabled);
    }
    void resetProgrammeLoudness() noexcept
    {
        loudnessResetRequested.store (true, std::memory_order_release);
    }
    MonitorOutputClient& getMonitorOutputClient() noexcept { return monitorOutput; }
    askp::LicenseManager& getLicenseManager() noexcept { return voiceEnhancerBridge.licenseManager(); }
    const askp::LicenseManager& getLicenseManager() const noexcept { return voiceEnhancerBridge.licenseManager(); }
    bool fullFeaturesAllowed() const noexcept { return voiceEnhancerBridge.editingAllowed(); }

    juce::StringArray getInputDeviceNames();
    juce::StringArray getOutputDeviceNames();
    juce::StringArray getMonitorOutputDeviceNames();
    juce::StringArray getPcAudioEndpointNames();
    juce::Array<double> getAvailableSampleRates() const;
    juce::Array<int> getAvailableBufferSizes() const;

    juce::String selectInputDevice (const juce::String& name);
    juce::String selectOutputDevice (const juce::String& name);
    juce::String selectMonitorOutputDevice (const juce::String& name);
    juce::String selectPcAudioEndpoint (const juce::String& name);
    juce::String getPcAudioEndpointName() const;
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

    void setMonitorSource (MonitorSource value) noexcept { monitorSource.store (value); }
    MonitorSource getMonitorSource() const noexcept { return monitorSource.load(); }
    void setMonitorGainDb (float value) noexcept { monitorOutput.setGainDb (value); }
    float getMonitorGainDb() const noexcept { return monitorOutput.getGainDb(); }
    void setMonitorMuted (bool value) noexcept { monitorOutput.setMuted (value); }
    bool isMonitorMuted() const noexcept { return monitorOutput.isMuted(); }

    void setEngineEnabled (bool value) noexcept { engineEnabled.store (value); }
    bool isEngineEnabled() const noexcept { return engineEnabled.load(); }
    void setBypassed (bool value) noexcept;
    bool isBypassed() const noexcept { return voiceEnhancerBypassed.load() && musicEnhancerBypassed.load(); }
    void setVoiceEnhancerBypassed (bool value) noexcept;
    bool isVoiceEnhancerBypassed() const noexcept { return voiceEnhancerBypassed.load(); }
    void setMusicEnhancerBypassed (bool value) noexcept;
    bool isMusicEnhancerBypassed() const noexcept { return musicEnhancerBypassed.load(); }
    void setOutputMuted (bool value) noexcept { outputMuted.store (value); }
    bool isOutputMuted() const noexcept { return outputMuted.load(); }
    void restartAudio();
    void applyVstReferenceConditions() noexcept;

    // Voice Signature Engine (kept under the original API for UI compatibility).
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

    // Independent Music Signature Engine. Music is enhanced before ducking.
    void setMusicPresetIndex (int index);
    int getMusicPresetIndex() const noexcept;
    juce::StringArray getMusicPresetNames() const;
    void setMusicEnhance (float value) noexcept;
    void setMusicSmartBass (float value) noexcept;
    void setMusicSmartTreble (float value) noexcept;
    void setMusicVocalBody (float value) noexcept;
    void setMusicStereoMagic (float value) noexcept;
    float getMusicEnhance() const noexcept;
    float getMusicSmartBass() const noexcept;
    float getMusicSmartTreble() const noexcept;
    float getMusicVocalBody() const noexcept;
    float getMusicStereoMagic() const noexcept;

    MusicalNoiseClean& getNoiseClean() noexcept { return noiseClean; }
    SmoothDownwardExpander& getExpander() noexcept { return expander; }

    Status getStatus() const;
    std::unique_ptr<juce::XmlElement> createDeviceState() const;

    void setVisualTapEnabled (bool enabled) noexcept
    {
        voiceEnhancerBridge.setVisualTapEnabled (enabled);
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
    void updateMeters (const juce::AudioBuffer<float>& finalOutput,
                       float preLimiterPeakDb) noexcept;
    void refreshDeviceStatus();
    void publishLatencyEstimate() noexcept;
    bool quiesceForPluginSwap() noexcept;
    juce::String applyDeviceSelection (const juce::String* inputName,
                                       const juce::String* outputName);
    void updateInputTrimRouting() noexcept;
    void updateOutputTrimRouting() noexcept;
    static bool looksLikeVirtualCable (const juce::String& name);
    static float peakDb (const juce::AudioBuffer<float>& buffer) noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> processBuffer;   // Voice lane, then master mix bus.
    juce::AudioBuffer<float> detectorBuffer;  // Voice detector before fader.
    juce::AudioBuffer<float> pcAudioBuffer;   // Enhanced music, then program ducking.
    juce::AudioBuffer<float> musicCueBuffer;  // Enhanced music fader, pre-ducking.
    juce::MidiBuffer voiceMidi;
    juce::MidiBuffer musicMidi;

    MusicalNoiseClean noiseClean;
    SmoothDownwardExpander expander;
    EnhancerEngineBridge voiceEnhancerBridge;
    EnhancerEngineBridge musicEnhancerBridge;
    InputVst3Rack voiceVst3;
    SmartPleasantPresence pleasantPresence;
    askp::ScopeTap programmeAnalyzer;
    PcAudioLoopbackCapture pcLoopback;
    MonitorOutputClient monitorOutput;
    juce::dsp::Limiter<float> masterLimiter;

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
    std::atomic<bool> voiceEnhancerBypassed { false };
    std::atomic<bool> musicEnhancerBypassed { false };
    std::atomic<bool> outputMuted { true };
    std::atomic<bool> sc220Muted { false };
    std::atomic<bool> pcAudioEnabled { true };
    std::atomic<float> pcAudioGainDb { -10.0f };
    std::atomic<bool> pcAudioMuted { false };
    std::atomic<bool> pcAudioLoopBlocked { false };
    std::atomic<bool> duckingEnabled { true };
    std::atomic<float> duckingDepthDb { -12.0f };
    std::atomic<float> duckGainDb { 0.0f };
    std::atomic<MonitorSource> monitorSource { MonitorSource::program };
    float duckGainLinear = 1.0f;
    std::array<juce::dsp::IIR::Filter<float>, 2> loudnessShelf;
    std::array<juce::dsp::IIR::Filter<float>, 2> loudnessHighPass;
    ProgrammeLoudnessIntegrator loudnessIntegrator;
    juce::dsp::Oversampling<float> truePeakOversampler {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false
    };
    float truePeakMaximumLinear = 0.0f;
    std::atomic<float> loudnessLufs { -120.0f };
    std::atomic<float> truePeakDbtp { -120.0f };
    std::atomic<float> loudnessElapsedSeconds { 0.0f };
    std::atomic<bool> loudnessReady { false };
    std::atomic<bool> loudnessResetRequested { false };

    // Active callbacks never wait; the UI swaps a fully prepared VST only
    // after the in-flight count drains, then destroys the previous processor.
    std::atomic<bool> vstSwapSuspended { false };
    std::atomic<int> audioCallbacksInFlight { 0 };
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
    std::atomic<float> voiceGainReduction { 0.0f };
    std::atomic<float> musicGainReduction { 0.0f };
    std::atomic<float> masterGainReduction { 0.0f };
    std::atomic<int> dropoutCount { 0 };
    std::atomic<int> totalLatencySamples { 0 };
    std::atomic<int> voiceLatencySamples { 0 };
    std::atomic<int> musicDspLatencySamples { 0 };

    mutable juce::CriticalSection statusLock;
    juce::String currentInputName;
    juce::String currentOutputName;
    juce::String pcAudioRoutingError;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveAudioEngine)
};

} // namespace askp::live
