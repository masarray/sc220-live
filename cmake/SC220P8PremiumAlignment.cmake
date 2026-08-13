# P8 presentation authority. Rebuild the compact console's upper information
# rows around explicit baselines, remove low-value diagnostic copy, and give the
# loudness/output area its own stable grid. DSP and routing remain untouched.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P8 premium alignment requires the generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p8_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P8 alignment anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p8_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    if(_start EQUAL -1)
        message(FATAL_ERROR "SC220 P8 range start changed: ${label}")
    endif()
    string(FIND "${_value}" "${end_marker}" _end)
    if(_end EQUAL -1 OR _end LESS _start)
        message(FATAL_ERROR "SC220 P8 range end changed: ${label}")
    endif()
    string(LENGTH "${end_marker}" _end_length)
    math(EXPR _tail_start "${_end} + ${_end_length}")
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_tail_start} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Keep the hardware illustration, status, label and knob in four non-overlapping
# rows. Device endpoint details remain available in Settings, not on the mixer.
sc220_p8_once(_ui "smaller SC220 product crop"
    "                     sc220StripArea.getCentreX() - 72, sc220StripArea.getY() + 6, 144, 82,"
    "                     sc220StripArea.getCentreX() - 63, sc220StripArea.getY() + 2, 126, 70,")

sc220_p8_range(_ui "quiet SC220 status row"
    "    g.setColour (sc220Detected ? palette::green : palette::red);"
    "    static constexpr char kSc220PcAudioHeadphonesSvg[]"
[=[    const int sc220StatusY = sc220StripArea.getY() + 68;
    g.setColour (sc220Detected ? palette::green : palette::red);
    g.fillEllipse (static_cast<float> (sc220StripArea.getCentreX() - 72),
                   static_cast<float> (sc220StatusY + 5), 7.0f, 7.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.0f, true));
    g.drawFittedText (sc220Detected ? "SC220 CONNECTED" : "SC220 NOT DETECTED",
                      sc220StripArea.getCentreX() - 58, sc220StatusY,
                      132, 18, juce::Justification::centredLeft, 1);

    static constexpr char kSc220PcAudioHeadphonesSvg[]]=])

sc220_p8_once(_ui "aligned PC icon"
[=[            g, juce::Rectangle<float> (static_cast<float> (pcStripArea.getCentreX() - 15),
                                       static_cast<float> (pcStripArea.getY() + 4), 30.0f, 30.0f),]=]
[=[            g, juce::Rectangle<float> (static_cast<float> (pcStripArea.getCentreX() - 14),
                                       static_cast<float> (pcStripArea.getY() + 4), 28.0f, 28.0f),]=])
sc220_p8_once(_ui "aligned PC heading"
[=[    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 37,
                pcStripArea.getWidth(), 24, juce::Justification::centred);]=]
[=[    g.setFont (interFont (10.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 36,
                pcStripArea.getWidth(), 22, juce::Justification::centred);]=])

sc220_p8_range(_ui "quiet PC status row"
    "    const bool pcEnabled = audioEngine.isPcAudioEnabled();"
    "    const auto& noise = audioEngine.getNoiseClean();"
[=[    const bool pcEnabled = audioEngine.isPcAudioEnabled();
    const bool pcReady = pcEnabled && lastStatus.pcLoopbackRunning
                      && ! lastStatus.pcAudioLoopBlocked
                      && lastStatus.pcAudioError.isEmpty();
    g.setColour (! pcEnabled ? palette::muted
                 : pcReady ? palette::green
                 : lastStatus.pcAudioLoopBlocked ? palette::red : palette::amber);
    const int pcStatusY = pcStripArea.getY() + 68;
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 72),
                   static_cast<float> (pcStatusY + 5), 7.0f, 7.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.0f, true));
    const auto pcStateText = ! pcEnabled ? "PC AUDIO OFF"
                           : pcReady ? "CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : "CAPTURE STARTING";
    g.drawFittedText (pcStateText,
                      pcStripArea.getCentreX() - 58, pcStatusY,
                      132, 18, juce::Justification::centredLeft, 1);

    const auto& noise = audioEngine.getNoiseClean();]=])

