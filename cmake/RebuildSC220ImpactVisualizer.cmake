# Build the SC220 programme visualizer from zero.
#
# The old ScopeTap, FFT, snapshot, direct-capture, and recovery paths are not
# used. The exact post-limiter stereo buffer is measured by fourteen fixed
# band-pass biquads inside ProgrammeBandTelemetry. Only atomic dB values cross
# into the UI. There is no sample queue and therefore no empty-frame lifecycle.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 atomic band visualizer requires final UI and engine sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_bands_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 atomic band range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

function(sc220_bands_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 atomic band anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_ui_include_anchor [=[
#include "SC220MainComponent.h"
]=])
set(_ui_include_replacement [=[
#include "SC220MainComponent.h"
#include "ProgrammeBandTelemetry.h"
]=])
sc220_bands_replace_once(_ui "UI band telemetry include"
    "${_ui_include_anchor}" "${_ui_include_replacement}")

set(_engine_include_anchor [=[
#include "LiveAudioEngine.h"
]=])
set(_engine_include_replacement [=[
#include "LiveAudioEngine.h"
#include "ProgrammeBandTelemetry.h"
]=])
sc220_bands_replace_once(_engine "engine band telemetry include"
    "${_engine_include_anchor}" "${_engine_include_replacement}")

set(_display_start "class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component")
set(_display_end "class SC220MainComponent::LoudnessDisplay final : public juce::Component")
set(_display_replacement [=[
class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component
{
public:
    LiveSpectrumDisplay()
    {
        levels.fill (0.0f);
        heldPeaks.fill (0.0f);
        peakHoldFrames.fill (0);
    }

    void updateFromTelemetry()
    {
        std::array<float, bandCount> rawDb {};
        std::uint64_t generation = 0;
        const bool available = programmeBandTelemetry.read (rawDb, generation);

        if (! available)
        {
            ++staleFrames;
            decayWithoutCallback();
            repaint();
            return;
        }

        if (generation == lastGeneration)
            ++staleFrames;
        else
        {
            lastGeneration = generation;
            staleFrames = 0;
        }

        float strongestDb = -120.0f;
        for (size_t band = 0; band < bandCount; ++band)
        {
            // Display-only contour. The audio is never modified.
            float displayDb = rawDb[band];
            if (band <= 2)
                displayDb += 4.0f;
            else if (band <= 5)
                displayDb += 2.0f;
            else if (band >= 12)
                displayDb -= 1.5f;

            strongestDb = juce::jmax (strongestDb, displayDb);
            const float target = normaliseDb (displayDb, -72.0f);
            if (target > levels[band])
                levels[band] += (target - levels[band]) * 0.76f;
            else
                levels[band] = levels[band] * 0.84f + target * 0.16f;

            if (levels[band] >= heldPeaks[band])
            {
                heldPeaks[band] = levels[band];
                peakHoldFrames[band] = 18; // 600 ms at 30 Hz.
            }
            else if (peakHoldFrames[band] > 0)
            {
                --peakHoldFrames[band];
            }
            else
            {
                heldPeaks[band] = juce::jmax (levels[band],
                                              heldPeaks[band] - 0.024f);
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

        auto graph = area.reduced (34.0f, 27.0f);
        graph.removeFromBottom (17.0f);

        g.setColour (juce::Colours::white.withAlpha (0.060f));
        for (int line = 0; line <= 4; ++line)
        {
            const float y = graph.getBottom()
                - graph.getHeight() * static_cast<float> (line) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        constexpr int segmentCount = 14;
        constexpr float barGap = 6.0f;
        constexpr float segmentGap = 2.0f;
        const float barWidth = (graph.getWidth()
            - barGap * static_cast<float> (bandCount - 1))
            / static_cast<float> (bandCount);
        const float segmentHeight = (graph.getHeight()
            - segmentGap * static_cast<float> (segmentCount - 1))
            / static_cast<float> (segmentCount);

        for (size_t band = 0; band < bandCount; ++band)
        {
            const float x = graph.getX()
                + static_cast<float> (band) * (barWidth + barGap);
            const int litSegments = juce::jlimit (0, segmentCount,
                juce::roundToInt (levels[band] * static_cast<float> (segmentCount)));

            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const float y = graph.getBottom()
                    - static_cast<float> (segment + 1) * segmentHeight
                    - static_cast<float> (segment) * segmentGap;
                const auto cell = juce::Rectangle<float> (x, y, barWidth, segmentHeight);
                const float heightNormal = static_cast<float> (segment)
                    / static_cast<float> (segmentCount - 1);
                const auto activeColour = heightNormal > 0.84f ? palette::red
                                        : heightNormal > 0.65f ? palette::amber
                                                               : palette::green;
                g.setColour (segment < litSegments
                    ? activeColour.withAlpha (0.96f)
                    : activeColour.withAlpha (0.095f));
                g.fillRoundedRectangle (cell, juce::jmin (1.8f, barWidth * 0.28f));
            }

            if (heldPeaks[band] > 0.025f)
            {
                const float peakY = graph.getBottom()
                    - heldPeaks[band] * graph.getHeight();
                g.setColour (palette::amber.withAlpha (0.96f));
                g.fillRoundedRectangle (x, peakY - 1.0f, barWidth, 2.0f,
                                        juce::jmin (1.0f, barWidth * 0.25f));
            }
        }

        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.setFont (interFont (9.5f, true));
        g.drawText ("PROGRAM ANALYZER",
                    juce::roundToInt (area.getX() + 34.0f),
                    juce::roundToInt (area.getY() + 6.0f), 190, 16,
                    juce::Justification::centredLeft);

        const bool callbackAlive = lastGeneration > 0 && staleFrames < 6;
        g.setColour (callbackAlive ? palette::green.withAlpha (0.84f)
                                   : palette::red.withAlpha (0.84f));
        g.setFont (interFont (8.3f, true));
        const auto status = callbackAlive
            ? "POST-LIMITER BANDS  #" + juce::String (lastGeneration)
            : "WAITING FOR ENGINE BANDS";
        g.drawFittedText (status + "  /  14 BAND  /  30 FPS",
                          juce::roundToInt (area.getRight() - 300.0f),
                          juce::roundToInt (area.getY() + 6.0f), 266, 16,
                          juce::Justification::centredRight, 1);

        g.setColour (juce::Colours::white.withAlpha (0.48f));
        g.setFont (interFont (8.0f));
        const std::array<std::pair<size_t, const char*>, 7> labels {{
            { 0, "50" }, { 2, "125" }, { 4, "315" }, { 6, "800" },
            { 8, "2k" }, { 10, "5k" }, { 12, "12k" }
        }};
        const int labelY = juce::roundToInt (graph.getBottom() + 5.0f);
        for (const auto& label : labels)
        {
            const float x = graph.getX()
                + static_cast<float> (label.first) * (barWidth + barGap)
                + barWidth * 0.5f;
            g.drawText (label.second, juce::roundToInt (x) - 22,
                        labelY, 44, 14, juce::Justification::centred);
        }

        if (! hasSignal)
        {
            g.setColour (juce::Colours::white.withAlpha (0.44f));
            g.setFont (interFont (11.0f, true));
            g.drawText (callbackAlive ? "PROGRAM SILENCE"
                                      : "WAITING FOR POST-LIMITER AUDIO",
                        graph.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    void decayWithoutCallback()
    {
        for (size_t band = 0; band < bandCount; ++band)
        {
            levels[band] *= 0.88f;
            if (peakHoldFrames[band] > 0)
                --peakHoldFrames[band];
            else
                heldPeaks[band] = juce::jmax (levels[band],
                                              heldPeaks[band] - 0.024f);
        }
        hasSignal = std::any_of (levels.begin(), levels.end(),
            [] (float value) { return value > 0.018f; });
    }

    static constexpr size_t bandCount = ProgrammeBandTelemetry::bandCount;
    std::array<float, bandCount> levels {};
    std::array<float, bandCount> heldPeaks {};
    std::array<int, bandCount> peakHoldFrames {};
    std::uint64_t lastGeneration = 0;
    int staleFrames = 0;
    bool hasSignal = false;
};
]=])
sc220_bands_replace_range(_ui "new atomic filter-bank visualizer"
    "${_display_start}" "${_display_end}" "${_display_replacement}")

set(_timer_anchor [=[
    static int spectrumDivider = 0;
    if (++spectrumDivider >= 2)
    {
        spectrumDivider = 0;
        spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                        lastStatus.sampleRate);
    }
]=])
set(_timer_replacement [=[
    spectrumDisplay->updateFromTelemetry();
]=])
sc220_bands_replace_once(_ui "drive UI from atomic band values"
    "${_timer_anchor}" "${_timer_replacement}")

set(_publish_anchor [=[
    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);
]=])
set(_publish_replacement [=[
    programmeBandTelemetry.process (voiceBlock.getReadPointer (0),
                                    voiceBlock.getReadPointer (1), numSamples);
]=])
sc220_bands_replace_once(_engine "measure exact post-limiter programme buffer"
    "${_publish_anchor}" "${_publish_replacement}")

set(_prepare_anchor [=[
    programmeAnalyzer.setEnabled (true);

    juce::dsp::ProcessSpec masterSpec;
]=])
set(_prepare_replacement [=[
    programmeAnalyzer.setEnabled (false);
    programmeBandTelemetry.prepare (sampleRate);

    juce::dsp::ProcessSpec masterSpec;
]=])
sc220_bands_replace_once(_engine "prepare fixed band filters on device start"
    "${_prepare_anchor}" "${_prepare_replacement}")

# Remove all UI lifecycle manipulation belonging to the deleted tap visualizer.
foreach(_obsolete_line IN ITEMS
        "    audioEngine.setProgrammeAnalyzerEnabled (true);\n"
        "    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);\n"
        "    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);\n")
    string(REPLACE "${_obsolete_line}" "" _ui "${_ui}")
endforeach()

foreach(_required IN ITEMS
        "ProgrammeBandTelemetry.h"
        "programmeBandTelemetry.prepare"
        "programmeBandTelemetry.process"
        "programmeBandTelemetry.read"
        "POST-LIMITER BANDS"
        "PROGRAM ANALYZER"
        "14 BAND"
        "segmentCount = 14"
        "peakHoldFrames[band] = 18")
    string(FIND "${_ui}${_engine}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 atomic band authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "ProgrammeSpectrumSnapshot"
        "programmeSpectrumSnapshot"
        "performFrequencyOnlyForwardTransform"
        "multiplyWithWindowingTable"
        "updateFromSnapshot"
        "updateFromTaps"
        "programmeTap.consumeLatest"
        "musicTap.consumeLatest"
        "voiceTap.consumeLatest"
        "PcAudioLoopbackCapture::consumeLatestVisual"
        "MUSIC RECOVERY"
        "VOICE RECOVERY"
        "PC CAPTURE"
        "NO FRAMES"
        "PROGRAM IMPACT"
        "PC INPUT IMPACT"
        "OLDER"
        "NOW"
        "historySize = 32")
    string(FIND "${_ui}${_engine}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 deleted visualizer path remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live atomic post-limiter filter-bank visualizer generated")
