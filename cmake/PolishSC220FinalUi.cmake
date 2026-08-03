# Final production authority applied after the aligned, physical-monitor, and
# persisted-state generators. This stage owns the final desktop geometry and
# the lightweight programme spectrum. Exact replacements fail configure when an
# upstream source changes, preventing silent layout drift.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 final UI polish requires SC220_UI_MONITOR_GENERATED_SOURCE")
endif()
if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 final UI polish requires SC220_ENGINE_GENERATED_SOURCE")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_polish_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 final polish anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Stable three-zone desktop composition. The Master area receives a real quarter
# of the width so Stream and Monitor never have to overlap at the 1180 px minimum.
set(_panel_ratio_anchor [=[
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.325f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.455f);
]=])
set(_panel_ratio_replacement [=[
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.310f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.440f);
]=])
sc220_polish_replace_once(_ui "stable panel ratios"
    "${_panel_ratio_anchor}" "${_panel_ratio_replacement}")

# One responsive console grid owns SC220, PC Audio, and Stream. The selector row
# adapts to window height; faders remain useful at 720 px and grow at 900 px.
set(_fader_grid_anchor [=[
    const int sharedFaderTop = inputArea.getY() + 366;
    const int sharedFaderBottom = inputArea.getBottom() - 154;
    const int sharedFaderHeight = juce::jmax (190, sharedFaderBottom - sharedFaderTop);
    const int sourceSelectorY = sharedFaderTop - 78;
]=])
set(_fader_grid_replacement [=[
    const int sourceSelectorY = inputArea.getY()
        + juce::jlimit (245, 288,
            juce::roundToInt (static_cast<float> (inputArea.getHeight()) * 0.37f));
    const int sharedFaderTop = sourceSelectorY + 82;
    const int sharedFaderBottom = inputArea.getBottom() - 76;
    const int sharedFaderHeight = juce::jmax (176,
        sharedFaderBottom - sharedFaderTop);
]=])
sc220_polish_replace_once(_ui "responsive shared fader grid"
    "${_fader_grid_anchor}" "${_fader_grid_replacement}")

# Input Trim and Auto Duck become symmetrical pre-fader controls. Moving ducking
# above the PC source row removes the former collision with the fader label/key.
set(_duck_layout_anchor [=[
    duckAmount.setBounds (pcStripArea.getX() + 24,
                          pcStripArea.getBottom() - 82, 70, 70);
]=])
set(_duck_layout_replacement [=[
    duckAmount.setBounds (pcStripArea.getCentreX() - 38,
                          sourceSelectorY - 94, 76, 84);
]=])
sc220_polish_replace_once(_ui "aligned pre-fader ducking control"
    "${_duck_layout_anchor}" "${_duck_layout_replacement}")

