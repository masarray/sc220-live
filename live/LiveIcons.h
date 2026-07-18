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
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::String resource;
    juce::Colour iconColour;
    std::unique_ptr<juce::Drawable> icon;
};

} // namespace askp::live
