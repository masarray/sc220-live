# P13: Surface compositor for the free public digital mixer.
# UI-only. All P0-P12 device, plugin, licensing and signal-flow contracts are preserved.
if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P13 requires the effective UI")
endif()
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p13_range label begin_marker end_marker replacement)
    string(FIND "${_ui}" "${begin_marker}" _a)
    string(FIND "${_ui}" "${end_marker}" _b)
    if(_a EQUAL -1 OR _b EQUAL -1 OR _b LESS _a)
        message(FATAL_ERROR "SC220 P13 surface moved: ${label}")
    endif()
    string(SUBSTRING "${_ui}" 0 ${_a} _left)
    string(SUBSTRING "${_ui}" ${_b} -1 _right)
    set(_ui "${_left}${replacement}${_right}" PARENT_SCOPE)
endfunction()

# Console-like dual channel bays: quiet surfaces, thin division, precise
# meter + authentic SC220 fader + independent FX power/status controls.
sc220_p13_range("Voice and PC channel surfaces"
[=[void SC220MainComponent::drawInputSurface (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawInputSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (inputArea);
    g.setColour (palette::border);
    g.drawVerticalLine (inputArea.getRight() - 1,
                        static_cast<float> (inputArea.getY()),
                        static_cast<float> (inputArea.getBottom()));

    // One restrained channel strip per signal lane, not stacked dashboard cards.
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto strip = channel == 0 ? sc220StripArea : pcStripArea;
        const auto plate = strip.reduced (3, 4).toFloat();
        juce::ColourGradient chassis (palette::surfaceAlt, plate.getX(), plate.getY(),
                                       palette::surface, plate.getRight(), plate.getBottom(), false);
        chassis.addColour (0.56, palette::surfaceAlt.darker (0.035f));
        g.setGradientFill (chassis);
        g.fillRoundedRectangle (plate, 5.0f);
        g.setColour (palette::borderSoft);
        g.drawRoundedRectangle (plate.reduced (0.5f), 4.5f, 1.0f);
        g.setColour (channel == 0 ? palette::cyan : palette::amber);
        g.fillRoundedRectangle (plate.getX() + 9.0f, plate.getY() + 8.0f,
                                3.0f, 18.0f, 1.5f);
        g.setColour (palette::ink);
        g.setFont (interMediumFont (11.5f));
        g.drawText (channel == 0 ? "01   SC220 / VOICE" : "02   PC MUSIC",
                    strip.getX() + 20, strip.getY() + 8,
                    strip.getWidth() - 30, 22,
                    juce::Justification::centredLeft);
        g.setColour (palette::borderSoft);
        g.drawHorizontalLine (strip.getY() + 36,
                              static_cast<float> (strip.getX() + 11),
                              static_cast<float> (strip.getRight() - 11));
    }

    const bool connected = looksLikeSc220Input (lastStatus.inputDevice);
    if (sc220Image.isValid())
    {
        g.setOpacity (connected ? 0.9f : 0.32f);
        g.drawImageWithin (sc220Image,
                           sc220StripArea.getCentreX() - 60,
                           sc220StripArea.getY() + 41, 120, 52,
                           juce::RectanglePlacement::centred, false);
        g.setOpacity (1.0f);
    }
    g.setColour (connected ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (sc220StripArea.getX() + 18),
                   static_cast<float> (sc220StripArea.getY() + 102), 6.0f, 6.0f);
    g.setColour (palette::graphite);
    g.setFont (interFont (9.0f));
    g.drawFittedText (connected ? "SC220 INPUT READY" : "SC220 NOT DETECTED",
                      sc220StripArea.getX() + 28, sc220StripArea.getY() + 95,
                      sc220StripArea.getWidth() - 36, 19,
                      juce::Justification::centredLeft, 1);

    g.setColour (lastStatus.pcLoopbackRunning ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getX() + 16),
                   static_cast<float> (pcStripArea.getY() + 58), 6.0f, 6.0f);
    g.setColour (palette::graphite);
    g.drawFittedText (lastStatus.pcLoopbackRunning ? "WINDOWS LOOPBACK ACTIVE"
                                                     : "PC CAPTURE NOT READY",
                      pcStripArea.getX() + 28, pcStripArea.getY() + 50,
                      pcStripArea.getWidth() - 36, 20,
                      juce::Justification::centredLeft, 1);

    g.setColour (palette::muted);
    g.setFont (interMediumFont (9.3f));
    g.drawText ("INPUT TRIM", inputTrim.getX() - 18, inputTrim.getY() - 20,
                inputTrim.getWidth() + 36, 16, juce::Justification::centred);
    g.drawText ("AUTO DUCK", duckAmount.getX() - 18, duckAmount.getY() - 20,
                duckAmount.getWidth() + 36, 16, juce::Justification::centred);
    g.setColour (audioEngine.isDuckingEnabled() ? palette::green : palette::muted);
    g.setFont (interFont (8.7f));
    g.drawText ("SC220 KEY", duckAmount.getX() - 14, duckAmount.getBottom() + 2,
                duckAmount.getWidth() + 28, 15, juce::Justification::centred);

    if (audioEngine.getInputChannelMode() == 2)
        drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());

    // The MUTE and FX controls are real JUCE Buttons, placed by resized().
    // Paint only their passive strip chassis, not fake interaction surfaces.
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto rack = channelRackAreas[static_cast<size_t> (channel)];
        if (rack.isEmpty()) continue;
        auto plate = rack.toFloat();
        g.setColour (palette::screen.withAlpha (0.86f));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setColour (palette::border);
        g.drawRoundedRectangle (plate.reduced (0.5f), 3.5f, 1.0f);
        g.setColour (palette::cyan.withAlpha (0.95f));
        g.fillRect (rack.getX() + 6, rack.getY() + 11, 2, 11);
        g.setColour (palette::ink);
        g.setFont (interMediumFont (10.0f));
        g.drawText (channel == 0 ? "VOICE INSERTS" : "MUSIC INSERTS",
                    rack.getX() + 13, rack.getY() + 4,
                    rack.getWidth() - 20, 22, juce::Justification::centredLeft);
        g.setColour (palette::borderSoft);
        g.drawHorizontalLine (rack.getY() + 27,
                              static_cast<float> (rack.getX() + 6),
                              static_cast<float> (rack.getRight() - 6));
    }

    g.setColour (palette::cyan);
    g.setFont (interMediumFont (10.5f));
    if (audioEngine.getInputChannelMode() == 2)
        g.drawText (juce::String (sc220Fader.getValue(), 1) + " dB",
                    sc220Fader.getX() - 7, sc220Fader.getY() - 18,
                    sc220Fader.getWidth() + 14, 16, juce::Justification::centred);
    else
    {
        g.drawText (juce::String (sc220MonoLeftFader.getValue(), 1),
                    sc220MonoLeftFader.getX() - 2, sc220MonoLeftFader.getY() - 18,
                    sc220MonoLeftFader.getWidth() + 4, 16, juce::Justification::centred);
        g.drawText (juce::String (sc220MonoRightFader.getValue(), 1),
                    sc220MonoRightFader.getX() - 2, sc220MonoRightFader.getY() - 18,
                    sc220MonoRightFader.getWidth() + 4, 16, juce::Justification::centred);
    }
    g.drawText (juce::String (pcFader.getValue(), 1) + " dB",
                pcFader.getX() - 7, pcFader.getY() - 18,
                pcFader.getWidth() + 14, 16, juce::Justification::centred);
}

]=])