# Replace the old right-side key annotation with a compact centred label. The
# slider's own value box remains below the knob, with no text crossing the fader.
set(_input_labels_anchor [=[
    g.setColour (palette::muted);
    g.setFont (interFont (9.0f, true));
    g.drawText ("INPUT TRIM", inputTrim.getX() - 14, inputTrim.getY() - 22,
                inputTrim.getWidth() + 28, 18, juce::Justification::centred);
    g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    g.drawText ("PC AUDIO", pcFader.getX() - 20, pcFader.getBottom() + 2,
                pcFader.getWidth() + 40, 18, juce::Justification::centred);
    g.drawText ("DUCK", duckAmount.getX() - 10, duckAmount.getY() - 17,
                duckAmount.getWidth() + 20, 18, juce::Justification::centred);

    const bool ducking = audioEngine.isDuckingEnabled();
    g.setColour (ducking ? palette::green : palette::muted);
    g.fillEllipse (static_cast<float> (duckAmount.getRight() + 8),
                   static_cast<float> (duckAmount.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (8.8f, true));
    g.drawText ("KEY: SC220", duckAmount.getRight() + 20, duckAmount.getY(),
                82, duckAmount.getHeight(), juce::Justification::centredLeft);
]=])
set(_input_labels_replacement [=[
    g.setColour (palette::muted);
    g.setFont (interFont (9.0f, true));
    g.drawText ("INPUT TRIM", inputTrim.getX() - 14, inputTrim.getY() - 22,
                inputTrim.getWidth() + 28, 18, juce::Justification::centred);
    g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    g.drawText ("PC AUDIO", pcFader.getX() - 20, pcFader.getBottom() + 2,
                pcFader.getWidth() + 40, 18, juce::Justification::centred);

    const bool ducking = audioEngine.isDuckingEnabled();
    g.setColour (ducking ? palette::green : palette::muted);
    g.setFont (interFont (8.8f, true));
    g.drawText ("AUTO DUCK  /  SC220 KEY",
                pcStripArea.getX() + 8, duckAmount.getY() - 23,
                pcStripArea.getWidth() - 16, 18,
                juce::Justification::centred);
]=])
sc220_polish_replace_once(_ui "clean input and ducking labels"
    "${_input_labels_anchor}" "${_input_labels_replacement}")

# Two explicit output columns. Selectors share one row, have a guaranteed gap,
# and scale down safely at minimum window width without touching each other.
set(_master_layout_anchor [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 100,
                                masterArea.getY() + 48, 200, 200);
    auto masterLower = masterArea.reduced (24, 0);
    const int streamColumnWidth = juce::jmin (174, masterLower.getWidth() / 2);
    auto streamColumn = masterLower.removeFromLeft (streamColumnWidth);
    auto monitorColumn = masterLower;

    outputBox.setBounds (streamColumn.getCentreX() - 82,
                         sourceSelectorY, 164, 34);
    streamMeter->setBounds (streamColumn.getX() + 8, sharedFaderTop,
                            sharedMeterWidth, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);

    monitorOutputBox.setBounds (monitorColumn.getCentreX() - 82,
                                sourceSelectorY, 164, 34);
    monitorPowerButton.setBounds (monitorOutputBox.getRight() - 36,
                                  monitorOutputBox.getY() - 48, 34, 34);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 48,
                           sourceSelectorY + 62, 96, 96);
    const int modeY = monitorKnob.getBottom() + 18;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - 82,
                                    modeY, 82, 30);
    musicCueButton.setBounds (monitorColumn.getCentreX(), modeY, 82, 30);
]=])
set(_master_layout_replacement [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 88,
                                masterArea.getY() + 42, 176, 176);
    auto masterLower = masterArea.reduced (16, 0);
    constexpr int masterColumnGap = 14;
    const int masterColumnWidth = juce::jmax (108,
        (masterLower.getWidth() - masterColumnGap) / 2);
    auto streamColumn = masterLower.removeFromLeft (masterColumnWidth);
    masterLower.removeFromLeft (masterColumnGap);
    auto monitorColumn = masterLower;
    const int selectorWidth = juce::jmax (106,
        juce::jmin (158,
            juce::jmin (streamColumn.getWidth(), monitorColumn.getWidth()) - 2));

    outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2,
                         sourceSelectorY, selectorWidth, 36);
    streamMeter->setBounds (streamColumn.getX() + 5, sharedFaderTop,
                            streamColumn.getWidth() - 10, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);

    monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                sourceSelectorY, selectorWidth, 36);
    monitorPowerButton.setBounds (monitorColumn.getRight() - 32,
                                  sourceSelectorY - 43, 30, 30);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 44,
                           sharedFaderTop + 8, 88, 96);
    const int modeY = monitorKnob.getBottom() + 14;
    const int modeWidth = selectorWidth / 2;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                    modeY, modeWidth, 30);
    musicCueButton.setBounds (programMonitorButton.getRight(), modeY,
                              selectorWidth - modeWidth, 30);
]=])
sc220_polish_replace_once(_ui "collision-free stream and monitor columns"
    "${_master_layout_anchor}" "${_master_layout_replacement}")

