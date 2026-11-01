# Add the physical Monitor Bus controls to the already aligned SC220 UI source.

if(NOT DEFINED SC220_UI_MONITOR_SOURCE OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 monitor UI generator requires source and destination")
endif()

file(READ "${SC220_UI_MONITOR_SOURCE}" _ui)

function(sc220_monitor_ui_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 monitor UI anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_component_list_anchor [=[
             &minimiseButton, &maximiseButton, &closeButton, &settingsButton,
             &enhancerPowerButton, &goLiveButton, &voiceFxButton, &presetBox, &outputBox,
             &stereoButton, &dualMonoButton, &pcSourceBox, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob,
             &sc220Mute, &streamMute })
]=])
set(_component_list_replacement [=[
             &minimiseButton, &maximiseButton, &closeButton, &settingsButton,
             &enhancerPowerButton, &monitorPowerButton, &goLiveButton,
             &voiceFxButton, &presetBox, &outputBox, &monitorOutputBox,
             &stereoButton, &dualMonoButton, &pcSourceBox,
             &programMonitorButton, &musicCueButton, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob,
             &sc220Mute, &streamMute })
]=])
sc220_monitor_ui_replace_once(_ui "visible monitor controls"
    "${_component_list_anchor}" "${_component_list_replacement}")

set(_tooltip_anchor [=[
    enhancerPowerButton.setTooltip ("Smart Enhancer On / Bypass");
    enhancerPowerButton.setIconColour (palette::orange);
    enhancerPowerButton.setOffColour (palette::muted);
]=])
set(_tooltip_replacement [=[
    enhancerPowerButton.setTooltip ("Smart Enhancer On / Bypass");
    enhancerPowerButton.setIconColour (palette::orange);
    enhancerPowerButton.setOffColour (palette::muted);
    monitorPowerButton.setTooltip ("Physical monitor output on / off");
    monitorPowerButton.setIconColour (palette::green);
    monitorPowerButton.setOffColour (palette::muted);
]=])
sc220_monitor_ui_replace_once(_ui "monitor icon styling"
    "${_tooltip_anchor}" "${_tooltip_replacement}")

