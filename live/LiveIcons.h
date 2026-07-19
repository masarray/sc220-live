#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace askp::live {

std::unique_ptr<juce::Drawable> createLucideIcon (const juce::String& resourceName,
                                                  juce::Colour colour);

class LucideIconButton final : public juce::Button
{
public:
    LucideIconButton (const juce::String& buttonName,
                      const juce::String& resourceName,
                      juce::Colour colour = juce::Colours::white);

    void setIconColour (juce::Colour colour);
    void setOffColour (juce::Colour colour);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    void rebuildIcons();
    void drawFallbackIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float alpha) const;

    juce::String resource;
    juce::Colour iconColour;
    juce::Colour offColour { juce::Colour (0xff8494a7) };
    std::unique_ptr<juce::Drawable> activeIcon;
    std::unique_ptr<juce::Drawable> inactiveIcon;
};

} // namespace askp::live