# Clear routing hierarchy: Stream means VB-CABLE; Monitor means a physical
# headphone/speaker endpoint. Secondary descriptions never overlap controls.
set(_master_labels_anchor [=[
    g.drawText ("STREAM", outputBox.getX(), outputBox.getY() - 47,
                outputBox.getWidth(), 22, juce::Justification::centred);
    g.drawText ("MONITOR", monitorOutputBox.getX(), monitorOutputBox.getY() - 47,
                monitorOutputBox.getWidth(), 22, juce::Justification::centred);
    const bool monitorHealthy = lastStatus.monitorRunning
                             && lastStatus.monitorError.isEmpty();
    g.setColour (monitorHealthy ? palette::green : palette::muted);
    g.setFont (interFont (8.8f, true));
    g.drawText (monitorHealthy ? "PHYSICAL OUTPUT ACTIVE" : "SELECT PHYSICAL OUTPUT",
                monitorOutputBox.getX(), musicCueButton.getBottom() + 8,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);
]=])
set(_master_labels_replacement [=[
    g.drawText ("STREAM OUTPUT", outputBox.getX(), outputBox.getY() - 43,
                outputBox.getWidth(), 20, juce::Justification::centred);
    g.drawText ("MONITOR OUTPUT", monitorOutputBox.getX(), monitorOutputBox.getY() - 43,
                monitorOutputBox.getWidth(), 20, juce::Justification::centred);
    g.setColour (palette::muted);
    g.setFont (interFont (8.2f, true));
    g.drawText ("VB-CABLE / OBS", outputBox.getX(), outputBox.getBottom() + 3,
                outputBox.getWidth(), 16, juce::Justification::centred);
    g.drawText ("HEADPHONES / SPEAKERS", monitorOutputBox.getX(),
                monitorOutputBox.getBottom() + 3,
                monitorOutputBox.getWidth(), 16, juce::Justification::centred);

    const bool monitorHealthy = lastStatus.monitorRunning
                             && lastStatus.monitorError.isEmpty();
    g.setColour (monitorHealthy ? palette::green : palette::muted);
    g.setFont (interFont (8.5f, true));
    g.drawText (monitorHealthy ? "MONITOR ACTIVE" : "SELECT PHYSICAL OUTPUT",
                monitorOutputBox.getX(), musicCueButton.getBottom() + 7,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);
]=])
sc220_polish_replace_once(_ui "explicit stream and monitor labels"
    "${_master_labels_anchor}" "${_master_labels_replacement}")

# Stream routing is intentionally restricted to virtual-cable endpoints. A
# previously persisted physical output no longer appears as a valid Stream choice.
set(_stream_list_anchor [=[
    outputBox.addItemList (audioEngine.getOutputDeviceNames(), 1);
    outputBox.setTextWhenNothingSelected ("SELECT VB-CABLE");
]=])
set(_stream_list_replacement [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
        if (name.containsIgnoreCase ("CABLE"))
            streamOutputs.add (name);
    for (int i = 0; i < streamOutputs.size(); ++i)
        outputBox.addItem (i == 0 ? "VB-CABLE"
                                  : "VB-CABLE " + juce::String (i + 1), i + 1);
    outputBox.setTextWhenNothingSelected ("SELECT VB-CABLE");
]=])
sc220_polish_replace_once(_ui "VB-CABLE-only stream selector"
    "${_stream_list_anchor}" "${_stream_list_replacement}")

set(_stream_change_anchor [=[
    outputBox.onChange = [this]
    {
        if (outputBox.getSelectedItemIndex() < 0)
            return;
        const auto error = audioEngine.selectOutputDevice (outputBox.getText());
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Output unavailable", error);
    };
]=])
set(_stream_change_replacement [=[
    outputBox.onChange = [this]
    {
        const int selectedIndex = outputBox.getSelectedItemIndex();
        if (selectedIndex < 0)
            return;
        juce::StringArray streamOutputs;
        for (const auto& name : audioEngine.getOutputDeviceNames())
            if (name.containsIgnoreCase ("CABLE"))
                streamOutputs.add (name);
        if (! juce::isPositiveAndBelow (selectedIndex, streamOutputs.size()))
            return;
        const auto error = audioEngine.selectOutputDevice (streamOutputs[selectedIndex]);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Output unavailable", error);
    };
]=])
sc220_polish_replace_once(_ui "short stream alias with real endpoint mapping"
    "${_stream_change_anchor}" "${_stream_change_replacement}")

set(_stream_refresh_anchor [=[
    const auto outputIndex = audioEngine.getOutputDeviceNames().indexOf (status.outputDevice);
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
]=])
set(_stream_refresh_replacement [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
        if (name.containsIgnoreCase ("CABLE"))
            streamOutputs.add (name);
    const int outputIndex = streamOutputs.indexOf (status.outputDevice);
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
    else
        outputBox.setSelectedId (0, juce::dontSendNotification);
]=])
sc220_polish_replace_once(_ui "filtered stream selection refresh"
    "${_stream_refresh_anchor}" "${_stream_refresh_replacement}")

