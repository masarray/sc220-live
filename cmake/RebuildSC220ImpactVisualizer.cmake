# Restore the proven responsive SC220 programme visualizer without the old
# ScopeTap/FFT queue lifecycle. The exact post-limiter stereo buffer feeds a
# preallocated 14-band atomic filter bank; the UI only reads published levels.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 responsive visualizer requires final UI and engine sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_visual_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 responsive visualizer anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_visual_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 responsive visualizer range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

sc220_visual_replace_once(_ui "UI telemetry include"
    "#include \"SC220MainComponent.h\"\n"
    "#include \"SC220MainComponent.h\"\n#include \"ProgrammeBandTelemetry.h\"\n")
sc220_visual_replace_once(_engine "engine telemetry include"
    "#include \"LiveAudioEngine.h\"\n"
    "#include \"LiveAudioEngine.h\"\n#include \"ProgrammeBandTelemetry.h\"\n")

set(_display_start "class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component")
set(_display_end "class SC220MainComponent::LoudnessDisplay final : public juce::Component")
set(_display_replacement [=[
class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component
{
public:
    LiveSpectrumDisplay()
    {
        levels.fill (0.0f);
        pulses.fill (0.0f);
        heldPeaks.fill (0.0f);
        peakHoldFrames.fill (0);
    }

    void updateFromTelemetry()
    {
        std::array<float, bandCount> rawDb {};
        std::array<float, bandCount> activity {};
        std::uint64_t generation = 0;
        const bool available = programmeBandTelemetry.read (rawDb, generation);
        programmeBandTelemetry.readActivity (activity);

        if (! available)
        {
            for (size_t band = 0; band < bandCount; ++band)
            {
                levels[band] *= 0.78f;
                pulses[band] *= 0.55f;
                heldPeaks[band] = juce::jmax (levels[band], heldPeaks[band] - 0.040f);
            }
            hasSignal = false;
            repaint();
            return;
        }

        lastGeneration = generation;
        float strongestDb = -120.0f;
        for (size_t band = 0; band < bandCount; ++band)
        {
            const float displayDb = rawDb[band];
            strongestDb = juce::jmax (strongestDb, displayDb);
            const float base = juce::jlimit (0.0f, 1.0f, (displayDb + 66.0f) / 52.0f);
            const float rhythm = juce::jlimit (0.0f, 1.0f, activity[band]);
            const float target = juce::jlimit (0.0f, 1.0f, base + rhythm * 0.22f);
            const float previous = levels[band];
            const float bandPosition = static_cast<float> (band)
                                     / static_cast<float> (bandCount - 1);
            const float releaseMix = 0.16f + 0.12f * bandPosition;

            // Fast attack / fast musical release: preserve the earlier lively
            // visual response without adding work to the audio callback.
            if (target > previous)
                levels[band] = previous + (target - previous) * 0.94f;
            else
                levels[band] = previous * (1.0f - releaseMix) + target * releaseMix;

            const float rise = juce::jmax (0.0f, target - previous);
            pulses[band] = juce::jmax (pulses[band] * 0.58f,
                juce::jmax (rhythm, juce::jmin (1.0f, rise * 3.8f)));

            if (levels[band] >= heldPeaks[band])
            {
                heldPeaks[band] = levels[band];
                peakHoldFrames[band] = 9;
            }
            else if (peakHoldFrames[band] > 0)
            {
                --peakHoldFrames[band];
            }
            else
            {
                heldPeaks[band] = juce::jmax (levels[band], heldPeaks[band] - 0.040f);
            }
        }

        hasSignal = strongestDb > -78.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::ink);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colour (0xff3d4040));
        g.drawRoundedRectangle (area, 7.0f, 1.0f);

        auto graph = area.reduced (34.0f, 26.0f);
        graph.removeFromBottom (15.0f);
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        for (int line = 0; line <= 4; ++line)
        {
            const float y = graph.getBottom()
                - graph.getHeight() * static_cast<float> (line) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        constexpr int segments = 16;
        constexpr float gap = 5.0f;
        constexpr float segmentGap = 1.7f;
        const float barWidth = (graph.getWidth() - gap * static_cast<float> (bandCount - 1))
                             / static_cast<float> (bandCount);
        const float segmentHeight = (graph.getHeight() - segmentGap * (segments - 1))
                                  / static_cast<float> (segments);

        for (size_t band = 0; band < bandCount; ++band)
        {
            const float x = graph.getX() + static_cast<float> (band) * (barWidth + gap);
            const int lit = juce::jlimit (0, segments,
                juce::roundToInt (levels[band] * static_cast<float> (segments)));
            for (int segment = 0; segment < segments; ++segment)
            {
                const float y = graph.getBottom()
                    - static_cast<float> (segment + 1) * segmentHeight
                    - static_cast<float> (segment) * segmentGap;
                const auto cell = juce::Rectangle<float> (x, y, barWidth, segmentHeight);
                const float h = static_cast<float> (segment) / static_cast<float> (segments - 1);
                const auto active = h > 0.84f ? palette::red
                                  : h > 0.65f ? palette::amber : palette::green;
                g.setColour (segment < lit ? active.withAlpha (0.94f)
                                           : active.withAlpha (0.075f));
                g.fillRoundedRectangle (cell, juce::jmin (1.7f, barWidth * 0.28f));
            }

            if (lit > 0 && pulses[band] > 0.04f)
            {
                const float transientY = graph.getBottom() - levels[band] * graph.getHeight();
                g.setColour (juce::Colours::white.withAlpha (0.18f + pulses[band] * 0.48f));
                g.fillRoundedRectangle (x, transientY - 1.5f, barWidth, 2.5f, 1.0f);
            }

            if (heldPeaks[band] > 0.025f)
            {
                const float peakY = graph.getBottom() - heldPeaks[band] * graph.getHeight();
                g.setColour (palette::amber.withAlpha (0.90f));
                g.fillRoundedRectangle (x, peakY - 1.0f, barWidth, 2.0f, 1.0f);
            }
        }

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (interFont (9.5f, true));
        g.drawText ("POST-MASTER ANALYZER", juce::roundToInt (area.getX() + 34.0f),
                    juce::roundToInt (area.getY() + 6.0f), 190, 16,
                    juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.setFont (interFont (8.3f));
        g.drawText ("14 BAND  /  30 FPS", juce::roundToInt (area.getRight() - 170.0f),
                    juce::roundToInt (area.getY() + 6.0f), 136, 16,
                    juce::Justification::centredRight);

        const std::array<std::pair<size_t, const char*>, 7> labels {{
            { 0, "50" }, { 2, "125" }, { 4, "315" }, { 6, "800" },
            { 8, "2k" }, { 10, "5k" }, { 12, "12k" }
        }};
        g.setColour (juce::Colours::white.withAlpha (0.48f));
        g.setFont (interFont (8.0f));
        const int labelY = juce::roundToInt (graph.getBottom() + 4.0f);
        for (const auto& label : labels)
        {
            const float x = graph.getX() + static_cast<float> (label.first) * (barWidth + gap)
                          + barWidth * 0.5f;
            g.drawText (label.second, juce::roundToInt (x) - 20, labelY, 40, 14,
                        juce::Justification::centred);
        }

        if (! hasSignal)
        {
            g.setColour (juce::Colours::white.withAlpha (0.42f));
            g.setFont (interFont (10.5f, true));
            g.drawText (lastGeneration > 0 ? "PROGRAM SILENCE" : "WAITING FOR PROGRAM AUDIO",
                        graph.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    static constexpr size_t bandCount = ProgrammeBandTelemetry::bandCount;
    std::array<float, bandCount> levels {};
    std::array<float, bandCount> pulses {};
    std::array<float, bandCount> heldPeaks {};
    std::array<int, bandCount> peakHoldFrames {};
    std::uint64_t lastGeneration = 0;
    bool hasSignal = false;
};
]=])
sc220_visual_replace_range(_ui "responsive atomic band display"
    "${_display_start}" "${_display_end}" "${_display_replacement}")

set(_timer_anchor [=[
    static int spectrumDivider = 0;
    if (++spectrumDivider >= 2)
    {
        spectrumDivider = 0;
        audioEngine.setProgrammeAnalyzerEnabled (uiActive);
        spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                        lastStatus.sampleRate);
    }
]=])
sc220_visual_replace_once(_ui "30 fps atomic telemetry read"
    "${_timer_anchor}" "    spectrumDisplay->updateFromTelemetry();\n")

set(_publish_anchor [=[
    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);
]=])
set(_publish_replacement [=[
    programmeBandTelemetry.process (voiceBlock.getReadPointer (0),
                                    voiceBlock.getReadPointer (1), numSamples);
]=])
sc220_visual_replace_once(_engine "post-limiter atomic band publish"
    "${_publish_anchor}" "${_publish_replacement}")

# Prepare telemetry on every audio-device start. ScopeTap is no longer part of
# the programme visual path and remains disabled.
sc220_visual_replace_once(_engine "prepare atomic telemetry"
    "    juce::dsp::ProcessSpec masterSpec;\n"
    "    programmeAnalyzer.setEnabled (false);\n    programmeBandTelemetry.prepare (sampleRate);\n\n    juce::dsp::ProcessSpec masterSpec;\n")

foreach(_obsolete_ui IN ITEMS
        "    audioEngine.setProgrammeAnalyzerEnabled (true);\n"
        "    audioEngine.setProgrammeAnalyzerEnabled (false);\n"
        "    audioEngine.setProgrammeAnalyzerEnabled (active);\n")
    string(REPLACE "${_obsolete_ui}" "" _ui "${_ui}")
endforeach()

foreach(_required IN ITEMS
        "ProgrammeBandTelemetry.h"
        "programmeBandTelemetry.prepare"
        "programmeBandTelemetry.process"
        "programmeBandTelemetry.read"
        "programmeBandTelemetry.readActivity"
        "updateFromTelemetry"
        "14 BAND  /  30 FPS"
        "positiveRise")
    # positiveRise existed in the old implementation; this restored version uses
    # the equivalent shorter name `rise`.
endforeach()

foreach(_required IN ITEMS
        "ProgrammeBandTelemetry.h"
        "programmeBandTelemetry.prepare"
        "programmeBandTelemetry.process"
        "programmeBandTelemetry.read"
        "programmeBandTelemetry.readActivity"
        "updateFromTelemetry"
        "14 BAND  /  30 FPS"
        "const float rise")
    string(FIND "${_ui}${_engine}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 responsive visualizer authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "performFrequencyOnlyForwardTransform"
        "multiplyWithWindowingTable"
        "programmeTap.consumeLatest"
        "updateFromTap (audioEngine.programmeAnalyzerTap()")
    string(FIND "${_ui}${_engine}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete queued FFT visualizer remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live responsive 14-band post-limiter visualizer restored")
