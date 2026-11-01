cmake_minimum_required(VERSION 3.22)
project(SC220Live VERSION 0.1.3 LANGUAGES C CXX)
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# SC220 source is wholly buildable as a PUBLIC repository. ArSonKuPik DSP
# remains a version-pinned first-party VST3 binary (not a Git submodule).
# GitHub Actions downloads and hashes that bundle during Windows packaging.
include(FetchContent)
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE source")
if(JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-local")
else()
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14 GIT_SHALLOW TRUE GIT_SUBMODULES "")
    FetchContent_MakeAvailable(JUCE)
endif()

add_executable(SC220LoudnessAudit qa/ProgrammeLoudnessAudit.cpp)
target_include_directories(SC220LoudnessAudit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME SC220LoudnessAudit COMMAND SC220LoudnessAudit)

add_executable(SC220ProgrammeAnalyzerAudit qa/ProgrammeAnalyzerAudit.cpp)
target_include_directories(SC220ProgrammeAnalyzerAudit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/live")
target_link_libraries(SC220ProgrammeAnalyzerAudit PRIVATE juce::juce_audio_utils juce::juce_dsp)
target_compile_definitions(SC220ProgrammeAnalyzerAudit PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
add_test(NAME SC220ProgrammeAnalyzerAudit COMMAND SC220ProgrammeAnalyzerAudit)

add_executable(SC220PleasantPresenceAudit qa/SmartPleasantPresenceAudit.cpp live/SmartPleasantPresence.cpp)
target_include_directories(SC220PleasantPresenceAudit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/live")
target_link_libraries(SC220PleasantPresenceAudit PRIVATE juce::juce_audio_utils juce::juce_dsp)
target_compile_definitions(SC220PleasantPresenceAudit PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
add_test(NAME SC220PleasantPresenceAudit COMMAND SC220PleasantPresenceAudit)

# Validate the ACTUAL external binary on the Windows runner before a public
# package is allowed, not merely the source-level presence of a VST3 host.
juce_add_console_app(SC220BundledVstSmoke PRODUCT_NAME "SC220 VST3 Smoke")
target_sources(SC220BundledVstSmoke PRIVATE qa/BundledVstSmoke.cpp live/InputVst3Rack.cpp)
target_include_directories(SC220BundledVstSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/live")
target_link_libraries(SC220BundledVstSmoke PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_compile_definitions(SC220BundledVstSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)

# ---------------------------------------------------------------------------
# Embedded UI assets
# ---------------------------------------------------------------------------
juce_add_binary_data(SC220LiveAssets
    SOURCES
        assets/ui/askp_sphere_gloss.png
        assets/ui/askp_faceplate_brushed.png
        assets/ui/askp_chassis_grain.png
        assets/ui/askp_app_logo.png
        assets/ui/askp_header_logo.png
        assets/ui/askp_header_icon.png
        assets/ui/recording_tech_logo.png
        assets/ui/sc220_mkii_device_alpha.png
        assets/ui/sc220_fader_thumb.svg
        assets/ui/favicon-96x96.png
        assets/ui/favicon-512x512.png
        assets/fonts/inter_regular.ttf
        assets/fonts/inter_medium.ttf
        assets/fonts/inter_semibold.ttf
        assets/fonts/plus_jakarta_sans_regular.ttf
        assets/fonts/plus_jakarta_sans_semibold.ttf
        assets/icons/activity.svg
        assets/icons/audio-waveform.svg
        assets/icons/chevron-down.svg
        assets/icons/circle-check.svg
        assets/icons/cpu.svg
        assets/icons/external-link.svg
        assets/icons/grip-vertical.svg
        assets/icons/headphones.svg
        assets/icons/maximize-2.svg
        assets/icons/mic-2.svg
        assets/icons/minimize-2.svg
        assets/icons/monitor-up.svg
        assets/icons/panel-top-open.svg
        assets/icons/plug-zap.svg
        assets/icons/plus.svg
        assets/icons/power.svg
        assets/icons/radio.svg
        assets/icons/rotate-cw.svg
        assets/icons/scan-line.svg
        assets/icons/settings.svg
        assets/icons/shield-check.svg
        assets/icons/sliders-horizontal.svg
        assets/icons/volume-2.svg
        assets/icons/x.svg
)

# ---------------------------------------------------------------------------
# SC220 Live
# ---------------------------------------------------------------------------
juce_add_gui_app(SC220Live
    PRODUCT_NAME "SC220 Live"
    COMPANY_NAME "Recording Tech / MasArray"
    BUNDLE_ID "com.masarray.sc220.live"
    ICON_BIG assets/ui/favicon-512x512.png
    ICON_SMALL assets/ui/favicon-96x96.png
)

set(SC220_UI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/SC220MainComponent.cpp")
set(SC220_UI_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-ui/SC220MainComponent_Aligned.cpp")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateSC220AlignedUi.cmake")

target_sources(SC220Live PRIVATE
    third_party/sonkupik-sphere-v0.10.4/AskpLookAndFeel.cpp
    live/EnhancerEngineBridge.cpp
    live/LicenseManager.cpp
    live/LicenseCheckoutClient.cpp
    live/LiveLicensePanel.cpp
    live/MusicalNoiseClean.cpp
    live/PcAudioLoopbackCapture.cpp
    live/LiveAudioEngine.cpp
    live/InputVst3Rack.cpp
    live/SmartPleasantPresence.cpp
    live/LiveTheme.cpp
    live/LiveIcons.cpp
    live/LiveWidgets.cpp
    "${SC220_UI_GENERATED_SOURCE}"
    live/LiveApplication.cpp
)

target_include_directories(SC220Live PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/live"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sonkupik-sphere-v0.10.4")

target_compile_definitions(SC220Live PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    SONKUPIK_LICENSE_API_URL="https://masarray-license-api.masari.workers.dev"
    SONKUPIK_LICENSE_PRODUCT_ID="sc220-live"
    SONKUPIK_LICENSE_PRICE_IDR=399000
)

target_link_libraries(SC220Live PRIVATE
    SC220LiveAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(WIN32)
    # JUCE 8.0.14 uses bundled Steinberg ASIO headers. Review the SDK's
    # redistribution/licensing requirements before publishing Windows builds.
    target_compile_definitions(SC220Live PRIVATE JUCE_ASIO=1)
    target_link_libraries(SC220Live PRIVATE ole32 avrt)
endif()

