# P10 final visual authority, driven by the annotated 1280x720 runtime audit.
# Geometry and painting only: no DSP, routing, persistence, or audio callbacks.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P10 annotated refinement requires generated UI")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p10_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P10 anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p10_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS _start)
        message(FATAL_ERROR "SC220 P10 range changed: ${label}")
    endif()
    string(LENGTH "${end_marker}" _end_length)
    math(EXPR _tail_start "${_end} + ${_end_length}")
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_tail_start} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Remove the redundant section caption and render the real alpha product cutout
# directly rather than cropping a baked white rectangle.
sc220_p10_once(_ui "remove INPUT MIX caption"
[=[    g.setColour (palette::ink);
    g.setFont (interFont (13.5f, true));
    g.drawText ("INPUT MIX", inputArea.withHeight (50), juce::Justification::centred);
    g.setColour (palette::borderSoft);]=]
[=[    g.setColour (palette::borderSoft);]=])

sc220_p10_range(_ui "transparent SC220 product cutout"
    "    if (sc220Image.isValid())"
    "    const int sc220StatusY = sc220StripArea.getY() + 68;"
[=[    if (sc220Image.isValid())
    {
        g.setOpacity (sc220Detected ? 1.0f : 0.40f);
        g.drawImageWithin (sc220Image,
                           sc220StripArea.getCentreX() - 70,
                           sc220StripArea.getY() + 4,
                           140, 64,
                           juce::RectanglePlacement::centred, false);
        g.setOpacity (1.0f);
    }

    const int sc220StatusY = sc220StripArea.getY() + 72;]=])

# Remove duplicated channel identity around the faders; source identity is
# already explicit in the upper control bay.
sc220_p10_once(_ui "remove redundant fader labels"
[=[    if (audioEngine.getInputChannelMode() == 2)
    {
        g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                    sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    }
    else
    {
        g.drawText ("CH1  " + juce::String (sc220MonoLeftFader.getValue(), 1) + " dB",
                    sc220MonoLeftFader.getX() - 15, sc220MonoLeftFader.getBottom() + 2,
                    sc220MonoLeftFader.getWidth() + 30, 18, juce::Justification::centred);
        g.drawText ("CH2  " + juce::String (sc220MonoRightFader.getValue(), 1) + " dB",
                    sc220MonoRightFader.getX() - 15, sc220MonoRightFader.getBottom() + 2,
                    sc220MonoRightFader.getWidth() + 30, 18, juce::Justification::centred);
    }
    g.drawText ("PC AUDIO", pcFader.getX() - 20, pcFader.getBottom() + 2,
                pcFader.getWidth() + 40, 18, juce::Justification::centred);]=]
[=[]=])

# Rebuild the enhancer surface as one functional card below the analyzer.
sc220_p10_range(_ui "single enhancer control card"
    "void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)"
    "void SC220MainComponent::drawMasterSurface (juce::Graphics& g)"
[=[void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (
        palette::surfaceAlt.brighter (0.035f),
        static_cast<float> (signatureArea.getX()),
        static_cast<float> (signatureArea.getY()),
        palette::ivory,
        static_cast<float> (signatureArea.getRight()),
        static_cast<float> (signatureArea.getBottom()), false));
    g.fillRect (signatureArea);

    const auto enhancerControlBay = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getX() + 12),
        static_cast<float> (spectrumDisplay->getBottom() + 12),
        static_cast<float> (signatureArea.getWidth() - 24),
        static_cast<float> (signatureArea.getBottom() - spectrumDisplay->getBottom() - 20));
    g.setColour (palette::header.withAlpha (0.76f));
    g.fillRoundedRectangle (enhancerControlBay, 6.0f);
    g.setColour (palette::border.withAlpha (0.90f));
    g.drawRoundedRectangle (enhancerControlBay.reduced (0.5f), 5.5f, 1.0f);

    g.setColour (palette::ink);
    g.setFont (interFont (13.0f, true));
    g.drawText ("SMART ENHANCER",
                juce::roundToInt (enhancerControlBay.getX() + 16.0f),
                juce::roundToInt (enhancerControlBay.getY() + 10.0f),
                190, 20, juce::Justification::centredLeft);
    g.setColour (palette::muted);
    g.setFont (interFont (8.8f, true));
    g.drawText ("VOICE / MUSIC DSP",
                juce::roundToInt (enhancerControlBay.getX() + 16.0f),
                juce::roundToInt (enhancerControlBay.getY() + 29.0f),
                190, 16, juce::Justification::centredLeft);

    const auto macroConsoleBay = juce::Rectangle<float> (
        enhancerControlBay.getX() + 10.0f,
        static_cast<float> (presetBox.getBottom() + 12),
        enhancerControlBay.getWidth() - 20.0f,
        static_cast<float> (enhancerControlBay.getBottom() - presetBox.getBottom() - 54));
    g.setColour (palette::surface.withAlpha (0.88f));
    g.fillRoundedRectangle (macroConsoleBay, 5.0f);
    g.setColour (palette::borderSoft);
    g.drawRoundedRectangle (macroConsoleBay.reduced (0.5f), 4.5f, 1.0f);

    static constexpr const char* names[] = {
        "ENHANCE", "SMART BASS", "SMART TREBLE", "VOCAL BODY", "STEREO MAGIC"
    };
    for (size_t i = 0; i < macroKnobs.size(); ++i)
    {
        const auto& knob = macroKnobs[i];
        g.setColour (palette::ink);
        g.setFont (interFont (10.0f, true));
        g.drawText (names[i], knob.getX() - 16, knob.getY() - 28,
                    knob.getWidth() + 32, 18, juce::Justification::centred);
        g.setColour (palette::cyan);
        g.setFont (interFont (11.5f));
        g.drawText (juce::String (knob.getValue(), 0) + "%", knob.getX(), knob.getY() - 10,
                    knob.getWidth(), 20, juce::Justification::centred);
    }

}

