cmake_minimum_required(VERSION 3.22)

project(SC220Live VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Pinned ArSonKuPik P0.3 development audio authority
# ---------------------------------------------------------------------------
# The full ASK-P repository is mounted as a git submodule at one reviewed
# development checkpoint. Canonical v0.5.23 DSP remains the base source, while
# the upstream fail-closed P0/P0.1/P0.2/P0.3 generator materialises the exact
# listening-approved effective DSP compiled by SC220 Live. The ASK-P editor
# remains excluded from the SC220 Live surface.
set(SONKUPIK_ASKP_COMMIT "240b9d3be3ca1fbdedfa5b7027e465c22a56b0ea")
set(ASKP_PORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/askp-vst")
set(ASKP_ENGINE_DIR "${ASKP_PORT_ROOT}/engine")
set(ASKP_PROCESSOR_DIR "${ASKP_PORT_ROOT}/src")
set(ASKP_P03_GENERATOR "${ASKP_PORT_ROOT}/scripts/generate_p0_musical_rebalance_dsp.py")
set(SONKUPIK_SPHERE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sonkupik-sphere-v0.10.4")

foreach(_required IN ITEMS
        "${ASKP_ENGINE_DIR}/arsonkupik_dsp.cpp"
        "${ASKP_ENGINE_DIR}/arsonkupik_dsp.hpp"
        "${ASKP_ENGINE_DIR}/arsonkupik_presets.cpp"
        "${ASKP_ENGINE_DIR}/wow_headroom_guard.hpp"
        "${ASKP_PROCESSOR_DIR}/PluginProcessor.cpp"
        "${ASKP_PROCESSOR_DIR}/PluginProcessor.h"
        "${ASKP_PROCESSOR_DIR}/LicenseManager.cpp"
        "${ASKP_PROCESSOR_DIR}/LicenseManager.h"
        "${ASKP_PROCESSOR_DIR}/LicensePublicKey.h"
        "${ASKP_P03_GENERATOR}")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "Pinned ASK-P P0.3 authority is missing: ${_required}\n"
            "Run: git submodule update --init --recursive third_party/askp-vst")
    endif()
endforeach()

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${ASKP_PORT_ROOT}" rev-parse HEAD
        RESULT_VARIABLE _askp_git_result
        OUTPUT_VARIABLE _askp_actual_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_askp_git_result EQUAL 0
       AND NOT _askp_actual_commit STREQUAL SONKUPIK_ASKP_COMMIT)
        message(FATAL_ERROR
            "ASK-P authority commit mismatch.\n"
            "Expected: ${SONKUPIK_ASKP_COMMIT}\n"
            "Actual:   ${_askp_actual_commit}\n"
            "Run git submodule update --init --recursive --force --checkout third_party/askp-vst")
    endif()
endif()

set(ASKP_DSP_CANONICAL_SOURCE "${ASKP_ENGINE_DIR}/arsonkupik_dsp.cpp")
set(ASKP_PRESET_SOURCE "${ASKP_ENGINE_DIR}/arsonkupik_presets.cpp")
set(ASKP_REFERENCE_PROCESSOR_SOURCE "${ASKP_PROCESSOR_DIR}/PluginProcessor.cpp")
set(ASKP_REFERENCE_PROCESSOR_HEADER "${ASKP_PROCESSOR_DIR}/PluginProcessor.h")
set(ASKP_REFERENCE_LICENSE_SOURCE "${ASKP_PROCESSOR_DIR}/LicenseManager.cpp")
set(ASKP_REFERENCE_LICENSE_HEADER "${ASKP_PROCESSOR_DIR}/LicenseManager.h")
set(SONKUPIK_LOOK_AND_FEEL_SOURCE "${SONKUPIK_SPHERE_DIR}/AskpLookAndFeel.cpp")
set(SONKUPIK_ORB_SOURCE "${SONKUPIK_SPHERE_DIR}/OrbVisualizer.cpp")

function(sonkupik_assert_sha256 file_path expected_sha)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Required immutable Sonkupik authority file is missing: ${file_path}")
    endif()
    file(SHA256 "${file_path}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
        message(FATAL_ERROR
            "Immutable Sonkupik authority hash mismatch for ${file_path}\n"
            "Expected: ${expected_sha}\nActual:   ${actual_sha}")
    endif()
endfunction()

# ASK-P is locked by its complete git commit. The local sphere remains locked by
# source hash because it intentionally does not come from the ASK-P submodule.
sonkupik_assert_sha256("${SONKUPIK_ORB_SOURCE}"
    "f5ed2351fb9d22dfbc520b56c86e19091632c516d174cc83983d708d4eda7a04")

file(READ "${ASKP_ENGINE_DIR}/arsonkupik_dsp.hpp" _askp_header)
if(NOT _askp_header MATCHES "ASKP_DSP_HEADER_REVISION 2026073102")
    message(FATAL_ERROR "Expected ArSonKuPik v0.5.23 base engine revision 2026073102 was not found")
endif()
file(READ "${ASKP_PRESET_SOURCE}" _askp_presets)
if(NOT _askp_presets MATCHES "profiles.reserve\\(13\\)")
    message(FATAL_ERROR "Expected the reviewed 13-profile ASK-P platform")
endif()
if(NOT _askp_presets MATCHES "P0 Musical Rebalance")
    message(FATAL_ERROR "Expected P0/P0.1/P0.2/P0.3 preset calibration was not found")
endif()

# ---------------------------------------------------------------------------
# Deterministic P0.3 DSP + headless processor variants
# ---------------------------------------------------------------------------
# Upstream deliberately keeps P0.3 as a development checkpoint generated from
# the canonical v0.5.23 DSP. Run the exact pinned generator here instead of
# pretending the unchanged canonical arsonkupik_dsp.cpp already contains P0.3.
set(ASKP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/askp-p0.3-240b9d3")
file(MAKE_DIRECTORY "${ASKP_GENERATED_DIR}")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ASKP_DSP_SOURCE "${ASKP_GENERATED_DIR}/arsonkupik_dsp_p0_3.cpp")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${ASKP_P03_GENERATOR}"
            --input "${ASKP_DSP_CANONICAL_SOURCE}"
            --output "${ASKP_DSP_SOURCE}"
    RESULT_VARIABLE _askp_p03_result
    OUTPUT_VARIABLE _askp_p03_stdout
    ERROR_VARIABLE _askp_p03_stderr)
if(NOT _askp_p03_result EQUAL 0)
    message(FATAL_ERROR
        "ASK-P P0.3 DSP generation failed.\n"
        "stdout: ${_askp_p03_stdout}\n"
        "stderr: ${_askp_p03_stderr}")
endif()
file(READ "${ASKP_DSP_SOURCE}" _askp_p03_dsp)
foreach(_p03_anchor IN ITEMS
        "lowerBodyLedVocalTarget"
        "tickleMidLift = 1.21"
        "runtime.gen_air_base = additive(w.high_width, 0.066, 0.026, 0.094)"
        "working_.compressor.enabled = false"
        "runtime.warm_transient_guard = 0.58")
    string(FIND "${_askp_p03_dsp}" "${_p03_anchor}" _p03_anchor_pos)
    if(_p03_anchor_pos EQUAL -1)
        message(FATAL_ERROR "ASK-P P0.3 effective DSP anchor missing: ${_p03_anchor}")
    endif()
endforeach()

# Keep the proven ASK-P 365-day state machine and RSA activation format, but
# isolate SC220 Live's trial clock and activation from the VST product. The
# ASKP-R1 request/ASKP-A1 activation transport stays compatible with the
# existing MasArray payment service and signing key.
file(READ "${ASKP_REFERENCE_LICENSE_HEADER}" _license_header)
file(READ "${ASKP_REFERENCE_LICENSE_SOURCE}" _license_source)
if(NOT _license_header MATCHES "betaDurationDays = 365")
    message(FATAL_ERROR "ASK-P licence authority is no longer the reviewed 365-day implementation")
endif()
foreach(_anchor IN ITEMS
        "ASKP-LOCAL-STATE-V1-84F6D183A7C24F96"
        "HKEY_CURRENT_USER\\\\Software\\\\MasArray\\\\ArSonKuPik\\\\"
        "ASKP-HARDWARE-FINGERPRINT-V1"
        ".getChildFile(\"ArSonKuPik\")")
    string(FIND "${_license_source}" "${_anchor}" _license_anchor_pos)
    if(_license_anchor_pos EQUAL -1)
        message(FATAL_ERROR "ASK-P licence isolation anchor changed: ${_anchor}")
    endif()
endforeach()
string(REPLACE "ASKP-LOCAL-STATE-V1-84F6D183A7C24F96"
               "SC220-LIVE-LOCAL-STATE-V1-2FA42A09B8454C50"
               _license_source "${_license_source}")
string(REPLACE "HKEY_CURRENT_USER\\\\Software\\\\MasArray\\\\ArSonKuPik\\\\"
               "HKEY_CURRENT_USER\\\\Software\\\\MasArray\\\\SC220 Live\\\\"
               _license_source "${_license_source}")
string(REPLACE "ASKP-HARDWARE-FINGERPRINT-V1"
               "SC220-LIVE-HARDWARE-FINGERPRINT-V1"
               _license_source "${_license_source}")
string(REPLACE ".getChildFile(\"ArSonKuPik\")"
               ".getChildFile(\"SC220 Live\")"
               _license_source "${_license_source}")
string(REPLACE "ArSonKuPik Customer" "SC220 Live Customer"
               _license_source "${_license_source}")
string(REPLACE "Public Beta ended - Read-only mode"
               "Free period ended - Creative controls are read-only"
               _license_source "${_license_source}")
string(REPLACE "Public Beta - " "SC220 Live Free - "
               _license_source "${_license_source}")
file(WRITE "${ASKP_GENERATED_DIR}/LicenseManager.h" "${_license_header}")
file(WRITE "${ASKP_GENERATED_DIR}/LicenseManager.cpp" "${_license_source}")
set(ASKP_LICENSE_SOURCE "${ASKP_GENERATED_DIR}/LicenseManager.cpp")

file(READ "${ASKP_REFERENCE_PROCESSOR_HEADER}" _processor_header)
set(_has_editor_anchor "bool hasEditor() const override { return true; }")
string(FIND "${_processor_header}" "${_has_editor_anchor}" _has_editor_pos)
if(_has_editor_pos EQUAL -1)
    message(FATAL_ERROR "ASK-P v0.5.23 hasEditor anchor changed; re-audit the port")
endif()
string(REPLACE "${_has_editor_anchor}"
               "bool hasEditor() const override { return false; }"
               _headless_header "${_processor_header}")
file(WRITE "${ASKP_GENERATED_DIR}/PluginProcessor.h" "${_headless_header}")

file(READ "${ASKP_REFERENCE_PROCESSOR_SOURCE}" _processor_source)
set(_editor_include_anchor "#include \"PluginEditor.h\"")
string(FIND "${_processor_source}" "${_editor_include_anchor}" _editor_include_pos)
if(_editor_include_pos EQUAL -1)
    message(FATAL_ERROR "ASK-P v0.5.23 PluginEditor include anchor changed; re-audit the port")
endif()
string(REPLACE "${_editor_include_anchor}"
               "// Sonkupik Live headless processor: ASK-P editor intentionally excluded."
               _headless_source "${_processor_source}")

set(_create_editor_anchor "juce::AudioProcessorEditor* Processor::createEditor()\n{\n    return new Editor(*this);\n}")
string(FIND "${_headless_source}" "${_create_editor_anchor}" _create_editor_pos)
if(_create_editor_pos EQUAL -1)
    message(FATAL_ERROR "ASK-P v0.5.23 createEditor anchor changed; re-audit the port")
endif()
string(REPLACE "${_create_editor_anchor}"
               "juce::AudioProcessorEditor* Processor::createEditor()\n{\n    return nullptr;\n}"
               _reference_headless_source "${_headless_source}")
file(WRITE "${ASKP_GENERATED_DIR}/PluginProcessor_Reference.cpp"
     "${_reference_headless_source}")

set(_ramp_anchor "const double ramp = 0.08; // 80 ms knob glide")
string(FIND "${_reference_headless_source}" "${_ramp_anchor}" _ramp_pos)
if(_ramp_pos EQUAL -1)
    message(FATAL_ERROR "ASK-P v0.5.23 80 ms control-ramp anchor changed; re-audit the port")
endif()
string(REPLACE "${_ramp_anchor}"
               "const double ramp = 0.012; // Sonkupik Live low-latency macro glide"
               _live_headless_source "${_reference_headless_source}")
file(WRITE "${ASKP_GENERATED_DIR}/PluginProcessor_Live.cpp"
     "${_live_headless_source}")
set(ASKP_LIVE_PROCESSOR_SOURCE "${ASKP_GENERATED_DIR}/PluginProcessor_Live.cpp")
set(ASKP_HEADLESS_REFERENCE_SOURCE "${ASKP_GENERATED_DIR}/PluginProcessor_Reference.cpp")

message(STATUS "SC220 Live enhancer authority: ASK-P P0.3 checkpoint @ ${SONKUPIK_ASKP_COMMIT}")
message(STATUS "SC220 Live base engine: v0.5.23 revision 2026073102; profiles: 13; Live glide: 12 ms")
message(STATUS "SC220 Live effective DSP: P0/P0.1/P0.2/P0.3 musical rebalance")

# Relaxed contraction stays confined to the generated effective DSP translation unit.
set_property(SOURCE "${ASKP_DSP_SOURCE}" APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/fp:fast>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fno-math-errno>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fno-trapping-math>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-ffp-contract=fast>"
)

add_executable(SC220LoudnessAudit qa/ProgrammeLoudnessAudit.cpp)
target_include_directories(SC220LoudnessAudit PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME SC220LoudnessAudit COMMAND SC220LoudnessAudit)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE source tree")
if(JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE from ${JUCE_SOURCE_DIR}")
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-local")
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.14
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES ""
    )
    FetchContent_MakeAvailable(JUCE)
endif()

add_executable(SC220ProgrammeAnalyzerAudit qa/ProgrammeAnalyzerAudit.cpp)
target_include_directories(SC220ProgrammeAnalyzerAudit PRIVATE
    "${ASKP_GENERATED_DIR}"
    "${ASKP_PROCESSOR_DIR}"
    "${ASKP_ENGINE_DIR}"
)
target_link_libraries(SC220ProgrammeAnalyzerAudit PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
target_compile_definitions(SC220ProgrammeAnalyzerAudit PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
add_test(NAME SC220ProgrammeAnalyzerAudit COMMAND SC220ProgrammeAnalyzerAudit)

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
        assets/ui/sc220_mkii_device.png
        assets/ui/favicon-96x96.png
        assets/ui/favicon-512x512.png
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
    "${ASKP_DSP_SOURCE}"
    "${ASKP_PRESET_SOURCE}"
    "${ASKP_LIVE_PROCESSOR_SOURCE}"
    "${ASKP_LICENSE_SOURCE}"
    "${SONKUPIK_LOOK_AND_FEEL_SOURCE}"
    "${SONKUPIK_ORB_SOURCE}"
    live/EnhancerEngineBridge.cpp
    live/LicenseCheckoutClient.cpp
    live/LiveLicensePanel.cpp
    live/MusicalNoiseClean.cpp
    live/PcAudioLoopbackCapture.cpp
    live/LiveAudioEngine.cpp
    live/LiveTheme.cpp
    live/LiveIcons.cpp
    live/LiveWidgets.cpp
    live/SonkupikSphereVisualizer.cpp
    live/LiveMainComponent.cpp
    "${SC220_UI_GENERATED_SOURCE}"
    live/LiveApplication.cpp
)

target_include_directories(SC220Live PRIVATE
    "${ASKP_GENERATED_DIR}"
    "${ASKP_PROCESSOR_DIR}"
    "${ASKP_ENGINE_DIR}"
    "${SONKUPIK_SPHERE_DIR}"
    live
)

target_compile_definitions(SC220Live PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=0
    JucePlugin_Build_Standalone=0
    JucePlugin_Name="ArSonKuPik"
    JucePlugin_ManufacturerCode=0x4d734172
    JucePlugin_PluginCode=0x416b5673
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_EditorRequiresKeyboardFocus=0
    JucePlugin_Version=0.5.23
    JucePlugin_VersionCode=0x517
    JucePlugin_VersionString="0.5.23"
    ASKP_BUILD_CHANNEL="SC220 Live Embedded Authority P0.3 @ 240b9d3"
    SONKUPIK_ENGINE_REVISION=2026073102
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
    target_link_libraries(SC220Live PRIVATE ole32 avrt)
endif()

# ---------------------------------------------------------------------------
# Optional preset-by-preset parity renderers
# ---------------------------------------------------------------------------
option(SONKUPIK_BUILD_PARITY_QA
    "Build ASK-P P0.3 checkpoint reference and Sonkupik Live offline parity renderers" OFF)

if(SONKUPIK_BUILD_PARITY_QA)
    function(sonkupik_add_parity_renderer target_name product_name processor_source variant_name)
        juce_add_console_app(${target_name} PRODUCT_NAME "${product_name}")
        target_sources(${target_name} PRIVATE
            qa/ParityRendererMain.cpp
            "${ASKP_DSP_SOURCE}"
            "${ASKP_PRESET_SOURCE}"
            "${processor_source}"
            "${ASKP_LICENSE_SOURCE}"
        )
        target_include_directories(${target_name} PRIVATE
            "${ASKP_GENERATED_DIR}"
            "${ASKP_PROCESSOR_DIR}"
            "${ASKP_ENGINE_DIR}"
        )
        target_compile_definitions(${target_name} PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_STANDALONE_APPLICATION=0 JucePlugin_Build_Standalone=0
            JucePlugin_Name="ArSonKuPik"
            JucePlugin_ManufacturerCode=0x4d734172
            JucePlugin_PluginCode=0x416b5673
            JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
            JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0
            JucePlugin_EditorRequiresKeyboardFocus=0
            JucePlugin_Version=0.5.23 JucePlugin_VersionCode=0x517
            JucePlugin_VersionString="0.5.23"
            ASKP_BUILD_CHANNEL="Sonkupik Live P0.3 Checkpoint Parity"
            SONKUPIK_PARITY_VARIANT="${variant_name}"
        )
        target_link_libraries(${target_name} PRIVATE
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
    endfunction()

    sonkupik_add_parity_renderer(
        SonkupikParityReference
        "ASK-P P0.3 Checkpoint Reference Renderer"
        "${ASKP_HEADLESS_REFERENCE_SOURCE}"
        "askp-p0.3-headless-80ms")

    sonkupik_add_parity_renderer(
        SonkupikParityLive
        "Sonkupik P0.3 Live Renderer"
        "${ASKP_LIVE_PROCESSOR_SOURCE}"
        "sonkupik-p0.3-live-12ms")
endif()
