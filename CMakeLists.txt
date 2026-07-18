cmake_minimum_required(VERSION 3.22)

project(SonkupikLive VERSION 0.8.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Authoritative ArSonKuPik DSP and preset source
# ---------------------------------------------------------------------------
# Sonkupik Live intentionally does not maintain a divergent copy of the Smart
# Enhancer recipe. Point this at the private askp-vst checkout so the Live app
# compiles the same engine and the same 40-preset bank as the audited VST.
set(SONKUPIK_ASKP_SOURCE_DIR "" CACHE PATH
    "Path to the authoritative masarray/askp-vst source checkout")
set(SONKUPIK_AUDITED_ASKP_COMMIT
    "655874dee6ee69df7212b8073ab941f9607d8faa" CACHE STRING
    "askp-vst commit audited for Sonkupik Live v0.8.2")

set(_askp_engine_dir "")
set(_askp_asset_dir "")
set(_askp_font_dir "")
set(_askp_source_identity "not configured")

if(SONKUPIK_ASKP_SOURCE_DIR)
    cmake_path(ABSOLUTE_PATH SONKUPIK_ASKP_SOURCE_DIR
               BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
               NORMALIZE OUTPUT_VARIABLE _askp_root)
    if(EXISTS "${_askp_root}/engine/arsonkupik_dsp.cpp"
       AND EXISTS "${_askp_root}/engine/arsonkupik_dsp.hpp"
       AND EXISTS "${_askp_root}/engine/arsonkupik_presets.cpp")
        set(_askp_engine_dir "${_askp_root}/engine")
        set(_askp_asset_dir "${_askp_root}/assets/ui")
        set(_askp_font_dir "${_askp_root}/assets/fonts")
        set(_askp_source_identity "${_askp_root}")

        find_package(Git QUIET)
        if(Git_FOUND AND EXISTS "${_askp_root}/.git")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${_askp_root}" rev-parse HEAD
                OUTPUT_VARIABLE _askp_commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(_askp_commit)
                set(_askp_source_identity "${_askp_root} @ ${_askp_commit}")
                if(NOT _askp_commit STREQUAL SONKUPIK_AUDITED_ASKP_COMMIT)
                    message(WARNING
                        "askp-vst commit ${_askp_commit} differs from the audited v0.8.2 lock "
                        "${SONKUPIK_AUDITED_ASKP_COMMIT}. The newer source will be used, but rerun QA.")
                endif()
            endif()
        endif()
    else()
        message(FATAL_ERROR
            "SONKUPIK_ASKP_SOURCE_DIR does not contain the authoritative engine files: "
            "${SONKUPIK_ASKP_SOURCE_DIR}")
    endif()
endif()

if(NOT _askp_engine_dir)
    message(FATAL_ERROR
        "Authoritative askp-vst source is required. Pass "
        "-DSONKUPIK_ASKP_SOURCE_DIR=D:/Git/askp-vst or run "
        "scripts/sync-askp-authority.ps1 first.")
endif()

set(ASKP_DSP_SOURCE "${_askp_engine_dir}/arsonkupik_dsp.cpp")
set(ASKP_PRESET_SOURCE "${_askp_engine_dir}/arsonkupik_presets.cpp")
set(ASKP_FONT_REGULAR "${_askp_font_dir}/plus_jakarta_sans_regular.ttf")
set(ASKP_FONT_SEMIBOLD "${_askp_font_dir}/plus_jakarta_sans_semibold.ttf")
if(NOT EXISTS "${ASKP_FONT_REGULAR}" OR NOT EXISTS "${ASKP_FONT_SEMIBOLD}")
    message(FATAL_ERROR
        "The authoritative askp-vst checkout is missing Plus Jakarta Sans assets.")
endif()

set(ASKP_SPHERE_GLOSS "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/askp_sphere_gloss.png")
if(_askp_asset_dir AND EXISTS "${_askp_asset_dir}/askp_sphere_gloss.png")
    set(ASKP_SPHERE_GLOSS "${_askp_asset_dir}/askp_sphere_gloss.png")
endif()
message(STATUS "Sonkupik Live Smart Enhancer authority: ${_askp_source_identity}")
message(STATUS "Sonkupik Live visual gloss authority: ${ASKP_SPHERE_GLOSS}")

# Match the audited v0.5.13/v0.5.14 performance release: relaxed contraction
# is confined to the DSP translation unit. UI, denoise, metering and routing keep
# normal compiler semantics.
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
        "${ASKP_SPHERE_GLOSS}"
        assets/ui/askp_app_logo.png
        assets/ui/askp_header_logo.png
        assets/ui/askp_header_icon.png
        assets/ui/favicon-96x96.png
        assets/ui/favicon-512x512.png
        "${ASKP_FONT_REGULAR}"
        "${ASKP_FONT_SEMIBOLD}"
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
# Sonkupik Live - lightweight live-streaming audio rack
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
    src/AskpLookAndFeel.cpp
    live/MusicalNoiseClean.cpp
    live/LiveAudioEngine.cpp
    live/LiveLiquidVisualizer.cpp
    live/LiveTheme.cpp
    live/LiveIcons.cpp
    live/LiveWidgets.cpp
    live/LiveMainComponent.cpp
    live/LiveApplication.cpp
)

target_include_directories(SonkupikLive PRIVATE
    "${_askp_engine_dir}"
    src
    live
)

target_compile_definitions(SonkupikLive PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    SONKUPIK_AUDITED_ASKP_COMMIT="${SONKUPIK_AUDITED_ASKP_COMMIT}"
)

target_link_libraries(SonkupikLive PRIVATE
    SonkupikLiveAssets
    juce::juce_audio_devices
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Third-party VST hosting is deliberately not linked into the default Live Safe
# binary. It will be enabled only when crash-isolated scanning and watchdog
# bypass are complete; this keeps startup, binary size and realtime risk lower.
