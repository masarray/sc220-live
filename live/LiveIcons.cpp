#include "LiveIcons.h"
#include "LiveTheme.h"
#include "BinaryData.h"

namespace askp::live {

std::unique_ptr<juce::Drawable> createLucideIcon (const juce::String& resourceName,
                                                  juce::Colour colour)
{
    int dataSize = 0;
    const auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize);
    if (data == nullptr || dataSize <= 0)
        return {};

    auto xmlText = juce::String::fromUTF8 (data, dataSize)
                       .replace ("currentColor", "#" + colour.toDisplayString (false));
    if (auto xml = juce::XmlDocument::parse (xmlText))
        return juce::Drawable::createFromSVG (*xml);
    return {};
}

LucideIconButton::LucideIconButton (const juce::String& buttonName,
                                    const juce::String& resourceName,
                                    juce::Colour colour)
    : juce::Button (buttonName), resource (resourceName), iconColour (colour),
      icon (createLucideIcon (resourceName, colour))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LucideIconButton::setIconColour (juce::Colour colour)
{
    iconColour = colour;
    icon = createLucideIcon (resource, colour);
    repaint();
}

void LucideIconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    if (highlighted || down || getToggleState())
    {
        g.setColour ((getToggleState() ? colours::cyan : colours::text).withAlpha (down ? 0.13f : 0.075f));
        g.fillRoundedRectangle (bounds.reduced (2.0f), 6.0f);
    }

    if (icon != nullptr)
    {
        const float alpha = isEnabled() ? (highlighted ? 1.0f : 0.80f) : 0.30f;
        icon->drawWithin (g, bounds.reduced (juce::jmax (5.0f, bounds.getWidth() * 0.25f)),
                          juce::RectanglePlacement::centred, alpha);
    }
}

} // namespace askp::live
