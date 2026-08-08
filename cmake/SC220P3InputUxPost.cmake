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

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P3 Noise controls fit above shared fader grid")
