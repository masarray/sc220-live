cmake_minimum_required(VERSION 3.22)

project(SonkupikLive VERSION 0.13.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Embedded ArSonKuPik Signature Engine v0.5.19
# ---------------------------------------------------------------------------
# v0.12.0 no longer reads DSP or visualizer source from a neighbouring checkout.
# The reviewed engine is vendored and hash-locked, making clean builds fully
# deterministic. The Sonkupik sphere remains a separate, locally versioned UI.
set(ASKP_PORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/askp-engine-v0.5.19")
set(ASKP_ENGINE_DIR "${ASKP_PORT_ROOT}/engine")
set(ASKP_PROCESSOR_DIR "${ASKP_PORT_ROOT}/src")
set(ASKP_REFERENCE_DIR "${ASKP_PORT_ROOT}/reference")
set(SONKUPIK_SPHERE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sonkupik-sphere-v0.10.4")

set(ASKP_DSP_SOURCE "${ASKP_ENGINE_DIR}/arsonkupik_dsp.cpp")
set(ASKP_PRESET_SOURCE "${ASKP_ENGINE_DIR}/arsonkupik_presets.cpp")
set(ASKP_LIVE_PROCESSOR_SOURCE "${ASKP_PROCESSOR_DIR}/PluginProcessor.cpp")
set(ASKP_REFERENCE_PROCESSOR_SOURCE "${ASKP_REFERENCE_DIR}/PluginProcessor.cpp")
set(ASKP_LICENSE_SOURCE "${ASKP_PROCESSOR_DIR}/LicenseManager.cpp")
set(SONKUPIK_LOOK_AND_FEEL_SOURCE "${SONKUPIK_SPHERE_DIR}/AskpLookAndFeel.cpp")
set(SONKUPIK_ORB_SOURCE "${SONKUPIK_SPHERE_DIR}/OrbVisualizer.cpp")

function(sonkupik_assert_sha256 file_path expected_sha)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Required embedded authority file is missing: ${file_path}")
    endif()
    file(SHA256 "${file_path}" actual_sha)
    if(NOT actual_sha STREQUAL expected_sha)
        message(FATAL_ERROR
            "Embedded authority hash mismatch for ${file_path}\n"
            "Expected: ${expected_sha}\nActual:   ${actual_sha}\n"
            "Re-port and rerun QA instead of building an unreviewed engine.")
    endif()
endfunction()

sonkupik_assert_sha256("${ASKP_DSP_SOURCE}"
    "d716a086dfcba06332871405f85058304be7f96bc356e8aa755fcc5300f79df2")
sonkupik_assert_sha256("${ASKP_ENGINE_DIR}/arsonkupik_dsp.hpp"
    "fbfe14be0ea8e7254341c85cc03f96cea8cf96c247c2fbb5b310cb88f02d62c2")
sonkupik_assert_sha256("${ASKP_PRESET_SOURCE}"
    "948ecf3bc7520413d1ea7a7b42cfca44291bbbf1ecb12c74aaeaca58a746910f")
sonkupik_assert_sha256("${ASKP_LIVE_PROCESSOR_SOURCE}"
    "3b8a31b753449386febf4d28478b1023e0ec10ec9b64ac0d1aff4215b84493ed")
sonkupik_assert_sha256("${ASKP_PROCESSOR_DIR}/PluginProcessor.h"
    "e2b5fef5a34330a8e26c437aed82aef97ba3703115de5094edd7142fbdd7fc6e")
sonkupik_assert_sha256("${ASKP_LICENSE_SOURCE}"
    "5e052cf06d65692cac1e03765e5e037a38bc2e4fa3b211b89ccbef658a342228")
sonkupik_assert_sha256("${ASKP_PROCESSOR_DIR}/LicenseManager.h"
    "dc6832ad9ccdb566f45dadb4ca36ec744845d212714f696389ae2b41207e3755")
sonkupik_assert_sha256("${ASKP_PROCESSOR_DIR}/LicensePublicKey.h"
    "8fa4238d321dbdeb00b5408a10365cbc8110cd28006aa2ccd526c90f2e5ca372")
sonkupik_assert_sha256("${CMAKE_CURRENT_SOURCE_DIR}/live/LicenseCheckoutClient.cpp"
    "eae7ce93266eca6fb3b7a3ce3e4149e1b69eff93076ae9cbb12ea8ee67af2b38")
sonkupik_assert_sha256("${CMAKE_CURRENT_SOURCE_DIR}/live/LiveLicensePanel.cpp"
    "3d34980d62f4b5bb8a90e3a91fbc6b64f281eda47b553da87360d80789ae3140")
sonkupik_assert_sha256("${SONKUPIK_ORB_SOURCE}"
    "f5ed2351fb9d22dfbc520b56c86e19091632c516d174cc83983d708d4eda7a04")

file(READ "${ASKP_ENGINE_DIR}/arsonkupik_dsp.hpp" _askp_header)
if(NOT _askp_header MATCHES "ASKP_DSP_HEADER_REVISION 2026072702")
    message(FATAL_ERROR "Expected ArSonKuPik engine revision 2026072702 was not found")
endif()
file(READ "${ASKP_PRESET_SOURCE}" _askp_presets)
if(NOT _askp_presets MATCHES "profiles.reserve\\(13\\)")
    message(FATAL_ERROR "Expected the reviewed 13-profile v0.5.19 platform")
endif()
message(STATUS "Sonkupik Live enhancer authority: embedded ArSonKuPik v0.5.19 / revision 2026072702")
message(STATUS "Sonkupik Live sphere authority: local approved v0.10.4 visual baseline")

# Relaxed contraction stays confined to the DSP translation unit.
set_property(SOURCE "${ASKP_DSP_SOURCE}" APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/fp:fast>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fno-math-errno>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-fno-trapping-math>"
    "$<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-ffp-contract=fast>"
)

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

# ---------------------------------------------------------------------------
# Embedded UI assets
# ---------------------------------------------------------------------------
juce_add_binary_data(SonkupikLiveAssets
    SOURCES
        assets/ui/askp_sphere_gloss.png
        assets/ui/askp_faceplate_brushed.png
        assets/ui/askp_chassis_grain.png
        assets/ui/askp_app_logo.png
        assets/ui/askp_header_logo.png
        assets/ui/askp_header_icon.png
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
# Sonkupik Live
# ---------------------------------------------------------------------------
juce_add_gui_app(SonkupikLive
    PRODUCT_NAME "Sonkupik Live"
    COMPANY_NAME "MasArray"
    BUNDLE_ID "com.masarray.sonkupik.live"
    ICON_BIG assets/ui/favicon-512x512.png
    ICON_SMALL assets/ui/favicon-96x96.png
)

target_sources(SonkupikLive PRIVATE
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
    live/LiveAudioEngine.cpp
    live/LiveTheme.cpp
    live/LiveIcons.cpp
    live/LiveWidgets.cpp
    live/SonkupikSphereVisualizer.cpp
    live/LiveMainComponent.cpp
    live/LiveApplication.cpp
)

target_include_directories(SonkupikLive PRIVATE
    "${ASKP_PROCESSOR_DIR}"
    "${ASKP_ENGINE_DIR}"
    "${SONKUPIK_SPHERE_DIR}"
    live
)

target_compile_definitions(SonkupikLive PRIVATE
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
    JucePlugin_Version=0.5.19
    JucePlugin_VersionCode=0x513
    JucePlugin_VersionString="0.5.19"
    ASKP_BUILD_CHANNEL="Sonkupik Live Embedded Engine v0.5.20"
    SONKUPIK_ENGINE_REVISION=2026072702
    SONKUPIK_LICENSE_API_URL="https://masarray-license-api.masari.workers.dev"
    SONKUPIK_LICENSE_PRODUCT_ID="sonkupik-live"
    SONKUPIK_LICENSE_PRICE_IDR=249000
)

target_link_libraries(SonkupikLive PRIVATE
    SonkupikLiveAssets
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

# ---------------------------------------------------------------------------
# Optional preset-by-preset parity renderers
# ---------------------------------------------------------------------------
option(SONKUPIK_BUILD_PARITY_QA
    "Build v0.5.19 reference and Sonkupik Live offline parity renderers" OFF)

if(SONKUPIK_BUILD_PARITY_QA)
    function(sonkupik_add_parity_renderer target_name product_name processor_source processor_include variant_name)
        juce_add_console_app(${target_name} PRODUCT_NAME "${product_name}")
        target_sources(${target_name} PRIVATE
            qa/ParityRendererMain.cpp
            "${ASKP_DSP_SOURCE}"
            "${ASKP_PRESET_SOURCE}"
            "${processor_source}"
            "${ASKP_LICENSE_SOURCE}"
        )
        target_include_directories(${target_name} PRIVATE
            "${processor_include}"
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
            JucePlugin_Version=0.5.19 JucePlugin_VersionCode=0x513
            JucePlugin_VersionString="0.5.19"
            ASKP_BUILD_CHANNEL="Sonkupik Live v0.5.19 Parity"
            SONKUPIK_PARITY_VARIANT="${variant_name}"
        )
        target_link_libraries(${target_name} PRIVATE
            SonkupikLiveAssets
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
        "Sonkupik v0.5.19 Reference Renderer"
        "${ASKP_REFERENCE_PROCESSOR_SOURCE}"
        "${ASKP_REFERENCE_DIR}"
        "askp-v0.5.19-headless-80ms")

    sonkupik_add_parity_renderer(
        SonkupikParityLive
        "Sonkupik v0.5.19 Live Renderer"
        "${ASKP_LIVE_PROCESSOR_SOURCE}"
        "${ASKP_PROCESSOR_DIR}"
        "sonkupik-live-12ms")
endif()