sc220_p8_range(_ui "conditional noise profile status"
    "    if (learned > 0.0f)"
    [=[    if (audioEngine.getInputChannelMode() == 2)
        drawFaderScale (g, sc220Fader.getBounds());]=]
[=[    if (learned > 0.0f)
    {
        g.setColour (noise.isLearning() ? palette::orange : palette::green);
        g.fillRoundedRectangle (progress.withWidth (
            progress.getWidth() * juce::jlimit (0.0f, 1.0f, learned)), 3.5f);
    }
    if (noise.isLearning() || noise.hasProfile())
    {
        g.setColour (noise.isLearning() ? palette::orange : palette::green);
        g.setFont (interFont (8.0f, true));
        const auto profileText = noise.isLearning()
            ? "LEARNING  " + juce::String (juce::roundToInt (noise.getLearningProgress() * 100.0f)) + "%"
            : "PROFILE  " + juce::String (noise.getEstimatedNoiseFloorDb(), 1) + " dB";
        g.drawFittedText (profileText,
                          noiseCleanButton.getX(), noiseCleanButton.getBottom() + 10,
                          learnNoiseButton.getRight() - noiseCleanButton.getX(), 12,
                          juce::Justification::centred, 1);
    }

    if (audioEngine.getInputChannelMode() == 2)
        drawFaderScale (g, sc220Fader.getBounds());]=])

sc220_p8_once(_ui "concise input gain label" "\"INPUT TRIM\"" "\"INPUT GAIN\"")
sc220_p8_once(_ui "concise duck label"
[=[    g.drawText ("AUTO DUCK  /  SC220 KEY",
                pcStripArea.getX() + 8, duckAmount.getY() - 23,
                pcStripArea.getWidth() - 16, 18,
                juce::Justification::centred);]=]
[=[    g.drawText ("AUTO DUCK", duckAmount.getX() - 12, duckAmount.getY() - 22,
                duckAmount.getWidth() + 24, 18,
                juce::Justification::centred);]=])

# The loudness dial owns the upper region. Stream and Monitor then share an
# explicit two-column header/selector baseline below it, with the headphone
# toggle centred in the Monitor column rather than floating beside the dial.
sc220_p8_range(_ui "premium output surface"
    "void SC220MainComponent::drawMasterSurface (juce::Graphics& g)"
    "void SC220MainComponent::drawFooter (juce::Graphics& g)"
[=[void SC220MainComponent::drawMasterSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (masterArea);
    g.setColour (palette::border);
    g.drawVerticalLine (masterArea.getX(), static_cast<float> (masterArea.getY()),
                        static_cast<float> (masterArea.getBottom()));

    g.setColour (palette::ink);
    g.setFont (interFont (10.5f, true));
    g.drawText ("LOUDNESS", masterArea.getX(), masterArea.getY() + 12,
                masterArea.getWidth(), 18, juce::Justification::centred);
    g.drawText ("STREAM OUTPUT", outputBox.getX(), outputBox.getY() - 27,
                outputBox.getWidth(), 18, juce::Justification::centred);
    g.drawText ("MONITOR OUTPUT", monitorOutputBox.getX(), monitorOutputBox.getY() - 27,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);

    g.setColour (palette::borderSoft);
    const int outputDividerY = outputBox.getY() - 39;
    g.drawHorizontalLine (outputDividerY,
                          static_cast<float> (masterArea.getX() + 18),
                          static_cast<float> (masterArea.getRight() - 18));
    const int columnDividerX = (outputBox.getRight() + monitorOutputBox.getX()) / 2;
    g.drawVerticalLine (columnDividerX,
                        static_cast<float> (outputBox.getY() - 29),
                        static_cast<float> (masterArea.getBottom() - 18));

    drawFaderScale (g, streamFader.getBounds());
    const bool monitorHealthy = lastStatus.monitorRunning
                             && lastStatus.monitorError.isEmpty();
    g.setColour (monitorHealthy ? palette::green : palette::muted);
    g.setFont (interFont (8.8f, true));
    g.drawText (monitorHealthy ? "MONITOR ACTIVE" : "SELECT OUTPUT",
                monitorOutputBox.getX(), musicCueButton.getBottom() + 8,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);
}

void SC220MainComponent::drawFooter (juce::Graphics& g)]=])

sc220_p8_once(_ui "structured input content area"
    "    auto inputContent = inputArea.reduced (14, 40);"
[=[    auto inputContent = inputArea.reduced (14, 0);
    inputContent.removeFromTop (52);
    inputContent.removeFromBottom (40);]=])

sc220_p8_once(_ui "better output-area proportion"
[=[    const int inputWidth = juce::roundToInt (area.getWidth() * 0.320f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.430f);]=]
[=[    const int inputWidth = juce::roundToInt (area.getWidth() * 0.310f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.420f);]=])

sc220_p8_once(_ui "collision-free shared grid"
[=[    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (214, 258,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.355f));
    const int sharedFaderTop = sourceSelectorY + 70;]=]
[=[    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (228, 270,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.370f));
    const int sharedFaderTop = sourceSelectorY + 86;]=])

