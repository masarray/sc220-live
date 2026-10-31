set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional cached JUCE checkout")
if(JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
  add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-local")
else()
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14 GIT_SHALLOW TRUE GIT_SUBMODULES "")
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_gui_app(SC220Live
  PRODUCT_NAME "SC220 Live" COMPANY_NAME "MasArray"
  BUNDLE_ID "com.masarray.sc220.live"
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/favicon-512x512.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/favicon-96x96.png")
target_sources(SC220Live PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/standalone_vst_host/SC220ExternalHost.cpp")
target_link_libraries(SC220Live PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors
  juce::juce_dsp juce::juce_gui_extra
  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
target_compile_definitions(SC220Live PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_PLUGINHOST_VST3=1 JUCE_STANDALONE_APPLICATION=1
  SC220_EXTERNAL_VST_ONLY=1)
if(WIN32)
  target_compile_definitions(SC220Live PRIVATE JUCE_ASIO=1)
  target_link_libraries(SC220Live PRIVATE ole32 avrt)
endif()
message(STATUS "SC220 independent VST3-host build: no ASK-P source/submodule")