# Use one dedicated post-master programme tap. Per-lane taps stay disabled so the
# analyzer cannot be starved or receive duplicate Voice/Music blocks.
set(_constructor_taps_anchor [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);
]=])
set(_constructor_taps_replacement [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.setProgrammeAnalyzerEnabled (true);
]=])
sc220_polish_replace_once(_ui "programme analyzer constructor state"
    "${_constructor_taps_anchor}" "${_constructor_taps_replacement}")

set(_destructor_taps_anchor [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
]=])
set(_destructor_taps_replacement [=[
    audioEngine.getVoiceEnhancerBridge().analysisTap().setEnabled (false);
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
]=])
# The constructor replacement creates the same two-line sequence once. Replace
# the last occurrence by first isolating the destructor block.
set(_destructor_block_anchor [=[
SC220MainComponent::~SC220MainComponent()
{
    stopTimer();
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    setLookAndFeel (nullptr);
}
]=])
set(_destructor_block_replacement [=[
SC220MainComponent::~SC220MainComponent()
{
    stopTimer();
    audioEngine.setProgrammeAnalyzerEnabled (false);
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    setLookAndFeel (nullptr);
}
]=])
sc220_polish_replace_once(_ui "programme analyzer destructor state"
    "${_destructor_block_anchor}" "${_destructor_block_replacement}")

set(_ui_active_taps_anchor [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (active);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (active);
]=])
set(_ui_active_taps_replacement [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.setProgrammeAnalyzerEnabled (active);
]=])
sc220_polish_replace_once(_ui "programme analyzer UI lifecycle"
    "${_ui_active_taps_anchor}" "${_ui_active_taps_replacement}")

set(_spectrum_update_anchor [=[
    spectrumDisplay->updateFromTap (audioEngine.getVoiceEnhancerBridge().analysisTap(),
                                    lastStatus.sampleRate);
]=])
set(_spectrum_update_replacement [=[
    static int spectrumDivider = 0;
    if (++spectrumDivider >= 2)
    {
        spectrumDivider = 0;
        audioEngine.setProgrammeAnalyzerEnabled (uiActive);
        spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                        lastStatus.sampleRate);
    }
]=])
sc220_polish_replace_once(_ui "post-master programme spectrum"
    "${_spectrum_update_anchor}" "${_spectrum_update_replacement}")

# The reviewed engine now owns its post-master analyzer and metering directly.
# Keep configure-time generation fail-closed if either authority disappears.
foreach(_required_ui_token IN ITEMS
        "static constexpr int fftOrder = 10;"
        "std::array<float, 32> smoothed {};"
        "std::array<float, fftSize * 2> leftFftData {};"
        "std::array<float, fftSize * 2> rightFftData {};"
        "getSliderThumbRadius"
        "return 34;"
        "POST-MASTER ANALYZER"
        "audioEngine.programmeAnalyzerTap()")
    string(FIND "${_ui}" "${_required_ui_token}" _required_ui_position)
    if(_required_ui_position EQUAL -1)
        message(FATAL_ERROR "SC220 final analyzer UI authority missing: ${_required_ui_token}")
    endif()
endforeach()

foreach(_required_engine_token IN ITEMS
        "programmeAnalyzer.push"
        "loudnessIntegrator.processFrame"
        "truePeakOversampler.processSamplesUp")
    string(FIND "${_engine}" "${_required_engine_token}" _required_engine_position)
    if(_required_engine_position EQUAL -1)
        message(FATAL_ERROR "SC220 final programme meter authority missing: ${_required_engine_token}")
    endif()
endforeach()

# Recover the intended SC220 -> VB-CABLE routing even when an older settings file
# persisted headphones as the primary stream output.
set(_default_stream_anchor "    if (savedDeviceState == nullptr)")
set(_default_stream_replacement "    if (savedDeviceState == nullptr || ! outputCable.load())")
sc220_polish_replace_once(_engine "restore VB-CABLE stream authority"
    "${_default_stream_anchor}" "${_default_stream_replacement}")

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live final console authority: ${SC220_UI_MONITOR_GENERATED_SOURCE}")
message(STATUS "SC220 Live post-master spectrum authority: ${SC220_ENGINE_GENERATED_SOURCE}")
