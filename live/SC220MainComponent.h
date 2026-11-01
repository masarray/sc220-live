#pragma once

#include "LiveAudioEngine.h"
#include "LiveIcons.h"
#include "LiveLicensePanel.h"
#include "SC220DeviceWatcher.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <functional>
#include <memory>

namespace askp::live {

class SC220MainComponent final : public juce::Component,
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

    SC220MainComponent (LiveAudioEngine& engine, WindowActions actions);
    ~SC220MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void setUiActive (bool active);
    void refreshAllControls();

private:
    class SurfaceLookAndFeel;
    class StereoMeter;
    class LiveSpectrumDisplay;
    class LoudnessDisplay;

    void timerCallback() override;
    void configureControls();
    void refreshMonitorDevices();
    void showSettingsMenu();
    void showAudioSettings();
    void showVoiceFxMenu();
    void chooseVoiceVst3();
    void showVoiceVst3Editor();
    void showChannelInsertMenu (int channel, int slot);
    void chooseChannelInsert (int channel, int slot);
    void showChannelInsertEditor (int channel, int slot);
    void refreshChannelInserts();
    void drawHeader (juce::Graphics&);
    void drawInputSurface (juce::Graphics&);
    void drawSignatureSurface (juce::Graphics&);
    void drawMasterSurface (juce::Graphics&);
    void drawFooter (juce::Graphics&);
    bool headerDragAllowedAt (juce::Point<int>) const noexcept;
    static bool looksLikeSc220Input (const juce::String& name) noexcept;

    LiveAudioEngine& audioEngine;
    SC220DeviceWatcher sc220DeviceWatcher { audioEngine };
    WindowActions windowActions;
    std::unique_ptr<SurfaceLookAndFeel> lookAndFeel;

    juce::Image recordingTechLogo;
    juce::Image sc220Image;

    LucideIconButton minimiseButton { "Minimise", "minimize_2_svg", juce::Colours::white };
    LucideIconButton maximiseButton { "Maximise", "maximize_2_svg", juce::Colours::white };
    LucideIconButton closeButton { "Close", "x_svg", juce::Colours::white };
    LucideIconButton settingsButton { "Settings", "settings_svg", juce::Colours::white };
    LucideIconButton enhancerPowerButton { "Smart Enhancer", "power_svg", juce::Colours::white };
    LucideIconButton monitorPowerButton { "Monitor On / Off", "headphones_svg", juce::Colours::white };
    juce::TextButton goLiveButton { "GO LIVE" };
    juce::TextButton voiceFxButton { "+ VST3 FX" };
    std::unique_ptr<juce::FileChooser> voiceFxChooser;
    std::unique_ptr<juce::DocumentWindow> voiceFxEditorWindow;
    // Compact DAW insert buttons docked alongside the existing faders.
    std::array<std::array<juce::TextButton, 3>, 2> channelInsertButtons;
    std::array<std::array<std::unique_ptr<juce::DocumentWindow>, 3>, 2> channelInsertEditors;
    std::unique_ptr<juce::FileChooser> channelInsertChooser;

    juce::ComboBox presetBox;
    juce::ComboBox outputBox;
    juce::ComboBox monitorOutputBox;
    juce::TextButton stereoButton { "STEREO" };
    juce::TextButton dualMonoButton { "DUAL MONO" };
    juce::TextButton noiseCleanButton { "DENOISE" };
    juce::TextButton learnNoiseButton { "LEARN" };
    juce::ComboBox pcSourceBox;
    juce::TextButton programMonitorButton { "PROGRAM" };
    juce::TextButton musicCueButton { "MUSIC CUE" };

    juce::Slider inputTrim;
    juce::Slider sc220Fader;
    juce::Slider sc220MonoLeftFader;
    juce::Slider sc220MonoRightFader;
    juce::Slider pcFader;
    juce::Slider duckAmount;
    std::array<juce::Slider, 5> macroKnobs;
    juce::Slider streamFader;
    juce::Slider monitorKnob;

    juce::TextButton sc220Mute { "MUTE" };
    juce::TextButton streamMute { "MUTE" };

    std::unique_ptr<StereoMeter> sc220Meter;
    std::unique_ptr<StereoMeter> pcMeter;
    std::unique_ptr<StereoMeter> streamMeter;
    std::unique_ptr<LiveSpectrumDisplay> spectrumDisplay;
    std::unique_ptr<LoudnessDisplay> loudnessDisplay;
    std::unique_ptr<LiveLicensePanel> licensePanel;

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> inputArea;
    juce::Rectangle<int> signatureArea;
    juce::Rectangle<int> masterArea;
    juce::Rectangle<int> footerArea;
    juce::Rectangle<int> sc220StripArea;
    juce::Rectangle<int> pcStripArea;

    juce::ComponentDragger windowDragger;
    bool draggingWindow = false;
    LiveAudioEngine::Status lastStatus;
    bool uiActive = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SC220MainComponent)
};

} // namespace askp::live
