# Source-only CMake validation of the exact production translation units.
# No private ASK-P token, compiler, or Steinberg SDK download is required.
cmake_minimum_required(VERSION 3.22)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(OUT "${CMAKE_CURRENT_BINARY_DIR}/sc220-generated-qa")
file(MAKE_DIRECTORY "${OUT}")
set(SC220_UI_SOURCE "${ROOT}/live/SC220MainComponent.cpp")
set(SC220_UI_GENERATED_SOURCE "${OUT}/SC220MainComponent_Aligned.cpp")
include("${ROOT}/cmake/GenerateSC220AlignedUi.cmake")
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" ENGINE)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" UI)
foreach(required IN ITEMS "voiceVst3.process (voiceBlock, voiceMidi)" "monitorOutput.push" "noiseClean.shouldCaptureProfile()" "truePeakOversampler")
  string(FIND "${ENGINE}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Effective engine lost required source: ${required}")
  endif()
endforeach()
foreach(required IN ITEMS "voiceFxButton" "monitorOutputBox" "musicCueButton"
                          "channelInsertPowers" "channelInsertMenus"
                          "showChannelInsertEditor (channel, slot)"
                          "chooseChannelInsert (channel, slot)"
                          "favicon_96x96_png")
  string(FIND "${UI}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Effective UI lost required source: ${required}")
  endif()
endforeach()
foreach(stale IN ITEMS "sharedFaderTop" "sharedFaderHeight" "sourceSelectorY")
  string(FIND "${UI}" "${stale}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "Obsolete mixer geometry override survived: ${stale}")
  endif()
endforeach()
message(STATUS "PASS: SC220 native mixer, slot controls, monitor, VST UI and engine")