void SC220MainComponent::drawMasterSurface (juce::Graphics& g)]=])

# Reclaim the input heading row, enlarge input controls, align every source
# selector, raise fader labels and MUTE controls, and space monitor modes.
sc220_p10_once(_ui "reclaimed input content"
[=[    auto inputContent = inputArea.reduced (14, 0);
    inputContent.removeFromTop (52);
    inputContent.removeFromBottom (40);]=]
[=[    auto inputContent = inputArea.reduced (14, 0);
    inputContent.removeFromTop (14);
    inputContent.removeFromBottom (40);]=])

sc220_p10_once(_ui "single integrated DSP control"
[=[    enhancerPowerButton.setIconColour (palette::orange);
    enhancerPowerButton.setOffColour (palette::muted);]=]
[=[    enhancerPowerButton.setIconColour (palette::orange);
    enhancerPowerButton.setOffColour (palette::muted);
    enhancerPowerButton.setEmbeddedLabels ("DSP ON", "DSP BYPASS",
                                            interFont (9.0f, true));]=])

sc220_p10_once(_ui "raised shared source row"
[=[    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (228, 270,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.370f));
    const int sharedFaderTop = sourceSelectorY + 86;
    const int sharedFaderBottom = inputArea.getBottom() - 60;]=]
[=[    const int sourceSelectorY = inputArea.getY() + 214;
    const int sharedFaderTop = sourceSelectorY + 76;
    const int sharedFaderBottom = inputArea.getBottom() - 64;]=])

sc220_p10_once(_ui "larger aligned input controls"
[=[    stereoButton.setBounds (sc220StripArea.getX() + 8, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 8, 28);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 8, 28);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 32,
                         sourceSelectorY - 66, 64, 64);

    const int noiseRowY = sourceSelectorY + 32;
    const int noiseWidth = (sc220StripArea.getWidth() - 22) / 2;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 8, noiseRowY,
                                noiseWidth, 22);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + 6, noiseRowY,
                                noiseWidth, 22);]=]
[=[    constexpr int inputButtonGap = 6;
    constexpr int inputButtonHeight = 26;
    const int inputButtonWidth = (sc220StripArea.getWidth() - 22) / 2;
    stereoButton.setBounds (sc220StripArea.getX() + 8, sourceSelectorY,
                            inputButtonWidth, inputButtonHeight);
    dualMonoButton.setBounds (stereoButton.getRight() + inputButtonGap,
                              sourceSelectorY, inputButtonWidth, inputButtonHeight);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 39,
                         sourceSelectorY - 84, 78, 78);

    const int noiseRowY = sourceSelectorY + inputButtonHeight + inputButtonGap;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 8, noiseRowY,
                                inputButtonWidth, inputButtonHeight);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + inputButtonGap,
                                noiseRowY, inputButtonWidth, inputButtonHeight);]=])