# The post-master analyzer remains live. Its lower tone controls form one
# concise bank rather than a second stack of ornamental panels.
sc220_p13_range("Signature tone and analyzer surface"
[=[void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawMasterSurface (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)
{
    g.setColour (palette::ivory);
    g.fillRect (signatureArea);
    g.setColour (palette::borderSoft);
    g.drawVerticalLine (signatureArea.getRight() - 1,
                        static_cast<float> (signatureArea.getY()),
                        static_cast<float> (signatureArea.getBottom()));
    const auto analyzer = spectrumDisplay->getBounds().toFloat().expanded (4.0f, 4.0f);
    g.setColour (palette::screen);
    g.fillRoundedRectangle (analyzer, 5.0f);
    g.setColour (palette::borderSoft);
    g.drawRoundedRectangle (analyzer, 5.0f, 1.0f);

    const auto card = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getX() + 12),
        static_cast<float> (spectrumDisplay->getBottom() + 12),
        static_cast<float> (signatureArea.getWidth() - 24),
        static_cast<float> (signatureArea.getBottom() - spectrumDisplay->getBottom() - 20));
    g.setColour (palette::surfaceAlt);
    g.fillRoundedRectangle (card, 5.0f);
    g.setColour (palette::border);
    g.drawRoundedRectangle (card, 5.0f, 1.0f);
    g.setColour (palette::cyan);
    g.fillRoundedRectangle (card.getX() + 12.0f, card.getY() + 14.0f,
                            3.0f, 16.0f, 1.5f);
    g.setColour (palette::ink);
    g.setFont (interMediumFont (13.0f));
    g.drawText ("SIGNATURE  /  TONE",
                juce::roundToInt (card.getX() + 23.0f),
                juce::roundToInt (card.getY() + 10.0f),
                juce::roundToInt (card.getWidth() - 34.0f), 24,
                juce::Justification::centredLeft);
    g.setColour (palette::muted);
    g.setFont (interFont (9.0f));
    g.drawText ("ARSONKUPIK ENGINE",
                juce::roundToInt (card.getX() + 23.0f),
                juce::roundToInt (card.getY() + 31.0f),
                juce::roundToInt (card.getWidth() - 34.0f), 16,
                juce::Justification::centredLeft);

    static constexpr const char* labels[] = {
        "ENHANCE", "BASS", "TREBLE", "BODY", "STEREO"
    };
    for (size_t i = 0; i < macroKnobs.size(); ++i)
    {
        const auto& knob = macroKnobs[i];
        g.setColour (palette::graphite);
        g.setFont (interMediumFont (9.4f));
        g.drawFittedText (labels[i],
                          knob.getX() - 6, knob.getY() - 25,
                          knob.getWidth() + 12, 16,
                          juce::Justification::centred, 1);
        g.setColour (palette::cyan);
        g.setFont (interMediumFont (10.0f));
        g.drawText (juce::String (knob.getValue(), 0) + "%",
                    knob.getX(), knob.getY() - 9,
                    knob.getWidth(), 17, juce::Justification::centred);
    }
}

]=])

