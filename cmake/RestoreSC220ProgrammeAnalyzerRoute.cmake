# Restore the proven programme-analyzer source after the final SC220 UI polish.
#
# Older generated UI can still point the spectrum at the Voice Enhancer analysis
# tap, while the current polished UI already expands that call into a throttled
# post-master programmeAnalyzerTap() block. LiveAudioEngine intentionally keeps
# the per-voice visual tap disabled and feeds programmeAnalyzer from the exact
# post-limiter master bus.
#
# This pass therefore repairs the one known stale route when present, then
# validates the semantic invariant instead of requiring one exact formatting
# shape. That keeps configure fail-closed without rejecting the already-correct
# polished route.

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

# Repair the legacy direct Voice-tap call if an upstream generator still emits
# it. Current PolishSC220FinalUi.cmake already emits a throttled multi-line
# programmeAnalyzerTap() block, so absence of this exact anchor is not an error.
string(FIND "${_ui}" "${_disabled_voice_tap_anchor}" _legacy_route_position)
if(NOT _legacy_route_position EQUAL -1)
    string(REPLACE "${_disabled_voice_tap_anchor}" "${_post_master_replacement}"
                   _ui "${_ui}")
endif()

# Semantic fail-closed checks. Any remaining Voice analysisTap read is a real
# regression; at least one programmeAnalyzerTap read must exist regardless of
# whether it is the simple legacy replacement or the current throttled block.
string(FIND "${_ui}" "getVoiceEnhancerBridge().analysisTap()" _stale_route)
if(NOT _stale_route EQUAL -1)
    message(FATAL_ERROR "SC220 generated UI still reads the disabled Voice Enhancer visual tap")
endif()

string(FIND "${_ui}" "audioEngine.programmeAnalyzerTap()" _post_master_route)
if(_post_master_route EQUAL -1)
    message(FATAL_ERROR
        "SC220 analyzer route changed: generated UI no longer reads the post-master programmeAnalyzerTap")
endif()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live analyzer route: post-master programmeAnalyzerTap")
