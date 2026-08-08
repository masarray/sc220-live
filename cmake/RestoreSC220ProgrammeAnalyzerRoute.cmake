# Restore the proven programme-analyzer source after the final SC220 UI polish.
#
# The raw SC220MainComponent still points the spectrum at the Voice Enhancer
# analysis tap, while LiveAudioEngine intentionally disables that tap and feeds
# the dedicated programmeAnalyzer from the exact post-limiter master bus.
# This fail-closed pass keeps the release analyzer wired to the live master bus.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 programme analyzer route requires generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

set(_disabled_voice_tap_anchor [=[
    spectrumDisplay->updateFromTap (audioEngine.getVoiceEnhancerBridge().analysisTap(),
                                    lastStatus.sampleRate);
]=])
set(_post_master_replacement [=[
    spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                    lastStatus.sampleRate);
]=])

string(FIND "${_ui}" "${_disabled_voice_tap_anchor}" _route_position)
if(_route_position EQUAL -1)
    string(FIND "${_ui}" "${_post_master_replacement}" _already_fixed)
    if(_already_fixed EQUAL -1)
        message(FATAL_ERROR
            "SC220 analyzer route changed: neither the known stale route nor the post-master route was found")
    endif()
else()
    string(REPLACE "${_disabled_voice_tap_anchor}" "${_post_master_replacement}"
                   _ui "${_ui}")
endif()

string(FIND "${_ui}" "getVoiceEnhancerBridge().analysisTap()" _stale_route)
if(NOT _stale_route EQUAL -1)
    message(FATAL_ERROR "SC220 generated UI still reads the disabled Voice Enhancer visual tap")
endif()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live analyzer route: post-master programmeAnalyzerTap")
