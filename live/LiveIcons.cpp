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
    : juce::Button (buttonName), resource (resourceName), iconColour (colour)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    rebuildIcons();
}

void LucideIconButton::rebuildIcons()
{
    activeIcon = createLucideIcon (resource, iconColour);
    inactiveIcon = createLucideIcon (resource, offColour);
}

void LucideIconButton::setIconColour (juce::Colour colour)
{
    iconColour = colour;
    rebuildIcons();
    repaint();
}

void LucideIconButton::setOffColour (juce::Colour colour)
{
    offColour = colour;
    rebuildIcons();
    repaint();
}

void LucideIconButton::drawFallbackIcon (juce::Graphics& g, juce::Rectangle<float> area,
                                          juce::Colour colour, float alpha) const
{
    area = area.reduced (juce::jmax (5.0f, area.getWidth() * 0.29f));
    g.setColour (colour.withAlpha (alpha));
    const juce::PathStrokeType stroke (1.7f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);

    if (resource.containsIgnoreCase ("minimize"))
    {
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.7f);
        return;
    }
    if (resource.containsIgnoreCase ("maximize"))
    {
        g.drawRoundedRectangle (area.reduced (1.0f), 1.5f, 1.6f);
        return;
    }
    if (resource.containsIgnoreCase ("panel_top_open"))
    {
        g.drawRoundedRectangle (area.reduced (1.0f), 2.0f, 1.5f);
        g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * 0.34f), area.getX() + 2.0f, area.getRight() - 2.0f);
        return;
    }
    if (resource == "x_svg" || resource.containsIgnoreCase ("close"))
    {
        g.drawLine (area.getX(), area.getY(), area.getRight(), area.getBottom(), 1.8f);
        g.drawLine (area.getRight(), area.getY(), area.getX(), area.getBottom(), 1.8f);
        return;
    }
    if (resource.containsIgnoreCase ("plus"))
    {
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 1.8f);
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.8f);
        return;
    }
    if (resource.containsIgnoreCase ("power"))
    {
        juce::Path arc;
        arc.addCentredArc (area.getCentreX(), area.getCentreY() + 1.0f,
                           area.getWidth() * 0.42f, area.getHeight() * 0.42f, 0.0f,
                           -juce::MathConstants<float>::pi * 0.73f,
                            juce::MathConstants<float>::pi * 0.73f, true);
        g.strokePath (arc, stroke);
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getCentreY(), 1.8f);
        return;
    }

    g.drawEllipse (area.reduced (2.0f), 1.6f);
}

void LucideIconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    const bool toggleButton = getClickingTogglesState();
    const bool active = ! toggleButton || getToggleState();

    // Momentary header/utility buttons stay visually quiet until hover. Toggle
    // power buttons retain a subtle active surface so bypass state is obvious.
    const bool showSurface = down || highlighted || (toggleButton && active);
    if (showSurface)
    {
        const auto fillColour = active ? colours::cyan : colours::textMuted;
        const float fillAlpha = down ? 0.14f : highlighted ? 0.09f : 0.065f;
        g.setColour (fillColour.withAlpha (fillAlpha));
        g.fillRoundedRectangle (bounds.reduced (2.0f), 7.0f);
        g.setColour ((active ? colours::cyan : colours::border).withAlpha (highlighted ? 0.34f : 0.18f));
        g.drawRoundedRectangle (bounds.reduced (2.5f), 7.0f, 1.0f);
    }

    const auto drawColour = active ? iconColour : offColour;
    const float alpha = isEnabled() ? (active ? (highlighted ? 1.0f : 0.88f) : 0.56f) : 0.28f;
    auto* drawable = active ? activeIcon.get() : inactiveIcon.get();
    auto iconBounds = bounds.reduced (juce::jmax (5.0f, bounds.getWidth() * 0.25f));
    if (drawable != nullptr)
        drawable->drawWithin (g, iconBounds, juce::RectanglePlacement::centred, alpha);
    else
        drawFallbackIcon (g, bounds, drawColour, alpha);
}

} // namespace askp::live
