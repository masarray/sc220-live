#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace askp::live {

std::unique_ptr<juce::Drawable> createLucideIcon (const juce::String& resourceName,
                                                  juce::Colour colour);

class LucideIconButton final : public juce::Button,
                               private juce::Timer
{
public:
    LucideIconButton (const juce::String& buttonName,
                      const juce::String& resourceName,
                      juce::Colour colour = juce::Colours::white);

    void setIconColour (juce::Colour colour);
    void setOffColour (juce::Colour colour);
    void setEmbeddedLabels (juce::String activeText,
                            juce::String inactiveText,
                            juce::Font labelFont);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void startAnimation();
    void rebuildIcons();
    void drawFallbackIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float alpha) const;

    juce::String resource;
    juce::Colour iconColour;
    juce::Colour offColour { juce::Colour (0xff8494a7) };
    juce::String activeEmbeddedLabel;
    juce::String inactiveEmbeddedLabel;
    juce::Font embeddedLabelFont { juce::FontOptions (9.0f) };
    std::unique_ptr<juce::Drawable> activeIcon;
    std::unique_ptr<juce::Drawable> inactiveIcon;
    float hoverMix = 0.0f;
    float pressMix = 0.0f;
};

} // namespace askp::live
