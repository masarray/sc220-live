#pragma once

#include "LiveAudioEngine.h"

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

    void setUiActive (bool active);
    void refreshAllControls();

private:
    class SurfaceLookAndFeel;
    class StereoMeter;
    class SignatureDisplay;
    class LoudnessDisplay;

    void timerCallback() override;
    void configureControls();
    void showAudioSettings();
    void drawHeader (juce::Graphics&);
    void drawInputSurface (juce::Graphics&);
    void drawSignatureSurface (juce::Graphics&);
    void drawMasterSurface (juce::Graphics&);
    void drawFooter (juce::Graphics&);

    LiveAudioEngine& audioEngine;
    WindowActions windowActions;
    std::unique_ptr<SurfaceLookAndFeel> lookAndFeel;

    juce::Image recordingTechLogo;
    juce::Image sc220Image;

    juce::TextButton minimiseButton { "-" };
    juce::TextButton maximiseButton { "[]" };
    juce::TextButton closeButton { "X" };
    juce::TextButton settingsButton { "SETTINGS" };
    juce::TextButton goLiveButton { "GO LIVE" };

    juce::ComboBox presetBox;
    juce::ComboBox outputBox;
    juce::TextButton stereoButton { "STEREO" };
    juce::TextButton dualMonoButton { "DUAL MONO" };
    juce::ComboBox pcSourceBox;

    juce::Slider inputTrim;
    juce::Slider sc220Fader;
    juce::Slider pcFader;
    juce::Slider duckAmount;
    std::array<juce::Slider, 5> macroKnobs;
    juce::Slider streamFader;
    juce::Slider monitorKnob;

    juce::TextButton sc220Mute { "MUTE" };
    juce::TextButton streamMute { "MUTE" };
    juce::TextButton detailsButton { "DETAILS" };

    std::unique_ptr<StereoMeter> sc220Meter;
    std::unique_ptr<StereoMeter> pcMeter;
    std::unique_ptr<StereoMeter> streamMeter;
    std::unique_ptr<SignatureDisplay> signatureDisplay;
    std::unique_ptr<LoudnessDisplay> loudnessDisplay;

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> inputArea;
    juce::Rectangle<int> signatureArea;
    juce::Rectangle<int> masterArea;
    juce::Rectangle<int> footerArea;
    juce::Rectangle<int> sc220StripArea;
    juce::Rectangle<int> pcStripArea;

    LiveAudioEngine::Status lastStatus;
    bool uiActive = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SC220MainComponent)
};

} // namespace askp::live
