#pragma once

#include "MusicalNoiseClean.h"
#include "VisualScopeTap.h"
#include "arsonkupik_dsp.hpp"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <cstdint>
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
        float outputPeakDbL = -120.0f;
        float outputPeakDbR = -120.0f;
        float gainReductionDb = 0.0f;
        float expanderReductionDb = 0.0f;
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
    void setInputTrimDb (float value) noexcept { inputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value)); }
    float getInputTrimDb() const noexcept { return inputTrimDb.load(); }
    void setOutputTrimDb (float value) noexcept { outputTrimDb.store (juce::jlimit (-60.0f, 12.0f, value)); }
    float getOutputTrimDb() const noexcept { return outputTrimDb.load(); }

    void setEngineEnabled (bool value) noexcept { engineEnabled.store (value); }
    bool isEngineEnabled() const noexcept { return engineEnabled.load(); }
    void setBypassed (bool value) noexcept { bypassed.store (value); }
    bool isBypassed() const noexcept { return bypassed.load(); }
    void setOutputMuted (bool value) noexcept { outputMuted.store (value); }
    bool isOutputMuted() const noexcept { return outputMuted.load(); }
    void restartAudio();

    void setPresetIndex (int index);
    int getPresetIndex() const noexcept { return presetIndex.load(); }
    juce::StringArray getPresetNames() const;

    void setEnhance (float value) noexcept;
    void setSmartBass (float value) noexcept;
    void setSmartTreble (float value) noexcept;
    void setVocalBody (float value) noexcept;
    void setStereoMagic (float value) noexcept;

    float getEnhance() const noexcept { return enhance.load(); }
    float getSmartBass() const noexcept { return smartBass.load(); }
    float getSmartTreble() const noexcept { return smartTreble.load(); }
    float getVocalBody() const noexcept { return vocalBody.load(); }
    float getStereoMagic() const noexcept { return stereoMagic.load(); }

    MusicalNoiseClean& getNoiseClean() noexcept { return noiseClean; }
    SmoothDownwardExpander& getExpander() noexcept { return expander; }

    Status getStatus() const;
    std::unique_ptr<juce::XmlElement> createDeviceState() const;

    void setVisualTapEnabled (bool enabled) noexcept { visualTap.setEnabled (enabled); }
    int copyLatestVisualSamples (float* left, float* right, int wanted) noexcept
    {
        return visualTap.popLatest (left, right, wanted);
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
    enum EngineSlotState : int
    {
        slotFree = 0,
        slotBuilding,
        slotPending,
        slotActive,
        slotTransition
    };

    static constexpr int engineSlotCount = 3;

    class EngineBuilderThread final : public juce::Thread
    {
    public:
        explicit EngineBuilderThread (LiveAudioEngine& ownerToUse)
            : juce::Thread ("ArSonKuPik DSP Builder"), owner (ownerToUse) {}
        void run() override;
    private:
        LiveAudioEngine& owner;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void requestEngineRebuild() noexcept;
    bool buildLatestEngineOffAudioThread();
    arsonkupik::RuntimeParams makeRuntimeParamsSnapshot() const;
    void activatePendingEngineIfReady (bool instant) noexcept;
    void processEnhancerWithTransition (int channels, int numSamples) noexcept;
    void clearOutputs (float* const* outputChannelData, int channels, int samples) noexcept;
    void updateMeters (const juce::AudioBuffer<float>& input,
                       const juce::AudioBuffer<float>& output) noexcept;
    void refreshDeviceStatus();
    static bool looksLikeVirtualCable (const juce::String& name);

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> processBuffer;
    juce::AudioBuffer<float> detectorBuffer;
    juce::AudioBuffer<float> transitionBuffer;

    MusicalNoiseClean noiseClean;
    SmoothDownwardExpander expander;
    LiveSafetyLimiter safetyLimiter;
    VisualScopeTap visualTap;

    std::array<std::unique_ptr<arsonkupik::ArSonKuPikEngine>, engineSlotCount> enhancerSlots;
    std::array<std::atomic<int>, engineSlotCount> engineSlotStates {};
    std::atomic<int> activeEngineSlot { 0 };
    std::atomic<int> pendingEngineSlot { -1 };
    int transitionOldSlot = -1;
    int transitionSamplesRemaining = 0;
    int transitionTotalSamples = 1;
    std::atomic<uint64_t> parameterRevision { 1 };
    std::atomic<uint64_t> builtRevision { 0 };
    juce::CriticalSection engineBuildLock;
    EngineBuilderThread engineBuilder { *this };

    std::atomic<bool> engineEnabled { true };
    std::atomic<float> inputTrimDb { 0.0f };
    std::atomic<float> outputTrimDb { 0.0f };
    std::atomic<int> inputChannelMode { 2 };
    std::atomic<bool> bypassed { false };
    std::atomic<bool> outputMuted { false };
    std::atomic<int> presetIndex { 0 };
    std::atomic<float> enhance { 65.0f };
    std::atomic<float> smartBass { 55.0f };
    std::atomic<float> smartTreble { 70.0f };
    std::atomic<float> vocalBody { 65.0f };
    std::atomic<float> stereoMagic { 85.0f };

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
