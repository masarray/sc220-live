#include "LiveWidgets.h"

#include <cmath>

namespace askp::live {
namespace {
float dbToY (float db, juce::Rectangle<float> bounds)
{
    const float normalized = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    return bounds.getBottom() - normalized * bounds.getHeight();
}

} // namespace

KnobControl::KnobControl (const juce::String& labelText, const juce::String& suffix,
                          juce::Colour accent, double min, double max, double step)
    : title (labelText), unit (suffix), accentColour (accent)
{
    addAndMakeVisible (knob);
    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange (min, max, step);
    knob.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    knob.setMouseDragSensitivity (170);
    knob.addListener (this);
}

KnobControl::~KnobControl()
{
    knob.removeListener (this);
}

void KnobControl::setValue (double value, juce::NotificationType notification)
{
    knob.setValue (value, notification);
    repaint();
}

void KnobControl::setFormatter (std::function<juce::String(double)> formatter)
{
    valueFormatter = std::move (formatter);
    repaint();
}

void KnobControl::sliderValueChanged (juce::Slider*)
{
    repaint();
}

void KnobControl::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (20);
    area.removeFromBottom (23);

    const int diameter = juce::jlimit (54, 84, juce::jmin (area.getWidth(), area.getHeight()));
    knob.setBounds (juce::Rectangle<int> (diameter, diameter).withCentre (area.getCentre()));
}

void KnobControl::paint (juce::Graphics& g)
{
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setColour (colours::textMuted);
    g.setFont (lf.jakarta (10.0f, true));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (18),
                juce::Justification::centred, false);

    const auto value = valueFormatter ? valueFormatter (knob.getValue())
                                      : juce::String (knob.getValue(), knob.getInterval() < 1.0 ? 1 : 0) + unit;
    g.setColour (colours::text);
    g.setFont (lf.jakarta (12.5f, true));
    g.drawText (value, getLocalBounds().removeFromBottom (21),
                juce::Justification::centred, false);
}

void StereoMeter::setLevels (float leftDb, float rightDb)
{
    if (std::abs (leftDb - levelL) < 0.28f && std::abs (rightDb - levelR) < 0.28f)
        return;
    levelL = leftDb;
    levelR = rightDb;
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (compact ? 4.0f : 8.0f, 5.0f);
    const float labelHeight = compact ? 12.0f : 17.0f;
    auto meterArea = area.withTrimmedBottom (labelHeight);
    const float gap = compact ? 4.0f : 7.0f;
    const float barWidth = juce::jmin (compact ? 8.0f : 11.0f,
                                      (meterArea.getWidth() - gap) * 0.5f);
    auto left = meterArea.withWidth (barWidth).withX (meterArea.getCentreX() - gap * 0.5f - barWidth);
    auto right = meterArea.withWidth (barWidth).withX (meterArea.getCentreX() + gap * 0.5f);

    auto drawBar = [&] (juce::Rectangle<float> bar, float db)
    {
        g.setColour (colours::black.withAlpha (0.72f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (colours::borderSoft);
        g.drawRoundedRectangle (bar, 2.0f, 1.0f);

        auto fill = bar.withTop (dbToY (db, bar));
        juce::ColourGradient gradient (colours::amber, fill.getX(), fill.getBottom(),
                                       juce::Colour (0xffffd53d), fill.getX(), fill.getY(), false);
        gradient.addColour (0.70, colours::amber);
        gradient.addColour (0.91, colours::red);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (fill, 1.5f);

        for (int i = 1; i < 11; ++i)
        {
            const auto y = bar.getY() + bar.getHeight() * static_cast<float> (i) / 11.0f;
            g.setColour (colours::black.withAlpha (0.58f));
            g.drawHorizontalLine (static_cast<int> (y), bar.getX(), bar.getRight());
        }
    };

    drawBar (left, levelL);
    drawBar (right, levelR);

    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (9.0f, true));
    g.setColour (colours::textMuted);
    const auto labels = area.removeFromBottom (labelHeight);
    g.drawText ("L", labels.withWidth (labels.getWidth() * 0.5f), juce::Justification::centred, false);
    g.drawText ("R", labels.withTrimmedLeft (labels.getWidth() * 0.5f), juce::Justification::centred, false);
}

VolumeFader::VolumeFader (const juce::String& labelText)
    : label (labelText)
{
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (-60.0, 6.0, 0.1);
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setSkewFactorFromMidPoint (-12.0);
    fader.addListener (this);
    meter.setCompact (true);
}

VolumeFader::~VolumeFader()
{
    fader.removeListener (this);
}

void VolumeFader::setValue (float valueDb, juce::NotificationType notification)
{
    fader.setValue (juce::jlimit (-60.0f, 6.0f, valueDb), notification);
    repaint();
}

void VolumeFader::setLevels (float leftDb, float rightDb)
{
    meter.setLevels (leftDb, rightDb);
}

void VolumeFader::sliderValueChanged (juce::Slider*)
{
    repaint();
    if (onValueChanged)
        onValueChanged (static_cast<float> (fader.getValue()));
}

void VolumeFader::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (37);
    auto meterArea = area.removeFromRight (68);
    meter.setBounds (meterArea.reduced (2, 0));
    fader.setBounds (area.reduced (22, 5));
}