sc220_p8_once(_ui "aligned input controls"
[=[    stereoButton.setBounds (sc220StripArea.getX() + 8, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 8, 28);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 8, 28);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 38,
                         sourceSelectorY - 84, 76, 76);

    const int noiseRowY = sourceSelectorY + 31;
    const int noiseWidth = (sc220StripArea.getWidth() - 22) / 2;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 8, noiseRowY,
                                noiseWidth, 20);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + 6, noiseRowY,
                                noiseWidth, 20);]=]
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
                                noiseWidth, 22);]=])

sc220_p8_once(_ui "aligned duck control"
[=[    duckAmount.setBounds (pcStripArea.getCentreX() - 36,
                          sourceSelectorY - 84, 72, 76);]=]
[=[    duckAmount.setBounds (pcStripArea.getCentreX() - 32,
                          sourceSelectorY - 66, 64, 64);]=])

sc220_p8_range(_ui "balanced output control grid"
    "    loudnessDisplay->setBounds (masterArea.getCentreX() - 78,"
    "    licensePanel->setBounds (getLocalBounds());"
[=[    loudnessDisplay->setBounds (masterArea.getCentreX() - 68,
                                masterArea.getY() + 34, 136, 136);
    auto masterLower = masterArea.reduced (14, 0);
    constexpr int masterColumnGap = 14;
    const int masterColumnWidth = juce::jmax (112,
        (masterLower.getWidth() - masterColumnGap) / 2);
    auto streamColumn = masterLower.removeFromLeft (masterColumnWidth);
    masterLower.removeFromLeft (masterColumnGap);
    auto monitorColumn = masterLower;
    const int selectorWidth = juce::jmax (112,
        juce::jmin (148,
            juce::jmin (streamColumn.getWidth(), monitorColumn.getWidth()) - 2));

    outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2,
                         sourceSelectorY, selectorWidth, 32);
    streamMeter->setBounds (streamColumn.getX() + 5, sharedFaderTop,
                            streamColumn.getWidth() - 10, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 34,
                          masterArea.getBottom() - 38, 68, 28);

    monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                sourceSelectorY, selectorWidth, 32);
    monitorPowerButton.setBounds (monitorColumn.getCentreX() - 13,
                                  monitorOutputBox.getBottom() + 12, 26, 26);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 41,
                           sharedFaderTop - 8, 82, 92);
    const int modeY = monitorKnob.getBottom() + 10;
    const int modeWidth = selectorWidth / 2;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                    modeY, modeWidth, 28);
    musicCueButton.setBounds (programMonitorButton.getRight(), modeY,
                              selectorWidth - modeWidth, 28);

    licensePanel->setBounds (getLocalBounds());]=])

sc220_p8_once(_ui "remove repeated macro endpoints"
[=[        g.setColour (palette::graphite);
        g.setFont (interFont (10.0f));
        g.drawText ("0%", knob.getX() - 9, knob.getBottom() - 16, 30, 16,
                    juce::Justification::centredLeft);
        g.drawText ("100%", knob.getRight() - 28, knob.getBottom() - 16, 38, 16,
                    juce::Justification::centredRight);]=]
    "")

sc220_p8_once(_ui "intentional monitor power plate"
    "    drawFaderScale (g, streamFader.getBounds());"
[=[    const auto monitorPowerPlate = monitorPowerButton.getBounds().toFloat().expanded (3.0f);
    g.setColour (palette::surfaceAlt);
    g.fillRoundedRectangle (monitorPowerPlate, 6.0f);
    g.setColour (palette::border);
    g.drawRoundedRectangle (monitorPowerPlate.reduced (0.5f), 5.5f, 1.0f);

    drawFaderScale (g, streamFader.getBounds());]=])

foreach(_required IN ITEMS
        "BinaryData::inter_regular_ttf"
        "inputContent.removeFromTop (52)"
        "area.getWidth() * 0.310f"
        "area.getWidth() * 0.420f"
        "juce::jlimit (228, 270"
        "sourceSelectorY + 86"
        "sourceSelectorY - 66, 64, 64"
        "masterArea.getY() + 34, 136, 136"
        "masterColumnGap = 14"
        "monitorOutputBox.getBottom() + 12, 26, 26"
        "monitorPowerPlate"
        "monitorHealthy ? \"MONITOR ACTIVE\" : \"SELECT OUTPUT\"")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P8 alignment invariant missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "ACTIVE: "
        "LEARN IN SILENCE"
        "AUTO DUCK  /  SC220 KEY"
        "VB-CABLE / OBS"
        "HEADPHONES / SPEAKERS"
        "g.drawText (\"0%\""
        "g.drawText (\"100%\""
        "font.setTypefaceName (\"Segoe UI Variable\")")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P8 noisy or non-embedded UI authority remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P8: embedded Inter + aligned input/output control grid")
