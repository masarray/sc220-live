# P7 product-density pass. Keep every routing and DSP control intact while
# making the default console fit a normal 1280x720 operator workspace with a
# calmer hierarchy and smaller, consistently aligned controls.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P7 compact layout requires generated UI and app sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p7_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P7 compact-layout anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

sc220_p7_once(_app "compact resize limits"
    "        setResizeLimits (1180, 720, 1920, 1200);"
    "        setResizeLimits (1080, 680, 1760, 1080);")
sc220_p7_once(_app "compact initial window"
    "        centreWithSize (1600, 900);"
    "        centreWithSize (1280, 720);")

sc220_p7_once(_ui "compact surface regions"
[=[
    headerArea = area.removeFromTop (66);
    footerArea = area.removeFromBottom (58);
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.310f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.440f);
]=]
[=[
    headerArea = area.removeFromTop (58);
    footerArea = area.removeFromBottom (46);
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.320f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.430f);
]=])

sc220_p7_once(_ui "compact header controls"
[=[
    closeButton.setBounds (getWidth() - 48, 13, 38, 38);
    maximiseButton.setBounds (getWidth() - 90, 13, 38, 38);
    minimiseButton.setBounds (getWidth() - 132, 13, 38, 38);
    settingsButton.setBounds (getWidth() - 184, 13, 38, 38);
    goLiveButton.setBounds (getWidth() - 322, 10, 126, 46);
    presetBox.setBounds (338, 15, 192, 36);

    auto inputContent = inputArea.reduced (18, 48);
]=]
[=[
    closeButton.setBounds (getWidth() - 42, 10, 32, 32);
    maximiseButton.setBounds (getWidth() - 78, 10, 32, 32);
    minimiseButton.setBounds (getWidth() - 114, 10, 32, 32);
    settingsButton.setBounds (getWidth() - 158, 10, 32, 32);
    goLiveButton.setBounds (getWidth() - 276, 8, 108, 40);
    presetBox.setBounds (300, 12, 180, 32);

    auto inputContent = inputArea.reduced (14, 40);
]=])

sc220_p7_once(_ui "compact shared fader grid"
[=[
    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (245, 288,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.37f));
    const int sharedFaderTop = sourceSelectorY + 82;
    const int sharedFaderBottom = inputArea.getBottom() - 76;
    const int sharedFaderHeight = juce::jmax (176,
        sharedFaderBottom - sharedFaderTop);
    constexpr int sharedFaderWidth = 68;
]=]
[=[
    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (214, 258,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.355f));
    const int sharedFaderTop = sourceSelectorY + 70;
    const int sharedFaderBottom = inputArea.getBottom() - 60;
    const int sharedFaderHeight = juce::jmax (158,
        sharedFaderBottom - sharedFaderTop);
    constexpr int sharedFaderWidth = 60;
]=])

sc220_p7_once(_ui "compact input controls"
[=[
    stereoButton.setBounds (sc220StripArea.getX() + 10, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 42,
                         sourceSelectorY - 94, 84, 84);

    const int noiseRowY = sourceSelectorY + 34;
    const int noiseWidth = (sc220StripArea.getWidth() - 26) / 2;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 10, noiseRowY,
                                noiseWidth, 22);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + 6, noiseRowY,
                                noiseWidth, 22);
]=]
[=[
    stereoButton.setBounds (sc220StripArea.getX() + 8, sourceSelectorY,
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
                                noiseWidth, 20);
]=])

sc220_p7_once(_ui "compact channel controls"
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         inputArea.getBottom() - 44, 72, 31);

    pcSourceBox.setBounds (pcStripArea.getCentreX() - 82, sourceSelectorY, 164, 34);
]=]
[=[
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 34,
                         inputArea.getBottom() - 38, 68, 28);

    pcSourceBox.setBounds (pcStripArea.getCentreX() - 75, sourceSelectorY, 150, 30);
]=])
sc220_p7_once(_ui "compact duck control"
[=[
    duckAmount.setBounds (pcStripArea.getCentreX() - 38,
                          sourceSelectorY - 94, 76, 84);
]=]
[=[
    duckAmount.setBounds (pcStripArea.getCentreX() - 36,
                          sourceSelectorY - 84, 72, 76);
]=])

