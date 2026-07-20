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
    const float alpha = isEnabled() ? 1.0f : 0.38f;
    g.setColour (colours::textMuted.withAlpha (0.95f * alpha));
    g.setFont (lf.jakarta (10.0f, true));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (18),
                juce::Justification::centred, false);

    const auto value = valueFormatter ? valueFormatter (knob.getValue())
                                      : juce::String (knob.getValue(), knob.getInterval() < 1.0 ? 1 : 0) + unit;
    g.setColour (colours::text.withAlpha (alpha));
    g.setFont (lf.jakarta (12.5f, true));
    g.drawText (value, getLocalBounds().removeFromBottom (21),
                juce::Justification::centred, false);
}

void StereoMeter::setLevels (float leftDb, float rightDb)
{
    auto updatePeakHold = [] (float level, float& hold, int& frames)
    {
        if (level >= hold)
        {
            hold = level;
            frames = 12; // approximately 500 ms at the 24 Hz UI cadence
        }
        else if (frames > 0)
        {
            --frames;
        }
        else
        {
            hold = juce::jmax (level, hold - 0.85f);
        }
    };

    updatePeakHold (leftDb, peakHoldL, peakHoldFramesL);
    updatePeakHold (rightDb, peakHoldR, peakHoldFramesR);

    if (std::abs (leftDb - levelL) < 0.18f
        && std::abs (rightDb - levelR) < 0.18f
        && peakHoldFramesL > 0 && peakHoldFramesR > 0)
        return;

    levelL = leftDb;
    levelR = rightDb;
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (compact ? 5.0f : 8.0f, 5.0f);
    const float labelHeight = compact ? 13.0f : 17.0f;
    auto meterArea = area.withTrimmedBottom (labelHeight);
    const float gap = compact ? 5.0f : 8.0f;
    const float barWidth = juce::jmin (compact ? 8.0f : 11.0f,
                                      (meterArea.getWidth() - gap) * 0.5f);
    auto left = meterArea.withWidth (barWidth).withX (meterArea.getCentreX() - gap * 0.5f - barWidth);
    auto right = meterArea.withWidth (barWidth).withX (meterArea.getCentreX() + gap * 0.5f);

    auto drawBar = [&] (juce::Rectangle<float> bar, float db, float peakHold)
    {
        g.setColour (colours::black.withAlpha (0.54f));
        g.fillRoundedRectangle (bar.expanded (1.5f, 1.0f), 4.0f);
        g.setColour (colours::borderSoft.withAlpha (0.82f));
        g.drawRoundedRectangle (bar.expanded (1.0f, 0.5f), 4.0f, 1.0f);

        auto fill = bar.withTop (dbToY (db, bar));
        if (fill.getHeight() > 0.5f)
        {
            juce::ColourGradient gradient (juce::Colour (0xffffb627), fill.getCentreX(), fill.getBottom(),
                                           juce::Colour (0xffff5964), fill.getCentreX(), fill.getY(), false);
            gradient.addColour (0.62, juce::Colour (0xffffb627));
            gradient.addColour (0.86, juce::Colour (0xffff7d39));
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (fill, 3.0f);
        }

        for (int i = 1; i < 7; ++i)
        {
            const auto y = bar.getY() + bar.getHeight() * static_cast<float> (i) / 7.0f;
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawHorizontalLine (static_cast<int> (y), bar.getX() + 1.0f, bar.getRight() - 1.0f);
        }

        const float peakY = dbToY (peakHold, bar);
        g.setColour ((peakHold > -3.0f ? colours::red : colours::amber).withAlpha (0.95f));
        g.fillRoundedRectangle (juce::Rectangle<float> (bar.getX() - 1.0f, peakY - 0.75f,
                                                        bar.getWidth() + 2.0f, 1.5f), 0.75f);
    };

    drawBar (left, levelL, peakHoldL);
    drawBar (right, levelR, peakHoldR);

    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (8.5f, true));
    g.setColour (colours::textMuted.withAlpha (0.86f));
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

