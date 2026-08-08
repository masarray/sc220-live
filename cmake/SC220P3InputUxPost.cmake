# Compact final spacing for the direct Noise controls. The aligned console has
# exactly 78 px between the source-mode row and the shared fader top.
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

foreach(_pair IN ITEMS
        "const int noiseRowY = sourceSelectorY + 37;|const int noiseRowY = sourceSelectorY + 34;"
        "noiseWidth, 25);|noiseWidth, 22);")
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _from)
    list(GET _parts 1 _to)
    string(FIND "${_ui}" "${_from}" _p)
    if(_p EQUAL -1)
        message(FATAL_ERROR "SC220 P3 compact Noise UI anchor changed: ${_from}")
    endif()
    string(REPLACE "${_from}" "${_to}" _ui "${_ui}")
endforeach()

# A Learn transaction is atomic from the user's point of view. Disabling Noise
# Clean halfway through would stop shouldCaptureProfile() and leave a profile in
# an indeterminate partial state. Lock both direct buttons for the two-second
# capture, then release them automatically when learning completes.
set(_learning_state_anchor [=[
    learnNoiseButton.setButtonText (audioEngine.getNoiseClean().isLearning()
                                    ? "LEARNING..." : "LEARN");
]=])
set(_learning_state_replacement [=[
    const bool noiseLearning = audioEngine.getNoiseClean().isLearning();
    learnNoiseButton.setButtonText (noiseLearning ? "LEARNING..." : "LEARN");
    noiseCleanButton.setEnabled (! noiseLearning);
    learnNoiseButton.setEnabled (! noiseLearning);
]=])
string(FIND "${_ui}" "${_learning_state_anchor}" _learning_state_pos)
if(_learning_state_pos EQUAL -1)
    message(FATAL_ERROR "SC220 P3 Noise learning-state anchor changed")
endif()
string(REPLACE "${_learning_state_anchor}" "${_learning_state_replacement}"
               _ui "${_ui}")

# Keep the Settings mirror consistent with the direct controls. The Noise Clean
# toggle cannot be changed during Learn; the profile capture therefore always
# reaches a deterministic 100% / frozen state.
set(_settings_toggle_anchor [=[
    menu.addItem (2, "Noise Clean", true, noiseEnabled);
]=])
set(_settings_toggle_replacement [=[
    menu.addItem (2, "Noise Clean", ! noise.isLearning(), noiseEnabled);
]=])
string(FIND "${_ui}" "${_settings_toggle_anchor}" _settings_toggle_pos)
if(NOT _settings_toggle_pos EQUAL -1)
    string(REPLACE "${_settings_toggle_anchor}" "${_settings_toggle_replacement}"
                   _ui "${_ui}")
endif()

foreach(_required IN ITEMS
        "noiseCleanButton.setEnabled (! noiseLearning)"
        "learnNoiseButton.setEnabled (! noiseLearning)")
    string(FIND "${_ui}" "${_required}" _required_pos)
    if(_required_pos EQUAL -1)
        message(FATAL_ERROR "SC220 P3 atomic Learn authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P3 Noise controls fit above shared fader grid and lock during Learn")
