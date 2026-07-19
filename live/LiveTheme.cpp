#include "LiveTheme.h"

namespace askp::live {

LiveLookAndFeel::LiveLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, colours::cyan.withAlpha (0.18f));
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::cyan);
    setColour (juce::ComboBox::backgroundColourId, colours::panelLo);
    setColour (juce::ComboBox::outlineColourId, colours::border);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::textMuted);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panelHi);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::cyan.withAlpha (0.12f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cyan);
    setColour (juce::Slider::rotarySliderFillColourId, colours::cyan);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::border);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelHi);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::TooltipWindow::outlineColourId, colours::border);
}

void LiveLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height))
                      .reduced (7.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.60f), centre.x, bounds.getBottom(),
                                 juce::Colours::transparentBlack, centre.x, bounds.getY(), false);
    g.setGradientFill (shadow);
    g.fillEllipse (bounds.translated (0.0f, 3.0f));

    g.setColour (colours::borderSoft);
    g.drawEllipse (bounds.expanded (1.0f), 1.0f);

    const auto arcBounds = bounds.expanded (2.0f);
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 1.0f, radius + 1.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colours::border);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centre.x, centre.y, radius + 1.0f, radius + 1.0f, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (accent.withAlpha (0.22f));
    g.strokePath (active, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (active, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    auto knob = bounds.reduced (7.0f);
    juce::ColourGradient metal (juce::Colour (0xff384554), knob.getX(), knob.getY(),
                                juce::Colour (0xff0a1017), knob.getRight(), knob.getBottom(), false);
    metal.addColour (0.48, juce::Colour (0xff171f29));
    g.setGradientFill (metal);
    g.fillEllipse (knob);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (knob.reduced (1.0f), 1.0f);

    juce::Path marker;
    marker.addRoundedRectangle (-1.1f, -radius * 0.48f, 2.2f, radius * 0.22f, 1.1f);
    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.fillPath (marker, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void LiveLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         const juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
        return;

    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    auto track = juce::Rectangle<float> (0.0f, area.getY(), 12.0f, area.getHeight()).withCentre (area.getCentre());
    g.setColour (colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (track, 6.0f);
    g.setColour (colours::borderSoft.withAlpha (0.9f));
    g.drawRoundedRectangle (track, 6.0f, 1.0f);

    auto fill = track.withTop (sliderPos);
    juce::ColourGradient grad (colours::cyan, fill.getCentreX(), fill.getBottom(),
                               colours::blue, fill.getCentreX(), fill.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 6.0f);

    auto thumb = juce::Rectangle<float> (track.getX() - 11.0f, sliderPos - 11.0f, track.getWidth() + 22.0f, 22.0f);
    juce::ColourGradient thumbGrad (juce::Colour (0xffe9eef4), thumb.getCentreX(), thumb.getY(),
                                    juce::Colour (0xffaab5c2), thumb.getCentreX(), thumb.getBottom(), false);
    thumbGrad.addColour (0.45, juce::Colour (0xffcfd7e2));
    g.setGradientFill (thumbGrad);
    g.fillRoundedRectangle (thumb, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.78f));
    g.drawRoundedRectangle (thumb, 6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.drawLine (thumb.getX() + 8.0f, thumb.getCentreY(), thumb.getRight() - 8.0f, thumb.getCentreY(), 1.2f);
}

void LiveLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto base = button.getToggleState() ? colours::cyan.withAlpha (0.13f)
                                              : colours::panelLo;
    g.setColour (down ? base.brighter (0.08f) : highlighted ? base.brighter (0.04f) : base);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour ((button.getToggleState() ? colours::cyan : colours::border).withAlpha (0.95f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

void LiveLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                      bool, bool)
{
    g.setFont (jakarta (13.0f, true));
    g.setColour (button.getToggleState() ? colours::cyan : colours::text);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (12, 1),
                      juce::Justification::centred, 1);
}

void LiveLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isDown,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width - 1),
                                          static_cast<float> (height - 1));
    g.setColour (isDown ? colours::panelHi.brighter (0.05f) : colours::panelLo);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::cyan.withAlpha (0.8f) : colours::border);
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    const auto cx = static_cast<float> (width - 18);
    const auto cy = static_cast<float> (height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (colours::textMuted);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void LiveLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 36, box.getHeight() - 2);
    label.setFont (jakarta (13.0f));
}

juce::Font LiveLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return jakarta (13.0f);
}

juce::Font LiveLookAndFeel::getPopupMenuFont()
{
    return jakarta (13.0f);
}

void LiveLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f,
                                                     static_cast<float> (width),
                                                     static_cast<float> (height)), 8.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f,
                                                     static_cast<float> (width - 1),
                                                     static_cast<float> (height - 1)), 8.0f, 1.0f);
}

void LiveLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                         bool separator, bool active, bool highlighted,
                                         bool ticked, bool hasSubMenu, const juce::String& text,
                                         const juce::String& shortcut, const juce::Drawable* icon,
                                         const juce::Colour* textColour)
{
    if (separator)
    {
        g.setColour (colours::borderSoft);
        g.fillRect (area.reduced (10, 0).withHeight (1).withCentre (area.getCentre()));
        return;
    }

    if (highlighted && active)
    {
        g.setColour (colours::cyan.withAlpha (0.10f));
        g.fillRoundedRectangle (area.toFloat().reduced (5.0f, 2.0f), 5.0f);
    }

    auto textArea = area.reduced (12, 0);
    if (icon != nullptr)
    {
        auto iconArea = textArea.removeFromLeft (22).toFloat().reduced (2.0f);
        icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, active ? 0.9f : 0.4f);
        textArea.removeFromLeft (6);
    }

    g.setColour (textColour != nullptr ? *textColour
                                       : active ? (highlighted ? colours::cyan : colours::text)
                                                : colours::textMuted.withAlpha (0.55f));
    g.setFont (jakarta (13.0f));
    g.drawText ((ticked ? "[x] " : "") + text, textArea,
                juce::Justification::centredLeft, true);

    if (shortcut.isNotEmpty())
    {
        g.setColour (colours::textMuted);
        g.drawText (shortcut, textArea, juce::Justification::centredRight, true);
    }
    if (hasSubMenu)
    {
        auto arrowArea = area;
        g.drawText (">", arrowArea.removeFromRight (18), juce::Justification::centred, false);
    }
}

void LiveLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool highlighted, bool)
{
    auto area = button.getLocalBounds().toFloat();
    auto toggle = area.removeFromRight (38.0f).withSizeKeepingCentre (34.0f, 18.0f);
    const bool on = button.getToggleState();
    g.setColour (on ? colours::cyan.withAlpha (0.32f) : colours::border);
    g.fillRoundedRectangle (toggle, 9.0f);
    g.setColour (on ? colours::cyan : colours::textMuted);
    const float diameter = 14.0f;
    const float x = on ? toggle.getRight() - diameter - 2.0f : toggle.getX() + 2.0f;
    g.fillEllipse (x, toggle.getY() + 2.0f, diameter, diameter);

    g.setFont (jakarta (12.5f, true));
    g.setColour (highlighted ? colours::text : colours::textMuted);
    g.drawText (button.getButtonText(), area.reduced (2.0f, 0.0f),
                juce::Justification::centredLeft, false);
}

void LiveLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                   int width, int height)
{
    auto bubble = juce::Rectangle<float> (6.0f, 0.0f, static_cast<float> (width - 12), static_cast<float> (height - 8));
    juce::Path callout;
    callout.addRoundedRectangle (bubble, 9.0f);
    callout.startNewSubPath (22.0f, bubble.getBottom());
    callout.lineTo (30.0f, bubble.getBottom() + 8.0f);
    callout.lineTo (38.0f, bubble.getBottom());

    g.setColour (juce::Colours::black.withAlpha (0.26f));
    g.fillPath (callout, juce::AffineTransform::translation (0.0f, 2.0f));

    juce::ColourGradient bg (colours::panelHi.brighter (0.03f), bubble.getX(), bubble.getY(),
                             colours::panelLo, bubble.getRight(), bubble.getBottom(), false);
    g.setGradientFill (bg);
    g.fillPath (callout);

    g.setColour (colours::cyan.withAlpha (0.16f));
    g.fillRoundedRectangle (bubble.reduced (1.5f).withHeight (12.0f), 8.0f);
    g.setColour (colours::border);
    g.strokePath (callout, juce::PathStrokeType (1.0f));

    g.setColour (colours::text);
    g.setFont (jakarta (12.0f));
    g.drawFittedText (text, 16, 7, width - 28, height - 20,
                      juce::Justification::centredLeft, 2);
}

void fillPremiumPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                       float radius, bool selected)
{
    juce::ColourGradient gradient (selected ? colours::panelHi.brighter (0.025f) : colours::panelHi,
                                   bounds.getX(), bounds.getY(),
                                   colours::panelLo, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour ((selected ? colours::cyan.withAlpha (0.55f) : colours::border).withAlpha (0.9f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<float> area,
                       const juce::String& title, const juce::String& subtitle)
{
    auto& lf = askp::AskpLookAndFeel::get();
    g.setColour (colours::text);
    g.setFont (lf.jakarta (14.0f, true));
    g.drawText (title, area, juce::Justification::centredLeft, false);
    if (subtitle.isNotEmpty())
    {
        g.setColour (colours::textMuted);
        g.setFont (lf.jakarta (10.5f, true));
        g.drawText (subtitle, area, juce::Justification::centredRight, false);
    }
}

} // namespace askp::live