sc220_p10_once(_ui "raised aligned SC220 mute"
    "    sc220Mute.setBounds (sc220StripArea.getCentreX() - 34,\n                         inputArea.getBottom() - 38, 68, 28);"
    "    sc220Mute.setBounds (sc220StripArea.getCentreX() - 34,\n                         inputArea.getBottom() - 50, 68, 28);")

sc220_p10_once(_ui "larger duck knob"
[=[    duckAmount.setBounds (pcStripArea.getCentreX() - 32,
                          sourceSelectorY - 66, 64, 64);]=]
[=[    duckAmount.setBounds (pcStripArea.getCentreX() - 39,
                          sourceSelectorY - 84, 78, 78);]=])

# Remove the extra profile rail; the LEARN button and conditional profile text
# already communicate state without introducing an orphan grey shape.
sc220_p10_range(_ui "remove noise profile rail"
    "    auto progress = juce::Rectangle<float> ("
    "    if (noise.isLearning() || noise.hasProfile())"
[=[    if (noise.isLearning() || noise.hasProfile())]=])

sc220_p10_once(_ui "analyzer above unified enhancer card"
[=[    presetBox.setBounds (signatureArea.getCentreX() - 95, signatureArea.getY() + 62, 190, 32);
    enhancerPowerButton.setBounds (signatureArea.getRight() - 46, signatureArea.getY() + 16, 32, 32);
    spectrumDisplay->setBounds (signatureArea.reduced (26, 0).withTop (signatureArea.getY() + 108)
                                .withBottom (signatureArea.getY() + juce::roundToInt (signatureArea.getHeight() * 0.535f)));

    auto macroRow = signatureArea.reduced (20, 0);
    macroRow.setTop (spectrumDisplay->getBottom() + 42);
    macroRow.setBottom (signatureArea.getBottom() - 50);
    const int macroWidth = macroRow.getWidth() / static_cast<int> (macroKnobs.size());
    for (auto& knob : macroKnobs)
    {
        auto slot = macroRow.removeFromLeft (macroWidth);
        const int knobWidth = juce::jmin (78, slot.getWidth() - 10);
        const int knobHeight = juce::jmin (112, slot.getHeight());
        knob.setBounds (slot.withSizeKeepingCentre (knobWidth, knobHeight));
    }]=]
[=[    spectrumDisplay->setBounds (signatureArea.getX() + 14,
                                signatureArea.getY() + 12,
                                signatureArea.getWidth() - 28, 220);
    const int enhancerCardTop = spectrumDisplay->getBottom() + 12;
    presetBox.setBounds (signatureArea.getX() + 28, enhancerCardTop + 48, 206, 28);
    enhancerPowerButton.setBounds (signatureArea.getRight() - 142,
                                   enhancerCardTop + 48, 128, 28);

    auto macroRow = juce::Rectangle<int> (
        signatureArea.getX() + 20, enhancerCardTop + 92,
        signatureArea.getWidth() - 40,
        signatureArea.getBottom() - enhancerCardTop - 142);
    const int macroWidth = macroRow.getWidth() / static_cast<int> (macroKnobs.size());
    for (auto& knob : macroKnobs)
    {
        auto slot = macroRow.removeFromLeft (macroWidth);
        const int knobWidth = juce::jmin (88, slot.getWidth() - 8);
        const int knobHeight = juce::jmin (124, slot.getHeight());
        knob.setBounds (slot.withSizeKeepingCentre (knobWidth, knobHeight));
    }]=])

sc220_p10_once(_ui "raised aligned stream mute"
    "    streamMute.setBounds (streamColumn.getCentreX() - 34,\n                          masterArea.getBottom() - 38, 68, 28);"
    "    streamMute.setBounds (streamColumn.getCentreX() - 34,\n                          masterArea.getBottom() - 50, 68, 28);")

sc220_p10_once(_ui "balanced headphone control spacing"
[=[    monitorPowerButton.setBounds (monitorColumn.getCentreX() - 13,
                                  monitorOutputBox.getBottom() + 12, 26, 26);]=]
[=[    monitorPowerButton.setBounds (monitorColumn.getCentreX() - 13,
                                  monitorOutputBox.getBottom() + 18, 26, 26);]=])