void VolumeFader::setBypassed (bool value) noexcept
{
    if (bypassed == value)
        return;

    bypassed = value;
    const float alpha = value ? 0.46f : 1.0f;
    fader.setAlpha (alpha);
    meter.setAlpha (value ? 0.38f : 1.0f);
    repaint();
}

void VolumeFader::sliderValueChanged (juce::Slider*)
{
    repaint();
    if (onValueChanged)
        onValueChanged (static_cast<float> (fader.getValue()));
}

void VolumeFader::resized()
{
    auto area = getLocalBounds().reduced (5);
    area.removeFromTop (58);
    area.removeFromBottom (5);

    auto meterArea = area.removeFromRight (40);
    area.removeFromRight (5);
    meter.setBounds (meterArea);

    auto faderArea = area;
    faderArea.removeFromLeft (34); // scale and labels
    faderArea.removeFromRight (5);
    fader.setBounds (faderArea.reduced (8, 5));
}

void VolumeFader::paint (juce::Graphics& g)
{
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float stateAlpha = bypassed ? 0.48f : 1.0f;

    juce::ColourGradient chassis (colours::panelHi.brighter (0.018f), bounds.getX(), bounds.getY(),
                                  colours::panelLo.darker (0.08f), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (chassis);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (colours::borderSoft.withAlpha (0.94f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.022f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 9.0f, 1.0f);

    auto header = getLocalBounds().reduced (8, 5).removeFromTop (48);
    g.setColour (colours::textMuted.withAlpha (0.92f * stateAlpha));
    g.setFont (lf.jakarta (9.4f, true));
    g.drawText (label.toUpperCase(), header.removeFromTop (16), juce::Justification::centred, false);

    auto valuePill = header.withSizeKeepingCentre (82, 25);
    g.setColour ((bypassed ? colours::textMuted : colours::cyan).withAlpha (0.07f));
    g.fillRoundedRectangle (valuePill.toFloat(), 8.0f);
    g.setColour ((bypassed ? colours::border : colours::cyan).withAlpha (0.24f));
    g.drawRoundedRectangle (valuePill.toFloat().reduced (0.5f), 8.0f, 1.0f);
    g.setColour (colours::text.withAlpha (stateAlpha));
    g.setFont (lf.jakarta (11.5f, true));
    g.drawText (juce::String (fader.getValue(), 1) + " dB", valuePill,
                juce::Justification::centred, false);

    g.setColour (colours::borderSoft.withAlpha (0.62f));
    g.drawHorizontalLine (56, 9.0f, (float) getWidth() - 9.0f);

    const auto faderBounds = fader.getBounds().toFloat();
    auto trackBounds = faderBounds.reduced (2.0f);
    trackBounds = juce::Rectangle<float> (0.0f, trackBounds.getY(), 12.0f, trackBounds.getHeight())
                    .withCentre (trackBounds.getCentre());

    auto lane = juce::Rectangle<float> (trackBounds.getX() - 15.0f, trackBounds.getY() - 7.0f,
                                        trackBounds.getWidth() + 30.0f, trackBounds.getHeight() + 14.0f);
    g.setColour (colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (lane, 9.0f);
    g.setColour (colours::borderSoft.withAlpha (0.44f));
    g.drawRoundedRectangle (lane, 9.0f, 1.0f);

    const int ticks[] = { 6, 0, -6, -12, -18, -24, -36, -48, -60 };
    g.setFont (lf.jakarta (8.3f));
    for (const int tick : ticks)
    {
        const float proportion = static_cast<float> (fader.valueToProportionOfLength ((double) tick));
        const float y = trackBounds.getBottom() - proportion * trackBounds.getHeight();
        const bool unity = tick == 0;
        g.setColour ((unity ? colours::cyan : colours::borderSoft).withAlpha (unity ? 0.52f : 0.82f));
        g.drawHorizontalLine ((int) std::round (y), trackBounds.getX() - 9.0f,
                              trackBounds.getX() - (unity ? 1.0f : 3.0f));
        g.setColour ((unity ? colours::cyan : colours::textMuted).withAlpha ((unity ? 0.80f : 0.72f) * stateAlpha));
        g.drawText (juce::String (tick), 1, (int) y - 7,
                    juce::jmax (0, (int) trackBounds.getX() - 12), 14,
                    juce::Justification::centredRight, false);
    }

    if (bypassed)
    {
        auto pill = juce::Rectangle<int> (8, getHeight() - 25, juce::jmax (0, getWidth() - 16), 18);
        g.setColour (colours::textMuted.withAlpha (0.10f));
        g.fillRoundedRectangle (pill.toFloat(), 7.0f);
        g.setColour (colours::textMuted.withAlpha (0.72f));
        g.setFont (lf.jakarta (8.4f, true));
        g.drawText ("BYPASSED - MOVE FADER TO ACTIVATE", pill,
                    juce::Justification::centred, false);
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
    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    const float alpha = isEnabled() ? 1.0f : 0.35f;
    auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::textMuted.withAlpha (0.90f * alpha));
    g.setFont (lf.jakarta (8.8f, true));
    g.drawText (title, bounds.removeFromTop (14.0f).toNearestInt(),
                juce::Justification::centred, false);
    bounds.removeFromTop (5.0f);

    if (vertical)
    {
        auto readout = bounds.removeFromBottom (18.0f);
        bounds.removeFromBottom (4.0f);
        auto shell = bounds.reduced (3.0f, 0.0f);

        juce::ColourGradient surface (colours::panelHi.brighter (0.015f), shell.getX(), shell.getY(),
                                      colours::panelLo, shell.getRight(), shell.getBottom(), false);
        g.setGradientFill (surface);
        g.fillRoundedRectangle (shell, 9.0f);
        g.setColour (colours::borderSoft.withAlpha (0.90f * alpha));
        g.drawRoundedRectangle (shell.reduced (0.5f), 9.0f, 1.0f);

        const float trackWidth = 12.0f;
        auto track = juce::Rectangle<float> (trackWidth,
                                             shell.getHeight() - 20.0f)
                         .withPosition (shell.getX() + 10.0f, shell.getY() + 10.0f);
        g.setColour (colours::black.withAlpha (0.54f * alpha));
        g.fillRoundedRectangle (track, trackWidth * 0.5f);
        g.setColour (colours::border.withAlpha (0.82f * alpha));
        g.drawRoundedRectangle (track, trackWidth * 0.5f, 1.0f);

        const float amount = juce::jlimit (0.0f, 1.0f, reductionDb / 24.0f);
        const float markerY = track.getY() + track.getHeight() * amount;
        auto fill = track.withBottom (markerY);
        if (fill.getHeight() > 0.5f)
        {
            juce::ColourGradient gradient (colours::amber, fill.getCentreX(), fill.getY(),
                                           colours::red, fill.getCentreX(), track.getBottom(), false);
            gradient.multiplyOpacity (alpha);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (fill, trackWidth * 0.5f);
        }

        g.setColour ((amount > 0.72f ? colours::red : colours::amber).withAlpha (0.96f * alpha));
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX() - 2.0f, markerY - 1.0f,
                                                        track.getWidth() + 4.0f, 2.0f), 1.0f);

        const int ticks[] = { 0, 6, 12, 18, 24 };
        g.setFont (lf.jakarta (7.8f));
        for (const int tick : ticks)
        {
            const float y = track.getY() + track.getHeight() * (static_cast<float> (tick) / 24.0f);
            g.setColour (colours::borderSoft.withAlpha (0.84f * alpha));
            g.drawHorizontalLine ((int) std::round (y), track.getRight() + 4.0f, track.getRight() + 8.0f);
            g.setColour (colours::textMuted.withAlpha (0.72f * alpha));
            g.drawText (tick == 0 ? juce::String ("0") : juce::String ("-") + juce::String (tick),
                        juce::Rectangle<int> ((int) track.getRight() + 9, (int) y - 6, 25, 12),
                        juce::Justification::centredLeft, false);
        }

        g.setColour (colours::text.withAlpha (0.95f * alpha));
        g.setFont (lf.jakarta (9.3f, true));
        g.drawText (juce::String (-reductionDb, 1) + " dB", readout.toNearestInt(),
                    juce::Justification::centred, false);
        return;
    }

    auto shell = bounds.reduced (2.0f, 7.0f);
    g.setColour (colours::black.withAlpha (0.60f * alpha));
    g.fillRoundedRectangle (shell, 3.0f);
    g.setColour (colours::borderSoft.withAlpha (alpha));
    g.drawRoundedRectangle (shell, 3.0f, 1.0f);

    const float amount = juce::jlimit (0.0f, 1.0f, reductionDb / 24.0f);
    auto fill = shell.withWidth (shell.getWidth() * amount);
    juce::ColourGradient gradient (colours::amber, fill.getX(), fill.getCentreY(),
                                   colours::red, fill.getRight(), fill.getCentreY(), false);
    gradient.multiplyOpacity (alpha);
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
                                bool selected, bool locked, bool openOnly)
    : rowIndex (index), moduleName (name), bypassAllowed (canBypass),
      selectedState (selected), lockedState (locked), openOnlyState (openOnly),
      moduleIcon (createLucideIcon (iconResource, selected ? colours::cyan : colours::textMuted)),
      actionButton (std::make_unique<LucideIconButton> (
          openOnly ? "Open module" : "Power",
          openOnly ? "plus_svg" : "power_svg", colours::cyan))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (openOnlyState ? "External VST Slot|Open this rack slot to insert or plan an external VST stage."
                              : lockedState ? "Safety Limiter|This protection stage stays active and cannot be bypassed from here."
                                            : "Rack Module|Click anywhere on the row or the power icon to toggle bypass.");
    actionButton->setOffColour (colours::textMuted.withAlpha (0.75f));
    actionButton->setTooltip (openOnlyState ? "Add Slot|Open this external VST slot."
                                            : lockedState ? "Safety Limiter|This stage stays protected and cannot be bypassed."
                                                          : "Bypass|Toggle this stage on or off.");
    addAndMakeVisible (*actionButton);
    actionButton->setClickingTogglesState (! openOnlyState);
    actionButton->setToggleState (! bypassed, juce::dontSendNotification);
    actionButton->setEnabled (! lockedState && (bypassAllowed || openOnlyState));
    actionButton->onClick = [this]
    {
        if (lockedState)
            return;
        if (openOnlyState)
        {
            if (onOpen)
                onOpen();
            return;
        }
        if (! bypassAllowed)
            return;
        bypassed = ! actionButton->getToggleState();
        repaint();
        if (onBypassChanged)
            onBypassChanged (bypassed);
    };
}

