# Generate the production SC220 main surface from the reviewed source while
# keeping layout corrections explicit and audit-friendly. This avoids copying
# the full component into a second hand-maintained implementation.

if(NOT DEFINED SC220_UI_SOURCE OR NOT DEFINED SC220_UI_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 UI generator requires SC220_UI_SOURCE and SC220_UI_GENERATED_SOURCE")
endif()

file(READ "${SC220_UI_SOURCE}" _sc220_ui_source)

function(sc220_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 UI layout anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_input_layout_anchor [=[
    stereoButton.setBounds (sc220StripArea.getX() + 10, sc220StripArea.getY() + 126,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 46, sc220StripArea.getY() + 186, 92, 92);
    sc220Meter->setBounds (sc220StripArea.getX() + 18, sc220StripArea.getY() + 306,
                           30, sc220StripArea.getHeight() - 392);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - 34, sc220StripArea.getY() + 302,
                          68, sc220StripArea.getHeight() - 388);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36, sc220StripArea.getBottom() - 42, 72, 31);

    pcSourceBox.setBounds (pcStripArea.getX() + 30, pcStripArea.getY() + 104,
                           pcStripArea.getWidth() - 60, 34);
    pcMeter->setBounds (pcStripArea.getX() + 18, pcStripArea.getY() + 218,
                        30, pcStripArea.getHeight() - 304);
    pcFader.setBounds (pcStripArea.getCentreX() - 34, pcStripArea.getY() + 214,
                       68, pcStripArea.getHeight() - 300);
    duckAmount.setBounds (pcStripArea.getX() + 24, pcStripArea.getBottom() - 82, 70, 70);
]=])

set(_input_layout_replacement [=[
    // One console grid governs SC220, PC Audio and Stream. All three channel
    // strips therefore use the same source row, meter width, fader width,
    // fader top and fader height.
    const int sharedFaderTop = inputArea.getY() + 366;
    const int sharedFaderBottom = inputArea.getBottom() - 154;
    const int sharedFaderHeight = juce::jmax (190, sharedFaderBottom - sharedFaderTop);
    const int sourceSelectorY = sharedFaderTop - 78;
    constexpr int sharedMeterWidth = 30;
    constexpr int sharedFaderWidth = 68;

    stereoButton.setBounds (sc220StripArea.getX() + 10, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 42,
                         sourceSelectorY - 94, 84, 84);
    sc220Meter->setBounds (sc220StripArea.getX() + 18, sharedFaderTop,
                           sharedMeterWidth, sharedFaderHeight);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - sharedFaderWidth / 2,
                          sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         sc220StripArea.getBottom() - 42, 72, 31);

    pcSourceBox.setBounds (pcStripArea.getCentreX() - 82, sourceSelectorY, 164, 34);
    pcMeter->setBounds (pcStripArea.getX() + 18, sharedFaderTop,
                        sharedMeterWidth, sharedFaderHeight);
    pcFader.setBounds (pcStripArea.getCentreX() - sharedFaderWidth / 2,
                       sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    duckAmount.setBounds (pcStripArea.getX() + 24,
                          pcStripArea.getBottom() - 82, 70, 70);
]=])

sc220_replace_once(_sc220_ui_source "input channel geometry"
    "${_input_layout_anchor}" "${_input_layout_replacement}")

set(_master_layout_anchor [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 100, masterArea.getY() + 48, 200, 200);
    auto masterLower = masterArea.reduced (24, 0);
    masterLower.setTop (masterArea.getY() + 300);
    streamMeter->setBounds (masterLower.getX(), masterLower.getY() + 42, 32,
                            masterLower.getHeight() - 140);
    streamFader.setBounds (masterLower.getX() + 40, masterLower.getY() + 38, 72,
                           masterLower.getHeight() - 134);
    streamMute.setBounds (streamFader.getBounds().getCentreX() - 36, masterArea.getBottom() - 44, 72, 31);
    monitorKnob.setBounds (masterArea.getRight() - 164, masterLower.getY() + 52, 108, 108);
    outputBox.setBounds (masterArea.getRight() - 184, masterArea.getBottom() - 108, 154, 36);
]=])

set(_master_layout_replacement [=[
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

sc220_replace_once(_sc220_ui_source "stream channel geometry"
    "${_master_layout_anchor}" "${_master_layout_replacement}")

set(_stream_title_anchor [=[
    g.drawText ("STREAM", streamFader.getX() - 25, streamFader.getY() - 28,
                streamFader.getWidth() + 50, 22, juce::Justification::centred);
]=])

set(_stream_title_replacement [=[
    g.drawText ("STREAM", outputBox.getX(), outputBox.getY() - 47,
                outputBox.getWidth(), 22, juce::Justification::centred);
]=])

sc220_replace_once(_sc220_ui_source "stream title alignment"
    "${_stream_title_anchor}" "${_stream_title_replacement}")

set(_ui_active_anchor [=[
void SC220MainComponent::setUiActive (bool active)
{
    uiActive = active;
    if (active) startTimerHz (30); else stopTimer();
}
]=])

set(_ui_active_replacement [=[
void SC220MainComponent::setUiActive (bool active)
{
    uiActive = active;
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (active);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (active);
    if (active)
        startTimerHz (30);
    else
        stopTimer();
}
]=])

sc220_replace_once(_sc220_ui_source "visual tap lifecycle"
    "${_ui_active_anchor}" "${_ui_active_replacement}")

get_filename_component(_generated_directory "${SC220_UI_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_UI_GENERATED_SOURCE}" "${_sc220_ui_source}")
message(STATUS "SC220 Live aligned UI source: ${SC220_UI_GENERATED_SOURCE}")

# Generate both final sources in normal configure and in CI script mode. Target
# mutation commands are guarded because they are not legal under cmake -P.
set(SC220_ENGINE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/LiveAudioEngine.cpp")
set(SC220_ENGINE_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/LiveAudioEngine_Monitor.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220MonitorEngine.cmake")

set(SC220_UI_MONITOR_SOURCE "${SC220_UI_GENERATED_SOURCE}")
set(SC220_UI_MONITOR_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/SC220MainComponent_Monitor.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220MonitorUi.cmake")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties("${SC220_ENGINE_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties("${SC220_UI_GENERATED_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE
        "${SC220_ENGINE_GENERATED_SOURCE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/live/MonitorOutputClient.cpp"
        "${SC220_UI_MONITOR_GENERATED_SOURCE}")
endif()