# Keep Stream and Monitor visually independent; never imply that OBS, VB-CABLE
# or any particular physical device is mandatory routing.
sc220_p13_range("Master output buses"
[=[void SC220MainComponent::drawMasterSurface (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawFooter (juce::Graphics& g)]=]
[=[void SC220MainComponent::drawMasterSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (masterArea);
    g.setColour (palette::border);
    g.drawVerticalLine (masterArea.getX(), static_cast<float> (masterArea.getY()),
                        static_cast<float> (masterArea.getBottom()));
    g.setColour (palette::ink);
    g.setFont (interMediumFont (12.5f));
    g.drawText ("MASTER  /  OUTPUT", masterArea.getX() + 14,
                masterArea.getY() + 10, masterArea.getWidth() - 28, 23,
                juce::Justification::centredLeft);
    g.setColour (palette::muted);
    g.setFont (interFont (9.3f));
    g.drawText ("LIVE LOUDNESS", masterArea.getX() + 14,
                loudnessDisplay->getY() - 15, masterArea.getWidth() - 28, 18,
                juce::Justification::centred);
    g.setColour (palette::borderSoft);
    g.drawHorizontalLine (loudnessDisplay->getBottom() + 8,
                          static_cast<float> (masterArea.getX() + 12),
                          static_cast<float> (masterArea.getRight() - 12));

    const int divider = (outputBox.getRight() + monitorOutputBox.getX()) / 2;
    g.setColour (palette::borderSoft.withAlpha (0.95f));
    g.drawVerticalLine (divider,
                        static_cast<float> (outputBox.getY() - 23),
                        static_cast<float> (masterArea.getBottom() - 14));

    g.setColour (palette::ink);
    g.setFont (interMediumFont (10.6f));
    g.drawText ("STREAM", outputBox.getX(), outputBox.getY() - 23,
                outputBox.getWidth(), 18, juce::Justification::centred);
    g.drawText ("MONITOR", monitorOutputBox.getX(), monitorOutputBox.getY() - 23,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);

    drawFaderScale (g, streamFader.getBounds());
    g.setColour (palette::cyan);
    g.setFont (interMediumFont (10.6f));
    g.drawText (juce::String (streamFader.getValue(), 1) + " dB",
                streamFader.getX() - 4, streamFader.getY() - 18,
                streamFader.getWidth() + 8, 17, juce::Justification::centred);

    const bool monitorHealthy = lastStatus.monitorRunning && lastStatus.monitorError.isEmpty();
    g.setColour (monitorHealthy ? palette::green : palette::muted);
    g.fillEllipse (static_cast<float> (monitorOutputBox.getX() + 6),
                   static_cast<float> (musicCueButton.getBottom() + 8), 6.0f, 6.0f);
    g.setFont (interFont (9.0f));
    g.drawFittedText (monitorHealthy ? "MONITOR ACTIVE" : "MONITOR NOT READY",
                      monitorOutputBox.getX() + 16, musicCueButton.getBottom() + 2,
                      monitorOutputBox.getWidth() - 20, 18,
                      juce::Justification::centredLeft, 1);
}

]=])

# Verify actual physical UI primitives and enforce that no paid panel returns.
foreach(_required IN ITEMS "drawFaderScale (g, pcFader.getBounds())"
        "VOICE INSERTS" "MUSIC INSERTS" "MASTER  /  OUTPUT"
        "getChannelFxName (channel, slot)" "kSc220FaderThumbSvg")
    string(FIND "${_ui}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "SC220 P13 visual contract missing: ${_required}")
    endif()
endforeach()
string(FIND "${_ui}" "licensePanel" _paid_ui)
if(NOT _paid_ui EQUAL -1)
    message(FATAL_ERROR "SC220 P13 paid panel regressed into free preview")
endif()
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P13: dual digital strips, clean Signature/FX and independent Stream/Monitor")