sc220_p10_once(_ui "monitor knob follows headphone control"
[=[    monitorKnob.setBounds (monitorColumn.getCentreX() - 41,
                           sharedFaderTop - 8, 82, 92);]=]
[=[    monitorKnob.setBounds (monitorColumn.getCentreX() - 50,
                           monitorPowerButton.getBottom() + 26, 100, 124);]=])

sc220_p10_once(_ui "spaced monitor mode buttons"
[=[    const int modeY = monitorKnob.getBottom() + 10;
    const int modeWidth = selectorWidth / 2;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                    modeY, modeWidth, 28);
    musicCueButton.setBounds (programMonitorButton.getRight(), modeY,
                              selectorWidth - modeWidth, 28);]=]
[=[    const int modeY = monitorKnob.getBottom() + 28;
    constexpr int monitorModeGap = 6;
    const int modeWidth = (selectorWidth - monitorModeGap) / 2;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                    modeY, modeWidth, 28);
    musicCueButton.setBounds (programMonitorButton.getRight() + monitorModeGap,
                              modeY, selectorWidth - modeWidth - monitorModeGap, 28);]=])

# Centre all header controls and the status LED on the same optical baseline.
sc220_p10_once(_ui "vertically centred top-bar controls"
[=[    closeButton.setBounds (getWidth() - 42, 10, 32, 32);
    maximiseButton.setBounds (getWidth() - 78, 10, 32, 32);
    minimiseButton.setBounds (getWidth() - 114, 10, 32, 32);
    settingsButton.setBounds (getWidth() - 158, 10, 32, 32);
    goLiveButton.setBounds (getWidth() - 276, 8, 108, 40);]=]
[=[    const int headerControlY = headerArea.getCentreY() - 16;
    closeButton.setBounds (getWidth() - 42, headerControlY, 32, 32);
    maximiseButton.setBounds (getWidth() - 78, headerControlY, 32, 32);
    minimiseButton.setBounds (getWidth() - 114, headerControlY, 32, 32);
    settingsButton.setBounds (getWidth() - 158, headerControlY, 32, 32);
    goLiveButton.setBounds (getWidth() - 276, headerArea.getCentreY() - 20, 108, 40);]=])

sc220_p10_once(_ui "centred header status lamp"
    "    g.fillEllipse (746.0f, 29.0f, 8.0f, 8.0f);"
    "    g.fillEllipse (746.0f, static_cast<float> (headerArea.getCentreY() - 4), 8.0f, 8.0f);")

sc220_p10_once(_ui "centred header logo"
    "g.drawImageWithin (recordingTechLogo, headerArea.getX() + 14, headerArea.getY() + 13,"
    "g.drawImageWithin (recordingTechLogo, headerArea.getX() + 14, headerArea.getY() + 10,")

foreach(_required IN ITEMS
        "sc220_mkii_device_alpha_png"
        "g.drawImageWithin (sc220Image"
        "inputContent.removeFromTop (14)"
        "const int sourceSelectorY = inputArea.getY() + 214"
        "inputButtonHeight = 26"
        "sourceSelectorY - 84, 78, 78"
        "inputArea.getBottom() - 50, 68, 28"
        "const auto enhancerControlBay"
        "spectrumDisplay->getBottom() + 12"
        "signatureArea.getY() + 12"
        "enhancerCardTop + 48, 206, 28"
        "enhancerCardTop + 48, 128, 28"
        "setEmbeddedLabels (\"DSP ON\", \"DSP BYPASS\""
        "monitorOutputBox.getBottom() + 18, 26, 26"
        "monitorPowerButton.getBottom() + 26, 100, 124"
        "monitorKnob.getBottom() + 28"
        "monitorModeGap = 6"
        "headerArea.getCentreY() - 4")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P10 annotated invariant missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "\"INPUT MIX\""
        "sc220_mkii_device_png"
        "sc220Fader.getBottom() + 2"
        "pcFader.getBottom() + 2"
        "g.drawText (\"SC220\", sc220Fader"
        "g.drawText (\"PC AUDIO\", pcFader"
        "auto progress = juce::Rectangle<float>"
        "signatureArea.getY() + 108"
        "SMART PROTECT  ON"
        "const auto dspGroupBounds"
        "masterArea.getBottom() - 38, 68, 28")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P10 stale annotated defect remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P10: all 14 annotated console refinements applied")
