# Final calibration pass for the generated SC220 Live analyzer and loudness UI.
# ProgrammeBandTelemetry already applies sample-rate-aware pink-noise correction.
# This pass removes the cosmetic placeholder contour and connects the loudness
# dial to live 400 ms BS.1770 momentary telemetry while retaining LUFS-I.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 meter calibration requires final UI and engine sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_meter_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 meter calibration anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_ui_include_anchor [=[
#include "ProgrammeBandTelemetry.h"
]=])
set(_ui_include_replacement [=[
#include "ProgrammeBandTelemetry.h"
#include "ProgrammeLiveLoudnessTelemetry.h"
]=])
sc220_meter_replace_once(_ui "UI live loudness include"
    "${_ui_include_anchor}" "${_ui_include_replacement}")

set(_engine_include_anchor [=[
#include "ProgrammeBandTelemetry.h"
]=])
set(_engine_include_replacement [=[
#include "ProgrammeBandTelemetry.h"
#include "ProgrammeLiveLoudnessTelemetry.h"
]=])
sc220_meter_replace_once(_engine "engine live loudness include"
    "${_engine_include_anchor}" "${_engine_include_replacement}")

set(_display_contour_anchor [=[
            // Display-only contour. The final calibration pass removes this
            // placeholder so all bands use the same pink-flat baseline.
            float displayDb = rawDb[band];
            if (band <= 2)
                displayDb += 4.0f;
            else if (band <= 5)
                displayDb += 2.0f;
            else if (band >= 12)
                displayDb -= 1.5f;
]=])
set(_display_contour_replacement [=[
            // ProgrammeBandTelemetry is already calibrated so equal-energy
            // pink noise is flat. Do not add any cosmetic bass/treble contour.
            const float displayDb = rawDb[band];
]=])
sc220_meter_replace_once(_ui "remove cosmetic analyzer contour"
    "${_display_contour_anchor}" "${_display_contour_replacement}")

string(REPLACE "POST-LIMITER BANDS" "PINK-FLAT BANDS" _ui "${_ui}")

set(_reading_anchor [=[
    void setReading (float nextLufs, float nextTruePeak, float nextElapsedSeconds,
                     bool nextReady)
    {
        lufs = nextLufs;
        truePeak = nextTruePeak;
        elapsedSeconds = nextElapsedSeconds;
        ready = nextReady;
        repaint();
    }
]=])
set(_reading_replacement [=[
    void setReading (float nextLufs, float nextMomentary, float nextTruePeak,
                     float nextElapsedSeconds, bool nextReady)
    {
        lufs = nextLufs;
        momentaryLufs = nextMomentary;
        truePeak = nextTruePeak;
        elapsedSeconds = nextElapsedSeconds;
        ready = nextReady;

        if (std::isfinite (nextMomentary) && nextMomentary > -100.0f)
        {
            if (! needleInitialised)
            {
                needleLufs = nextMomentary;
                needleInitialised = true;
            }
            else
            {
                const float coefficient = nextMomentary > needleLufs ? 0.62f : 0.18f;
                needleLufs += (nextMomentary - needleLufs) * coefficient;
            }
        }
        repaint();
    }
]=])
sc220_meter_replace_once(_ui "live loudness reading signature"
    "${_reading_anchor}" "${_reading_replacement}")

set(_dial_source_anchor [=[
        const float displayLufs = ready ? lufs : -36.0f;
]=])
set(_dial_source_replacement [=[
        // Needle and LED ring use responsive 400 ms LUFS-M. The large number
        // remains gated programme-integrated LUFS-I.
        const float displayLufs = needleInitialised ? needleLufs
                                                    : (ready ? lufs : -36.0f);
]=])
sc220_meter_replace_once(_ui "drive loudness needle from LUFS-M"
    "${_dial_source_anchor}" "${_dial_source_replacement}")

set(_live_text_anchor [=[
        const auto truePeakText = truePeak <= -100.0f
            ? juce::String ("-- dBTP") : juce::String (truePeak, 1) + " dBTP MAX";
        g.drawText (truePeakText,
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);
]=])
set(_live_text_replacement [=[
        const auto momentaryText = momentaryLufs <= -100.0f
            ? juce::String ("M --") : "M " + juce::String (momentaryLufs, 1);
        const auto truePeakText = truePeak <= -100.0f
            ? juce::String ("TP --") : "TP " + juce::String (truePeak, 1);
        g.drawText (momentaryText + "  /  " + truePeakText,
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);
]=])
sc220_meter_replace_once(_ui "show LUFS-M and true peak"
    "${_live_text_anchor}" "${_live_text_replacement}")

set(_loudness_members_anchor [=[
    float lufs = -120.0f;
    float truePeak = -120.0f;
    float elapsedSeconds = 0.0f;
    bool ready = false;
]=])
set(_loudness_members_replacement [=[
    float lufs = -120.0f;
    float momentaryLufs = -120.0f;
    float needleLufs = -36.0f;
    float truePeak = -120.0f;
    float elapsedSeconds = 0.0f;
    bool ready = false;
    bool needleInitialised = false;
]=])
sc220_meter_replace_once(_ui "loudness movement state"
    "${_loudness_members_anchor}" "${_loudness_members_replacement}")

set(_timer_loudness_anchor [=[
    loudnessDisplay->setReading (lastStatus.loudnessLufs, lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=])
set(_timer_loudness_replacement [=[
    float liveMomentaryLufs = -120.0f;
    std::uint64_t liveLoudnessGeneration = 0;
    programmeLiveLoudnessTelemetry.read (liveMomentaryLufs, liveLoudnessGeneration);
    loudnessDisplay->setReading (lastStatus.loudnessLufs, liveMomentaryLufs,
                                 lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=])
sc220_meter_replace_once(_ui "feed live momentary loudness to UI"
    "${_timer_loudness_anchor}" "${_timer_loudness_replacement}")

set(_engine_publish_anchor [=[
    loudnessLufs.store (loudnessIntegrator.integratedLufs());
]=])
set(_engine_publish_replacement [=[
    programmeLiveLoudnessTelemetry.publish (loudnessIntegrator.momentaryLufs());
    loudnessLufs.store (loudnessIntegrator.integratedLufs());
]=])
sc220_meter_replace_once(_engine "publish live momentary loudness"
    "${_engine_publish_anchor}" "${_engine_publish_replacement}")

string(REPLACE
    "        loudnessLufs.store (-120.0f);"
    "        loudnessLufs.store (-120.0f);\n        programmeLiveLoudnessTelemetry.reset();"
    _engine "${_engine}")
string(REPLACE
    "    loudnessLufs.store (-120.0f);"
    "    loudnessLufs.store (-120.0f);\n    programmeLiveLoudnessTelemetry.reset();"
    _engine "${_engine}")

foreach(_required IN ITEMS
        "PINK-FLAT BANDS"
        "const float displayDb = rawDb[band]"
        "programmeLiveLoudnessTelemetry.publish"
        "programmeLiveLoudnessTelemetry.read"
        "momentaryLufs()"
        "M \" + juce::String"
        "needleLufs")
    string(FIND "${_ui}${_engine}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 calibrated meter authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "displayDb += 4.0f"
        "displayDb += 2.0f"
        "displayDb -= 1.5f")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete display contour remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live pink-flat analyzer and live loudness meter calibrated")