void VolumeFader::paint (juce::Graphics& g)
{
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    auto bounds = getLocalBounds();

    g.setColour (colours::textMuted);
    g.setFont (lf.jakarta (9.5f, true));
    g.drawText (label.toUpperCase(), bounds.removeFromTop (15), juce::Justification::centred, false);

    g.setColour (colours::text);
    g.setFont (lf.jakarta (12.0f, true));
    g.drawText (juce::String (fader.getValue(), 1) + " dB", 0, 16, getWidth(), 20,
                juce::Justification::centred, false);

    const auto faderBounds = fader.getBounds().toFloat();
    const int ticks[] = { 6, 0, -6, -12, -18, -24, -36, -48, -60 };
    g.setFont (lf.jakarta (8.5f));
    for (const int tick : ticks)
    {
        const float norm = static_cast<float> (tick + 60) / 66.0f;
        const float y = faderBounds.getBottom() - norm * faderBounds.getHeight();
        g.setColour (colours::borderSoft);
        g.drawHorizontalLine (static_cast<int> (y), faderBounds.getX() - 4.0f,
                              faderBounds.getX() + 3.0f);
        g.setColour (colours::textMuted.withAlpha (0.75f));
        g.drawText (juce::String (tick), 0, static_cast<int> (y - 7.0f),
                    juce::jmax (0, fader.getX() - 5), 14,
                    juce::Justification::centredRight, false);
    }
}

