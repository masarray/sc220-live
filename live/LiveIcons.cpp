#include "LiveIcons.h"
#include "LiveTheme.h"
#include "BinaryData.h"

#include <cmath>

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

void LucideIconButton::startAnimation()
{
    if (! isTimerRunning())
        startTimerHz (60);
}

void LucideIconButton::timerCallback()
{
    const auto stepTowards = [] (float current, float target)
    {
        return current + (target - current) * 0.22f;
    };

    const float targetHover = isMouseOver (true) ? 1.0f : 0.0f;
    const float targetPress = isMouseButtonDown() ? 1.0f : 0.0f;
    hoverMix = stepTowards (hoverMix, targetHover);
    pressMix = stepTowards (pressMix, targetPress);
    repaint();

    if (std::abs (hoverMix - targetHover) < 0.015f && std::abs (pressMix - targetPress) < 0.015f)
    {
        hoverMix = targetHover;
        pressMix = targetPress;
        stopTimer();
        repaint();
    }
}

void LucideIconButton::mouseEnter (const juce::MouseEvent& e)
{
    juce::Button::mouseEnter (e);
    startAnimation();
}

void LucideIconButton::mouseExit (const juce::MouseEvent& e)
{
    juce::Button::mouseExit (e);
    startAnimation();
}

void LucideIconButton::mouseDown (const juce::MouseEvent& e)
{
    juce::Button::mouseDown (e);
    startAnimation();
}

void LucideIconButton::mouseUp (const juce::MouseEvent& e)
{
    juce::Button::mouseUp (e);
    startAnimation();
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

    const float hover = juce::jlimit (0.0f, 1.0f, hoverMix + (highlighted ? 0.12f : 0.0f));
    const float press = juce::jlimit (0.0f, 1.0f, pressMix + (down ? 0.18f : 0.0f));
    const bool showSurface = active || hover > 0.02f || press > 0.02f;

    if (showSurface)
    {
        auto shell = bounds.reduced (2.0f);
        const auto fillColour = active ? colours::cyan : colours::textMuted;
        juce::ColourGradient surface (colours::panelHi.brighter (0.02f), shell.getX(), shell.getY(),
                                      colours::panelLo, shell.getRight(), shell.getBottom(), false);
        surface.multiplyOpacity (0.90f);
        g.setGradientFill (surface);
        g.fillRoundedRectangle (shell, 8.0f);

        g.setColour (fillColour.withAlpha (0.05f + hover * 0.08f + press * 0.06f));
        g.fillRoundedRectangle (shell.reduced (0.5f), 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.12f + press * 0.04f));
        g.drawRoundedRectangle (shell.reduced (1.0f), 7.0f, 1.0f);
        g.setColour ((active ? colours::cyan : colours::border).withAlpha (0.16f + hover * 0.14f + press * 0.08f));
        g.drawRoundedRectangle (shell.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.018f + hover * 0.02f));
        g.fillRoundedRectangle (juce::Rectangle<float> (shell.getX() + 6.0f, shell.getY() + 1.5f,
                                                        juce::jmax (0.0f, shell.getWidth() - 12.0f), 1.5f), 1.0f);
    }

    const auto drawColour = active ? iconColour : offColour;
    const float alpha = isEnabled() ? (active ? (0.86f + hover * 0.12f) : (0.52f + hover * 0.08f)) : 0.28f;
    auto* drawable = active ? activeIcon.get() : inactiveIcon.get();
    auto iconBounds = bounds.reduced (juce::jmax (5.0f, bounds.getWidth() * 0.25f));
    const auto iconShift = juce::AffineTransform::translation (0.0f, press * 0.6f);
    if (drawable != nullptr)
        drawable->drawWithin (g, iconBounds, juce::RectanglePlacement::centred, alpha);
    else
        drawFallbackIcon (g, iconBounds.transformedBy (iconShift), drawColour, alpha);
}

} // namespace askp::live