void CompactRackRow::startAnimation()
{
    if (! isTimerRunning())
        startTimerHz (60);
}

void CompactRackRow::timerCallback()
{
    const auto stepTowards = [] (float current, float target)
    {
        return current + (target - current) * 0.18f;
    };

    const float hoverTarget = isMouseOver (true) ? 1.0f : 0.0f;
    const float pressTarget = isMouseButtonDown() ? 1.0f : 0.0f;
    hoverMix = stepTowards (hoverMix, hoverTarget);
    pressMix = stepTowards (pressMix, pressTarget);
    repaint();

    if (std::abs (hoverMix - hoverTarget) < 0.015f && std::abs (pressMix - pressTarget) < 0.015f)
    {
        hoverMix = hoverTarget;
        pressMix = pressTarget;
        stopTimer();
        repaint();
    }
}

void CompactRackRow::setBypassed (bool value)
{
    bypassed = value;
    if (actionButton != nullptr && ! openOnlyState)
        actionButton->setToggleState (! value, juce::dontSendNotification);
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
    if (actionButton != nullptr)
        actionButton->setEnabled (! lockedState && (bypassAllowed || openOnlyState));
    repaint();
}

void CompactRackRow::resized()
{
    if (actionButton != nullptr)
        actionButton->setBounds (getWidth() - 40, 5, 34, getHeight() - 10);
}