void GainReductionMeter::setReduction (float db) noexcept
{
    if (std::abs (db - reductionDb) < 0.12f)
        return;
    reductionDb = db;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f, 7.0f);
    g.setColour (colours::black.withAlpha (0.60f));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (colours::borderSoft);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    const float amount = juce::jlimit (0.0f, 1.0f, reductionDb / 24.0f);
    auto fill = bounds.withWidth (bounds.getWidth() * amount);
    juce::ColourGradient gradient (colours::amber, fill.getX(), fill.getCentreY(),
                                   colours::red, fill.getRight(), fill.getCentreY(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (fill, 2.0f);
}

void StatusPill::setStatus (const juce::String& text, juce::Colour colour, bool active)
{
    if (statusText == text && statusColour == colour && statusActive == active)
        return;
    statusText = text;
    statusColour = colour;
    statusActive = active;
    repaint();
}

void StatusPill::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour ((statusActive ? statusColour : colours::textMuted).withAlpha (0.08f));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour ((statusActive ? statusColour : colours::border).withAlpha (0.85f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
    g.setColour (statusActive ? statusColour : colours::textMuted);
    g.fillEllipse (10.0f, bounds.getCentreY() - 3.5f, 7.0f, 7.0f);
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (12.0f, true));
    g.drawText (statusText, 24, 0, getWidth() - 31, getHeight(),
                juce::Justification::centredLeft, false);
}

CompactRackRow::CompactRackRow (int index, const juce::String& iconResource,
                                const juce::String& name, bool canBypass,
                                bool selected, bool locked)
    : rowIndex (index), moduleName (name), bypassAllowed (canBypass),
      selectedState (selected), lockedState (locked),
      moduleIcon (createLucideIcon (iconResource, selected ? colours::cyan : colours::textMuted))
{
    addAndMakeVisible (power);
    power.setClickingTogglesState (true);
    power.setToggleState (true, juce::dontSendNotification);
    power.setEnabled (canBypass && ! locked);
    power.onClick = [this]
    {
        if (! bypassAllowed || lockedState)
            return;
        bypassed = ! power.getToggleState();
        repaint();
        if (onBypassChanged)
            onBypassChanged (bypassed);
    };
}

void CompactRackRow::setBypassed (bool value)
{
    bypassed = value;
    power.setToggleState (! value, juce::dontSendNotification);
    repaint();
}

void CompactRackRow::setSelected (bool value)
{
    selectedState = value;
    repaint();
}

void CompactRackRow::setLocked (bool value)
{
    lockedState = value;
    power.setEnabled (bypassAllowed && ! lockedState);
    repaint();
}

void CompactRackRow::resized()
{
    power.setBounds (getWidth() - 40, 5, 34, getHeight() - 10);
}

void CompactRackRow::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (selectedState ? colours::cyan.withAlpha (0.08f)
                               : colours::panelLo.withAlpha (0.88f));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (selectedState ? colours::cyan.withAlpha (0.38f) : colours::borderSoft);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (10.5f, true));
    g.setColour (selectedState ? colours::cyan : colours::textMuted);
    g.drawText (juce::String (rowIndex), 8, 0, 24, getHeight(), juce::Justification::centred, false);

    if (moduleIcon != nullptr)
        moduleIcon->drawWithin (g, juce::Rectangle<float> (34.0f, 10.0f, 18.0f, 18.0f),
                                juce::RectanglePlacement::centred, 0.92f);

    g.setColour (selectedState ? colours::cyan : colours::text);
    g.setFont (lf.jakarta (11.5f, true));
    g.drawFittedText (moduleName, 60, 0, juce::jmax (0, getWidth() - 104), getHeight(),
                      juce::Justification::centredLeft, 1);

    if (lockedState)
    {
        g.setColour (colours::textMuted.withAlpha (0.72f));
        g.setFont (lf.jakarta (8.0f, true));
        g.drawText ("LOCKED", getWidth() - 91, 0, 48, getHeight(),
                    juce::Justification::centredRight, false);
    }
}

void CompactRackRow::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onOpen)
        onOpen();
}

MiniStat::MiniStat (const juce::String& labelTextToUse, const juce::String& iconResource)
    : labelText (labelTextToUse)
{
    if (iconResource.isNotEmpty())
        icon = createLucideIcon (iconResource, colours::textMuted);
}

void MiniStat::setValue (const juce::String& value, juce::Colour colour)
{
    if (valueText == value && valueColour == colour)
        return;
    valueText = value;
    valueColour = colour;
    repaint();
}

void MiniStat::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    if (icon != nullptr)
    {
        icon->drawWithin (g, area.removeFromLeft (18.0f).reduced (1.0f),
                          juce::RectanglePlacement::centred, 0.72f);
        area.removeFromLeft (6.0f);
    }
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (10.0f, true));
    g.setColour (colours::textMuted);
    auto labelArea = area.removeFromLeft (juce::jmin (95.0f, area.getWidth() * 0.58f));
    g.drawText (labelText.toUpperCase(), labelArea, juce::Justification::centredLeft, false);
    g.setFont (lf.jakarta (11.5f, true));
    g.setColour (valueColour);
    g.drawText (valueText, area, juce::Justification::centredLeft, false);
}

} // namespace askp::live
