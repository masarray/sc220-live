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

# The native mixer source is the single authority for channel/fader/layout geometry.
# The prior generated grid is intentionally disabled: it overwrites both FX racks.
# Visual tap lifecycle and downstream Monitor UI generation remain active.

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

# Generate final sources in normal configure and CI script mode. Target mutation
# commands are guarded because they are not legal under cmake -P.
set(SC220_ENGINE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/LiveAudioEngine.cpp")
set(SC220_ENGINE_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/LiveAudioEngine_Monitor.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220MonitorEngine.cmake")

set(SC220_UI_MONITOR_SOURCE "${SC220_UI_GENERATED_SOURCE}")
set(SC220_UI_MONITOR_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/SC220MainComponent_Monitor.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220MonitorUi.cmake")

set(SC220_APP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/LiveApplication.cpp")
set(SC220_APP_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/LiveApplication_BroadcastState.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220BroadcastState.cmake")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties("${SC220_ENGINE_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties("${SC220_UI_GENERATED_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties("${SC220_APP_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE
        "${SC220_ENGINE_GENERATED_SOURCE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/live/MonitorOutputClient.cpp"
        "${SC220_UI_MONITOR_GENERATED_SOURCE}"
        "${SC220_APP_GENERATED_SOURCE}")
endif()
