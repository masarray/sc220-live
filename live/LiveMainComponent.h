#pragma once

#include "LiveAudioEngine.h"
#include "LiveWidgets.h"
#include "LiveLiquidVisualizer.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace askp::live {

class LiveMainComponent final : public juce::Component,
                                private juce::Timer
{
public:
    struct WindowActions
    {
        std::function<void()> minimise;
        std::function<void()> toggleMaximise;
        std::function<void()> hideToTray;
        std::function<void()> showMiniController;
    };

    LiveMainComponent (LiveAudioEngine& engine, WindowActions actions);
    ~LiveMainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    void refreshAllControls();
    void setUiActive (bool active);

private:
    void timerCallback() override;
    void configureControls();
    void updateDeviceLists();
    void updateStatus();
    void showSettingsCard();
    void showExternalVstRoadmap();
    void setComboTextWithoutNotification (juce::ComboBox&, const juce::String&);
    void showError (const juce::String& title, const juce::String& message);

    LiveAudioEngine& audioEngine;
    WindowActions windowActions;
    LiveLookAndFeel lookAndFeel;
    juce::ComponentDragger windowDragger;

    LucideIconButton settingsButton { "Settings", "settings_svg", colours::textMuted };
    LucideIconButton trayButton { "Tray", "panel_top_open_svg", colours::textMuted };
    LucideIconButton minimiseButton { "Minimise", "minimize_2_svg", colours::textMuted };
    LucideIconButton maximiseButton { "Maximise", "maximize_2_svg", colours::textMuted };
    LucideIconButton closeButton { "Close", "x_svg", colours::textMuted };

    juce::ComboBox inputDevice;
    juce::ComboBox outputDevice;

    CompactRackRow inputTrimRow { 1, "sliders_horizontal_svg", "Input Trim", false };
    CompactRackRow noiseRow { 2, "audio_waveform_svg", "Musical Noise Clean", true, true };
    CompactRackRow vstPreRow { 3, "plug_zap_svg", "External VST Slot", false };
    VolumeFader inputFader { "Input Volume" };

    KnobControl noiseReduction { "Reduction", " dB", colours::cyan, 0.0, 12.0, 0.1 };
    KnobControl silenceAmount { "Silence", "%", colours::violet, 0.0, 100.0, 1.0 };
    KnobControl preserveAmount { "Preserve", "%", colours::cyan, 0.0, 100.0, 1.0 };
    LiveLiquidVisualizer visualizer;
    StatusPill profileStatus;
    juce::TextButton learnNoise { "Learn Noise" };

    juce::ComboBox preset;
    KnobControl enhanceKnob { "Enhance", "%", colours::cyan, 0.0, 100.0, 0.1 };
    KnobControl bassKnob { "Smart Bass", "%", colours::violet, 0.0, 100.0, 0.1 };
    KnobControl trebleKnob { "Smart Treble", "%", colours::cyan, 0.0, 100.0, 0.1 };
    KnobControl vocalKnob { "Vocal Body", "%", colours::violet, 0.0, 100.0, 0.1 };
    KnobControl stereoKnob { "Stereo Magic", "%", colours::cyan, 0.0, 100.0, 0.1 };
    GainReductionMeter gainReductionMeter;

    CompactRackRow vstPostRow { 1, "plug_zap_svg", "External VST Slot", false };
    CompactRackRow outputTrimRow { 2, "sliders_horizontal_svg", "Output Trim", false };
    CompactRackRow limiterRow { 3, "shield_check_svg", "Safety Limiter", false, false, true };
    VolumeFader outputFader { "Output Volume" };

    MiniStat cpuStat { "CPU", "cpu_svg" };
    MiniStat latencyStat { "Total Latency", "activity_svg" };
    MiniStat dropoutsStat { "Dropouts", "radio_svg" };
    MiniStat profileStat { "Profile", "circle_check_svg" };
    MiniStat trayStat { "Tray Mode", "monitor_up_svg" };

    juce::TooltipWindow tooltip { this, 600 };
    bool updatingControls = false;
    bool uiActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveMainComponent)
};

} // namespace askp::live