sc220_p7_once(_ui "compact Signature layout"
[=[
    presetBox.setBounds (signatureArea.getCentreX() - 102, signatureArea.getY() + 72, 204, 36);
    enhancerPowerButton.setBounds (signatureArea.getRight() - 54, signatureArea.getY() + 20, 36, 36);
    spectrumDisplay->setBounds (signatureArea.reduced (34, 0).withTop (signatureArea.getY() + 128)
                                .withBottom (signatureArea.getY() + juce::roundToInt (signatureArea.getHeight() * 0.56f)));

    auto macroRow = signatureArea.reduced (24, 0);
    macroRow.setTop (spectrumDisplay->getBottom() + 52);
    macroRow.setBottom (signatureArea.getBottom() - 62);
]=]
[=[
    presetBox.setBounds (signatureArea.getCentreX() - 95, signatureArea.getY() + 62, 190, 32);
    enhancerPowerButton.setBounds (signatureArea.getRight() - 46, signatureArea.getY() + 16, 32, 32);
    spectrumDisplay->setBounds (signatureArea.reduced (26, 0).withTop (signatureArea.getY() + 108)
                                .withBottom (signatureArea.getY() + juce::roundToInt (signatureArea.getHeight() * 0.535f)));

    auto macroRow = signatureArea.reduced (20, 0);
    macroRow.setTop (spectrumDisplay->getBottom() + 42);
    macroRow.setBottom (signatureArea.getBottom() - 50);
]=])
sc220_p7_once(_ui "compact macro knobs"
[=[
        const int knobWidth = juce::jmin (88, slot.getWidth() - 12);
        const int knobHeight = juce::jmin (132, slot.getHeight());
]=]
[=[
        const int knobWidth = juce::jmin (78, slot.getWidth() - 10);
        const int knobHeight = juce::jmin (112, slot.getHeight());
]=])

sc220_p7_once(_ui "compact master top"
[=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 88,
                                masterArea.getY() + 42, 176, 176);
    auto masterLower = masterArea.reduced (16, 0);
    constexpr int masterColumnGap = 14;
]=]
[=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 78,
                                masterArea.getY() + 34, 156, 156);
    auto masterLower = masterArea.reduced (12, 0);
    constexpr int masterColumnGap = 12;
]=])
sc220_p7_once(_ui "compact master controls"
[=[
    outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2,
                         sourceSelectorY, selectorWidth, 36);
]=]
[=[
    outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2,
                         sourceSelectorY, selectorWidth, 32);
]=])
sc220_p7_once(_ui "compact stream mute"
[=[
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);
]=]
[=[
    streamMute.setBounds (streamColumn.getCentreX() - 34,
                          masterArea.getBottom() - 38, 68, 28);
]=])
sc220_p7_once(_ui "compact monitor controls"
[=[
    monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                sourceSelectorY, selectorWidth, 36);
    monitorPowerButton.setBounds (monitorColumn.getRight() - 32,
                                  sourceSelectorY - 43, 30, 30);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 44,
                           sharedFaderTop + 8, 88, 96);
    const int modeY = monitorKnob.getBottom() + 14;
]=]
[=[
    monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                sourceSelectorY, selectorWidth, 32);
    monitorPowerButton.setBounds (monitorColumn.getRight() - 29,
                                  sourceSelectorY - 38, 27, 27);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 40,
                           sharedFaderTop + 6, 80, 88);
    const int modeY = monitorKnob.getBottom() + 10;
]=])
sc220_p7_once(_ui "compact monitor mode buttons"
    "                                    modeY, modeWidth, 30);"
    "                                    modeY, modeWidth, 28);")
sc220_p7_once(_ui "compact music cue button"
    "                              selectorWidth - modeWidth, 30);"
    "                              selectorWidth - modeWidth, 28);")

# Calm the remaining oversized display hierarchy without reducing body/control
# text below the existing readable 8.2-11.5 px range.
sc220_p7_once(_ui "smaller header title font"
    "    g.setFont (interFont (18.0f, true));"
    "    g.setFont (interFont (16.0f, true));")
sc220_p7_once(_ui "smaller input heading font"
    "    g.setFont (interFont (15.0f, true));"
    "    g.setFont (interFont (13.5f, true));")
sc220_p7_once(_ui "smaller Signature heading font"
    "    g.setFont (interFont (16.0f, true));"
    "    g.setFont (interFont (14.0f, true));")
sc220_p7_once(_ui "smaller macro value font"
    "        g.setFont (interFont (14.0f));"
    "        g.setFont (interFont (12.5f));")

foreach(_required IN ITEMS
        "setResizeLimits (1080, 680, 1760, 1080)"
        "centreWithSize (1280, 720)"
        "headerArea = area.removeFromTop (58)"
        "footerArea = area.removeFromBottom (46)"
        "sharedFaderWidth = 60"
        "juce::jmax (158"
        "spectrumDisplay->getBottom() + 42"
        "masterColumnGap = 12")
    string(FIND "${_app}${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P7 compact-layout invariant missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P7: compact 1280x720 console with aligned operator-density grid")
