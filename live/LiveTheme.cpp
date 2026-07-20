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
    const bool enabled = slider.isEnabled();
    const bool engaged = slider.isMouseOverOrDragging();
    const float alpha = enabled ? 1.0f : 0.34f;

    juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.60f * alpha), centre.x, bounds.getBottom(),
                                 juce::Colours::transparentBlack, centre.x, bounds.getY(), false);
    g.setGradientFill (shadow);
    g.fillEllipse (bounds.translated (0.0f, 3.0f));

    g.setColour ((engaged && enabled ? accent : colours::borderSoft).withAlpha (engaged && enabled ? 0.26f : alpha));
    g.drawEllipse (bounds.expanded (1.0f), engaged && enabled ? 1.4f : 1.0f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 1.0f, radius + 1.0f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (colours::border.withAlpha (enabled ? 1.0f : 0.6f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path active;
    active.addCentredArc (centre.x, centre.y, radius + 1.0f, radius + 1.0f, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour (accent.withAlpha (enabled ? 0.22f : 0.06f));
    g.strokePath (active, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.setColour ((enabled ? accent : colours::textMuted).withAlpha (enabled ? (engaged ? 1.0f : 0.90f) : 0.55f));
    g.strokePath (active, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    auto knob = bounds.reduced (7.0f);
    juce::ColourGradient metal (juce::Colour (0xff384554), knob.getX(), knob.getY(),
                                juce::Colour (0xff0a1017), knob.getRight(), knob.getBottom(), false);
    metal.addColour (0.48, juce::Colour (0xff171f29));
    if (! enabled)
        metal.multiplyOpacity (0.62f);
    g.setGradientFill (metal);
    g.fillEllipse (knob);
    g.setColour (juce::Colours::white.withAlpha ((enabled ? 0.10f : 0.04f)));
    g.drawEllipse (knob.reduced (1.0f), 1.0f);

    juce::Path marker;
    marker.addRoundedRectangle (-1.1f, -radius * 0.48f, 2.2f, radius * 0.22f, 1.1f);
    g.setColour ((enabled ? juce::Colours::white : colours::textMuted).withAlpha (enabled ? 0.82f : 0.45f));
    g.fillPath (marker, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void LiveLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
        return;

    const bool engaged = slider.isMouseOverOrDragging();
    const float alpha = slider.isEnabled() ? 1.0f : 0.42f;
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    auto track = juce::Rectangle<float> (0.0f, area.getY(), 10.0f, area.getHeight()).withCentre (area.getCentre());

    g.setColour (juce::Colours::black.withAlpha (0.20f * alpha));
    g.fillRoundedRectangle (track.expanded (5.0f, 2.0f), 8.0f);
    g.setColour (colours::black.withAlpha (0.72f * alpha));
    g.fillRoundedRectangle (track, 5.0f);
    g.setColour ((engaged ? colours::cyan : colours::borderSoft).withAlpha (engaged ? 0.32f : 0.88f));
    g.drawRoundedRectangle (track, 5.0f, 1.0f);

    auto fill = track.withTop (sliderPos);
    if (fill.getHeight() > 0.5f)
    {
        juce::ColourGradient grad (colours::cyan, fill.getCentreX(), fill.getBottom(),
                                   colours::blue, fill.getCentreX(), fill.getY(), false);
        grad.multiplyOpacity (alpha);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 5.0f);
    }

    auto shadow = juce::Rectangle<float> (track.getX() - 15.0f, sliderPos - 12.0f,
                                          track.getWidth() + 30.0f, 27.0f);
    g.setColour (juce::Colours::black.withAlpha (0.34f * alpha));
    g.fillRoundedRectangle (shadow.translated (0.0f, 2.0f), 7.0f);

    auto thumb = shadow.withTrimmedBottom (3.0f);
    juce::ColourGradient thumbGrad (engaged ? juce::Colour (0xfff0f5fa) : juce::Colour (0xffdfe6ee),
                                    thumb.getCentreX(), thumb.getY(),
                                    juce::Colour (0xff8c98a6), thumb.getCentreX(), thumb.getBottom(), false);
    thumbGrad.addColour (0.32, juce::Colour (0xffcbd4de));
    thumbGrad.addColour (0.68, juce::Colour (0xffaab5c1));
    thumbGrad.multiplyOpacity (alpha);
    g.setGradientFill (thumbGrad);
    g.fillRoundedRectangle (thumb, 6.0f);

    g.setColour (juce::Colours::white.withAlpha (0.70f * alpha));
    g.drawRoundedRectangle (thumb.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f * alpha));
    g.drawRoundedRectangle (thumb.reduced (2.0f), 4.5f, 1.0f);

    const float grooveY = thumb.getCentreY();
    g.setColour (juce::Colours::black.withAlpha (0.28f * alpha));
    g.drawLine (thumb.getX() + 9.0f, grooveY, thumb.getRight() - 9.0f, grooveY, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (0.36f * alpha));
    g.drawLine (thumb.getX() + 9.0f, grooveY - 1.2f, thumb.getRight() - 9.0f, grooveY - 1.2f, 0.8f);

    g.setColour (juce::Colour (0xff66717f).withAlpha (0.65f * alpha));
    g.fillEllipse (thumb.getX() + 5.0f, thumb.getCentreY() - 1.5f, 3.0f, 3.0f);
    g.fillEllipse (thumb.getRight() - 8.0f, thumb.getCentreY() - 1.5f, 3.0f, 3.0f);
}

void LiveLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool toggled = button.getToggleState();
    const float hover = highlighted ? 1.0f : 0.0f;
    const float press = down ? 1.0f : 0.0f;

    g.setColour (juce::Colours::black.withAlpha (0.12f + press * 0.04f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 8.0f);

    juce::ColourGradient surface ((toggled ? colours::panelHi.brighter (0.03f) : colours::panelHi.brighter (0.016f)),
                                  bounds.getX(), bounds.getY(),
                                  colours::panelLo, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto accent = toggled ? colours::cyan : colours::textMuted;
    g.setColour (accent.withAlpha (0.05f + hover * 0.06f + press * 0.04f));
    g.fillRoundedRectangle (bounds.reduced (0.5f), 8.0f);

    g.setColour (juce::Colours::white.withAlpha (0.020f + hover * 0.018f));
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 10.0f, bounds.getY() + 1.5f,
                                                    juce::jmax (0.0f, bounds.getWidth() - 20.0f), 1.4f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 7.0f, 1.0f);
    g.setColour ((toggled ? colours::cyan : colours::border).withAlpha (0.18f + hover * 0.16f + press * 0.06f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
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
    const bool hover = box.isMouseOverOrDragging();
    juce::ColourGradient surface (colours::panelHi.brighter (hover ? 0.022f : 0.010f),
                                  bounds.getX(), bounds.getY(),
                                  colours::panelLo, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (colours::cyan.withAlpha (hover ? 0.045f : 0.02f));
    g.fillRoundedRectangle (bounds.reduced (0.5f), 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.018f + (hover ? 0.016f : 0.0f)));
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 10.0f, bounds.getY() + 1.5f,
                                                    juce::jmax (0.0f, bounds.getWidth() - 22.0f), 1.4f), 1.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::cyan.withAlpha (0.72f)
                                             : hover ? colours::border.withAlpha (0.96f)
                                                     : colours::borderSoft.withAlpha (0.88f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    const auto cx = static_cast<float> (width - 18);
    const auto cy = static_cast<float> (height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour ((hover ? colours::text : colours::textMuted).withAlpha (hover ? 0.86f : 0.72f));
    g.strokePath (chevron, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
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

    juce::ColourGradient track (on ? colours::cyan.withAlpha (0.34f) : colours::panelLo,
                                toggle.getX(), toggle.getY(),
                                on ? colours::cyan.withAlpha (0.18f) : colours::panelHi,
                                toggle.getRight(), toggle.getBottom(), false);
    g.setGradientFill (track);
    g.fillRoundedRectangle (toggle, 9.0f);
    g.setColour ((on ? colours::cyan : colours::border).withAlpha (0.85f + (highlighted ? 0.08f : 0.0f)));
    g.drawRoundedRectangle (toggle, 9.0f, 1.0f);

    const float diameter = 14.0f;
    const float x = on ? toggle.getRight() - diameter - 2.0f : toggle.getX() + 2.0f;
    auto thumb = juce::Rectangle<float> (x, toggle.getY() + 2.0f, diameter, diameter);
    juce::ColourGradient thumbFill (juce::Colour (0xffeef3f9), thumb.getX(), thumb.getY(),
                                    juce::Colour (0xffaeb9c5), thumb.getRight(), thumb.getBottom(), false);
    g.setGradientFill (thumbFill);
    g.fillEllipse (thumb);
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.drawEllipse (thumb, 1.0f);

    g.setFont (jakarta (12.5f, true));
    g.setColour ((highlighted ? colours::text : colours::textMuted).withAlpha (highlighted ? 1.0f : 0.90f));
    g.drawText (button.getButtonText(), area.reduced (2.0f, 0.0f),
                juce::Justification::centredLeft, false);
}

juce::Rectangle<int> LiveLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                            juce::Point<int> screenPos,
                                                            juce::Rectangle<int> parentArea)
{
    const bool structured = tipText.containsChar ('|');
    const auto body = structured ? tipText.fromFirstOccurrenceOf ("|", false, false).trim()
                                 : tipText.trim();
    const int preferredWidth = juce::jlimit (260, 360, 276 + body.length() * 2);
    const int lineCount = juce::jlimit (1, 4, 1 + body.length() / 46);
    const int preferredHeight = structured ? 58 + lineCount * 17
                                           : 34 + lineCount * 17;

    int x = screenPos.x + 14;
    int y = screenPos.y + 18;
    if (x + preferredWidth > parentArea.getRight())
        x = screenPos.x - preferredWidth - 14;
    if (y + preferredHeight > parentArea.getBottom())
        y = screenPos.y - preferredHeight - 16;

    return juce::Rectangle<int> (x, y, preferredWidth, preferredHeight)
        .constrainedWithin (parentArea.reduced (8));
}

void LiveLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                   int width, int height)
{
    // TooltipWindow may be backed by a native/opaque surface on Windows. Fill
    // every pixel first so rounded corners or a resize never expose the OS
    // default white background.
    g.fillAll (colours::panelLo);

    const bool structured = text.containsChar ('|');
    const auto title = structured ? text.upToFirstOccurrenceOf ("|", false, false).trim()
                                  : juce::String ("Info");
    const auto body = structured ? text.fromFirstOccurrenceOf ("|", false, false).trim()
                                 : text.trim();

    auto shell = juce::Rectangle<float> (0.5f, 0.5f,
                                         static_cast<float> (width - 1),
                                         static_cast<float> (height - 1));

    g.setColour (juce::Colours::black.withAlpha (0.13f));
    g.fillRoundedRectangle (shell.translated (0.0f, 1.0f), 10.0f);

    juce::ColourGradient background (colours::panelHi.brighter (0.025f), shell.getX(), shell.getY(),
                                     colours::panelLo, shell.getRight(), shell.getBottom(), false);
    background.addColour (0.55, colours::panel.withAlpha (0.99f));
    g.setGradientFill (background);
    g.fillRoundedRectangle (shell, 11.0f);

    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.drawRoundedRectangle (shell.reduced (1.5f), 9.5f, 1.0f);
    g.setColour (colours::borderSoft.withAlpha (0.96f));
    g.drawRoundedRectangle (shell.reduced (0.5f), 11.0f, 1.0f);

    auto content = shell.reduced (14.0f, 10.0f);
    auto titleArea = content.removeFromTop (18.0f);
    content.removeFromTop (5.0f);

    g.setColour (colours::cyan.withAlpha (0.92f));
    g.fillRoundedRectangle (juce::Rectangle<float> (titleArea.getX(), titleArea.getBottom() - 2.0f,
                                                    juce::jmin (28.0f, titleArea.getWidth()), 2.0f), 1.0f);
    g.setFont (jakarta (10.5f, true));
    g.drawFittedText (title.toUpperCase(), titleArea.toNearestInt(),
                      juce::Justification::centredLeft, 1);

    g.setColour (colours::text.withAlpha (0.96f));
    g.setFont (jakarta (12.0f));
    g.drawFittedText (body, content.toNearestInt(),
                      juce::Justification::topLeft, 4);
}

void fillPremiumPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                       float radius, bool selected)
{
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.0f), radius);

    juce::ColourGradient surface (selected ? colours::panelHi.brighter (0.018f) : colours::panelHi,
                                  bounds.getX(), bounds.getY(),
                                  colours::panelLo, bounds.getRight(), bounds.getBottom(), false);
    surface.addColour (0.62, colours::panel.withAlpha (0.99f));
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour ((selected ? colours::cyan : colours::borderSoft)
                     .withAlpha (selected ? 0.62f : 0.92f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.018f));
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + radius,
                                                    bounds.getY() + 1.0f,
                                                    juce::jmax (0.0f, bounds.getWidth() - radius * 2.0f),
                                                    1.0f), 0.5f);
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