set(_output_selector_anchor [=[
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
set(_output_selector_replacement [=[
    outputBox.onChange = [this]
    {
        if (outputBox.getSelectedItemIndex() < 0)
            return;
        const auto error = audioEngine.selectOutputDevice (outputBox.getText());
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Output unavailable", error);
    };

    refreshMonitorDevices();
    monitorOutputBox.setTextWhenNothingSelected ("SELECT MONITOR");
    monitorOutputBox.onChange = [this]
    {
        if (monitorOutputBox.getSelectedItemIndex() < 0)
            return;
        const auto error = audioEngine.selectMonitorOutputDevice (monitorOutputBox.getText());
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Monitor unavailable", error);
        refreshAllControls();
    };

    programMonitorButton.setClickingTogglesState (true);
    musicCueButton.setClickingTogglesState (true);
    programMonitorButton.setRadioGroupId (702);
    musicCueButton.setRadioGroupId (702);
    programMonitorButton.onClick = [this]
    {
        audioEngine.setMonitorSource (LiveAudioEngine::MonitorSource::program);
    };
    musicCueButton.onClick = [this]
    {
        audioEngine.setMonitorSource (LiveAudioEngine::MonitorSource::musicCue);
    };

    monitorPowerButton.setClickingTogglesState (true);
    monitorPowerButton.onClick = [this]
    {
        audioEngine.setMonitorMuted (! monitorPowerButton.getToggleState());
        repaint (masterArea);
    };
]=])
sc220_monitor_ui_replace_once(_ui "monitor selector and source mode"
    "${_output_selector_anchor}" "${_output_selector_replacement}")

set(_monitor_knob_anchor [=[
    configureKnob (monitorKnob, 0.0, 10.0, 0.1, "");
]=])
set(_monitor_knob_replacement [=[
    configureKnob (monitorKnob, -60.0, 12.0, 0.1, " dB");
]=])
sc220_monitor_ui_replace_once(_ui "monitor gain range"
    "${_monitor_knob_anchor}" "${_monitor_knob_replacement}")

set(_monitor_callback_anchor [=[
    streamFader.onValueChange = [this] { audioEngine.setOutputTrimDb (static_cast<float> (streamFader.getValue())); };
]=])
set(_monitor_callback_replacement [=[
    streamFader.onValueChange = [this] { audioEngine.setOutputTrimDb (static_cast<float> (streamFader.getValue())); };
    monitorKnob.onValueChange = [this]
    {
        audioEngine.setMonitorGainDb (static_cast<float> (monitorKnob.getValue()));
    };
]=])
sc220_monitor_ui_replace_once(_ui "monitor gain callback"
    "${_monitor_callback_anchor}" "${_monitor_callback_replacement}")

set(_disabled_monitor_anchor [=[
    monitorKnob.setEnabled (false);
    monitorKnob.setTooltip ("Physical monitor output is not active yet.");
]=])
set(_disabled_monitor_replacement [=[
    monitorKnob.setEnabled (true);
    monitorKnob.setTooltip ("Physical monitor output level");
]=])
sc220_monitor_ui_replace_once(_ui "enable physical monitor"
    "${_disabled_monitor_anchor}" "${_disabled_monitor_replacement}")

set(_refresh_values_anchor [=[
    streamFader.setValue (audioEngine.getOutputTrimDb(), juce::dontSendNotification);
]=])
set(_refresh_values_replacement [=[
    streamFader.setValue (audioEngine.getOutputTrimDb(), juce::dontSendNotification);
    monitorKnob.setValue (audioEngine.getMonitorGainDb(), juce::dontSendNotification);
]=])
sc220_monitor_ui_replace_once(_ui "refresh monitor gain"
    "${_refresh_values_anchor}" "${_refresh_values_replacement}")

set(_refresh_toggles_anchor [=[
    enhancerPowerButton.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    goLiveButton.setToggleState (! audioEngine.isOutputMuted(), juce::dontSendNotification);
]=])
set(_refresh_toggles_replacement [=[
    enhancerPowerButton.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    monitorPowerButton.setToggleState (! audioEngine.isMonitorMuted(), juce::dontSendNotification);
    const bool programMonitor = audioEngine.getMonitorSource()
                             == LiveAudioEngine::MonitorSource::program;
    programMonitorButton.setToggleState (programMonitor, juce::dontSendNotification);
    musicCueButton.setToggleState (! programMonitor, juce::dontSendNotification);
    goLiveButton.setToggleState (! audioEngine.isOutputMuted(), juce::dontSendNotification);
]=])
sc220_monitor_ui_replace_once(_ui "refresh monitor state"
    "${_refresh_toggles_anchor}" "${_refresh_toggles_replacement}")

set(_refresh_output_anchor [=[
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
}
]=])
set(_refresh_output_replacement [=[
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);

    if (monitorOutputBox.getNumItems() == 0
        || (status.monitorDevice.isNotEmpty()
            && monitorOutputBox.getItemText (
                   monitorOutputBox.getSelectedItemIndex()) != status.monitorDevice))
        refreshMonitorDevices();
}

void SC220MainComponent::refreshMonitorDevices()
{
    const auto current = audioEngine.getStatus().monitorDevice;
    const auto devices = audioEngine.getMonitorOutputDeviceNames();
    monitorOutputBox.clear (juce::dontSendNotification);
    monitorOutputBox.addItemList (devices, 1);
    const int selected = devices.indexOf (current);
    if (selected >= 0)
        monitorOutputBox.setSelectedItemIndex (selected, juce::dontSendNotification);
    else
        monitorOutputBox.setSelectedId (0, juce::dontSendNotification);
}
]=])
sc220_monitor_ui_replace_once(_ui "monitor device refresh"
    "${_refresh_output_anchor}" "${_refresh_output_replacement}")

set(_master_labels_anchor [=[
    g.drawText ("MONITOR", monitorKnob.getX() - 16, monitorKnob.getY() - 24,
                monitorKnob.getWidth() + 32, 22, juce::Justification::centred);
    g.setColour (palette::muted);
    g.setFont (interFont (9.0f, true));
    g.drawText ("STREAM OUTPUT", outputBox.getX(), outputBox.getY() - 21,
                outputBox.getWidth(), 18, juce::Justification::centred);
]=])
set(_master_labels_replacement [=[
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
sc220_monitor_ui_replace_once(_ui "monitor labels and health"
    "${_master_labels_anchor}" "${_master_labels_replacement}")

set(_aligned_master_anchor [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 100,
                                masterArea.getY() + 48, 200, 200);
    auto masterLower = masterArea.reduced (24, 0);
    const int streamColumnWidth = juce::jmin (174, masterLower.getWidth() / 2);
    auto streamColumn = masterLower.removeFromLeft (streamColumnWidth);

    outputBox.setBounds (streamColumn.getCentreX() - 82,
                         sourceSelectorY, 164, 34);
    streamMeter->setBounds (streamColumn.getX() + 8, sharedFaderTop,
                            sharedMeterWidth, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);

    monitorKnob.setBounds (masterLower.getCentreX() - 54,
                           sourceSelectorY + 72, 108, 108);
]=])
set(_aligned_master_replacement [=[
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
sc220_monitor_ui_replace_once(_ui "monitor column layout"
    "${_aligned_master_anchor}" "${_aligned_master_replacement}")

get_filename_component(_generated_directory "${SC220_UI_MONITOR_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live monitor UI source: ${SC220_UI_MONITOR_GENERATED_SOURCE}")