void CompactRackRow::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const bool hovered = isMouseOver (true);
    const bool active = ! bypassed && ! openOnlyState;
    const float hover = juce::jlimit (0.0f, 1.0f, hoverMix + (hovered ? 0.08f : 0.0f));
    const float press = juce::jlimit (0.0f, 1.0f, pressMix);

    juce::ColourGradient surface (selectedState ? colours::panelHi.brighter (0.030f + hover * 0.010f)
                                                : colours::panelLo.brighter (hover * 0.018f),
                                  bounds.getX(), bounds.getY(),
                                  colours::panelLo.darker (0.035f), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (surface);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.08f + press * 0.04f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 7.0f, 1.0f);
    g.setColour (selectedState ? colours::cyan.withAlpha (0.34f + hover * 0.12f)
                               : colours::borderSoft.withAlpha (0.82f + hover * 0.12f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    const auto railColour = openOnlyState ? colours::violet
                           : lockedState ? colours::green
                           : active ? colours::cyan : colours::textMuted;
    g.setColour (railColour.withAlpha (0.52f + hover * 0.28f));
    g.fillRoundedRectangle (juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getY() + 8.0f,
                                                    2.0f, bounds.getHeight() - 16.0f), 1.0f);

    auto& lf = static_cast<LiveLookAndFeel&> (getLookAndFeel());
    g.setFont (lf.jakarta (8.2f, true));
    g.setColour ((selectedState ? colours::cyan : colours::textMuted).withAlpha (bypassed ? 0.42f : 0.70f));
    g.drawText (juce::String (rowIndex).paddedLeft ('0', 2), 7, 4, 24, 18,
                juce::Justification::centredLeft, false);

    const int actionLeft = actionButton != nullptr ? actionButton->getX() : getWidth() - 40;
    auto nameArea = juce::Rectangle<int> (34, 5, juce::jmax (0, actionLeft - 39), 23);
    g.setColour ((selectedState ? colours::cyan : colours::text).withAlpha (bypassed ? 0.46f : 0.96f));
    g.setFont (lf.jakarta (10.5f, true));
    g.drawFittedText (moduleName, nameArea, juce::Justification::centredLeft, 1, 0.82f);

    const juce::String status = openOnlyState ? "ADD SLOT"
                               : lockedState ? "SAFE"
                               : bypassed ? "BYPASSED" : "ACTIVE";
    const auto statusColour = openOnlyState ? colours::violet
                            : lockedState ? colours::green
                            : bypassed ? colours::textMuted : colours::cyan;
    auto statusArea = juce::Rectangle<int> (34, 27, juce::jmax (0, actionLeft - 39), 14);
    g.setColour (statusColour.withAlpha (bypassed ? 0.58f : 0.82f));
    g.setFont (lf.jakarta (7.5f, true));
    g.drawText (status, statusArea, juce::Justification::centredLeft, false);
}

void CompactRackRow::toggleBypassFromUi()
{
    if (lockedState || openOnlyState || ! bypassAllowed)
        return;

    bypassed = ! bypassed;
    if (actionButton != nullptr)
        actionButton->setToggleState (! bypassed, juce::dontSendNotification);
    repaint();
    if (onBypassChanged)
        onBypassChanged (bypassed);
}

void CompactRackRow::mouseEnter (const juce::MouseEvent&)
{
    startAnimation();
}

void CompactRackRow::mouseExit (const juce::MouseEvent&)
{
    startAnimation();
}

void CompactRackRow::mouseDown (const juce::MouseEvent&)
{
    startAnimation();
}

void CompactRackRow::mouseUp (const juce::MouseEvent& event)
{
    startAnimation();
    if (actionButton != nullptr && actionButton->getBounds().contains (event.getPosition()))
        return;

    if (openOnlyState)
    {
        if (onOpen)
            onOpen();
        return;
    }

    toggleBypassFromUi();
}

void CompactRackRow::mouseDoubleClick (const juce::MouseEvent&)
{
    if (openOnlyState && onOpen)
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
