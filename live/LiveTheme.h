#pragma once

#include "../src/AskpLookAndFeel.h"

namespace askp::live {


namespace metrics {
inline constexpr int space2 = 2;
inline constexpr int space4 = 4;
inline constexpr int space6 = 6;
inline constexpr int space8 = 8;
inline constexpr int space10 = 10;
inline constexpr int space12 = 12;
inline constexpr int space14 = 14;
inline constexpr int space16 = 16;
inline constexpr int space20 = 20;
inline constexpr int space24 = 24;
inline constexpr int headerHeight = 60;
inline constexpr int footerHeight = 48;
inline constexpr int sidePanelWidth = 220;
inline constexpr int rackRowHeight = 48;
inline constexpr float cardRadius = 12.0f;
inline constexpr float controlRadius = 8.0f;
}

namespace colours {
inline const juce::Colour appBg      { 0xff080d14 };
inline const juce::Colour panel      { 0xff0d141e };
inline const juce::Colour panelHi    { 0xff111b27 };
inline const juce::Colour panelLo    { 0xff0a1018 };
inline const juce::Colour border     { 0xff223142 };
inline const juce::Colour borderSoft { 0xff1b2a3a };
inline const juce::Colour text       { 0xffe8eef6 };
inline const juce::Colour textMuted  { 0xff93a2b5 };
inline const juce::Colour cyan       { 0xff18c7f4 };
inline const juce::Colour blue       { 0xff2b8cff };
inline const juce::Colour violet     { 0xff8b5cf6 };
inline const juce::Colour violetHot  { 0xffb66cff };
inline const juce::Colour amber      { 0xffffa41f };
inline const juce::Colour green      { 0xff55d96b };
inline const juce::Colour red        { 0xffff5f66 };
inline const juce::Colour black      { 0xff05080d };
}

class LiveLookAndFeel final : public askp::AskpLookAndFeel
{
public:
    LiveLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted,
                         bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool separator, bool active, bool highlighted,
                            bool ticked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcut, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted,
                           bool down) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                             juce::Point<int> screenPos,
                                             juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int width, int height) override;
};

void fillPremiumPanel (juce::Graphics&, juce::Rectangle<float>, float radius = 10.0f,
                       bool selected = false);
void drawSectionTitle (juce::Graphics&, juce::Rectangle<float>, const juce::String& title,
                       const juce::String& subtitle = {});

} // namespace askp::live
