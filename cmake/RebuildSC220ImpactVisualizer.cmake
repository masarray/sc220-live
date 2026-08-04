# Replace the fragile ScopeTap/FFT display with a deterministic OBS-style
# programme impact meter. The UI is driven by the same live peak telemetry as
# the visible PC and Stream meters, so it cannot remain frozen while those
# meters are moving. No FFT, allocation, lock, or audio-callback work is added.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 impact visualizer requires the final generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_impact_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 impact visualizer range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

function(sc220_impact_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 impact visualizer anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_display_start "class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component")
set(_display_end "class SC220MainComponent::LoudnessDisplay final : public juce::Component")
set(_display_replacement [=[
class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component
{
public:
    void updateFromLevels (float programmeLeftDb, float programmeRightDb,
                           float pcLeftDb, float pcRightDb,
                           bool pcCaptureRunning)
    {
        const float programmeDb = juce::jmax (programmeLeftDb, programmeRightDb);
        const float pcDb = juce::jmax (pcLeftDb, pcRightDb);
        const bool programmeLive = std::isfinite (programmeDb) && programmeDb > -78.0f;
        const bool pcLive = pcCaptureRunning && std::isfinite (pcDb) && pcDb > -78.0f;

        source = programmeLive ? Source::programme
                               : (pcLive ? Source::pcInput : Source::none);
        const float selectedDb = source == Source::programme ? programmeDb
                               : source == Source::pcInput ? pcDb : -120.0f;
        const float target = juce::jlimit (0.0f, 1.0f,
            (selectedDb + 72.0f) / 72.0f);

        // Fast attack keeps kicks satisfying; slower release avoids nervous
        // flicker. This runs on the 30 Hz message timer, never the audio thread.
        if (target > envelope)
            envelope += (target - envelope) * 0.82f;
        else
            envelope = envelope * 0.87f + target * 0.13f;

        std::rotate (history.begin(), history.begin() + 1, history.end());
        history.back() = envelope;

        if (envelope >= heldPeak)
        {
            heldPeak = envelope;
            peakHoldFrames = 15; // 500 ms at the 30 Hz UI rate.
        }
        else if (peakHoldFrames > 0)
        {
            --peakHoldFrames;
        }
        else
        {
            heldPeak = juce::jmax (envelope, heldPeak - 0.025f);
        }

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::ink);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colour (0xff3d4040));
        g.drawRoundedRectangle (area, 7.0f, 1.0f);

        auto graph = area.reduced (34.0f, 28.0f);
        graph.removeFromBottom (12.0f);

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (int line = 0; line <= 4; ++line)
        {
            const float y = graph.getBottom()
                - graph.getHeight() * static_cast<float> (line) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        constexpr float gap = 4.0f;
        const float barWidth = (graph.getWidth()
            - gap * static_cast<float> (history.size() - 1))
            / static_cast<float> (history.size());
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float level = juce::jlimit (0.0f, 1.0f, history[i]);
            const float x = graph.getX() + static_cast<float> (i) * (barWidth + gap);
            const float height = juce::jmax (2.0f, level * graph.getHeight());
            const auto inactive = juce::Rectangle<float> (x, graph.getY(),
                                                           barWidth, graph.getHeight());
            const auto active = juce::Rectangle<float> (x, graph.getBottom() - height,
                                                         barWidth, height);
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRoundedRectangle (inactive, juce::jmin (2.0f, barWidth * 0.35f));

            const float age = static_cast<float> (i)
                            / static_cast<float> (history.size() - 1);
            auto colour = level > 0.88f ? palette::orange
                        : level > 0.70f ? palette::amber
                                       : palette::green;
            g.setColour (colour.withMultipliedAlpha (0.38f + age * 0.62f));
            g.fillRoundedRectangle (active, juce::jmin (2.0f, barWidth * 0.35f));
        }

        if (heldPeak > 0.01f)
        {
            const float peakY = graph.getBottom()
                - juce::jlimit (0.0f, 1.0f, heldPeak) * graph.getHeight();
            g.setColour (palette::amber.withAlpha (0.92f));
            g.drawHorizontalLine (juce::roundToInt (peakY),
                                  graph.getX(), graph.getRight());
            g.setFont (interFont (8.0f, true));
            g.drawText ("PEAK", juce::roundToInt (graph.getRight() - 38.0f),
                        juce::roundToInt (peakY - 14.0f), 38, 13,
                        juce::Justification::centredRight);
        }

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (interFont (9.5f, true));
        g.drawText (source == Source::pcInput ? "PC INPUT IMPACT" : "PROGRAM IMPACT",
                    juce::roundToInt (area.getX() + 34.0f),
                    juce::roundToInt (area.getY() + 6.0f), 190, 16,
                    juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.setFont (interFont (8.5f));
        g.drawText ("30 FPS  /  500 ms PEAK HOLD",
                    juce::roundToInt (area.getRight() - 220.0f),
                    juce::roundToInt (area.getY() + 6.0f), 186, 16,
                    juce::Justification::centredRight);

        g.setColour (juce::Colours::white.withAlpha (0.48f));
        g.setFont (interFont (8.5f));
        g.drawText ("OLDER", juce::roundToInt (graph.getX()),
                    juce::roundToInt (graph.getBottom() + 5.0f), 50, 14,
                    juce::Justification::centredLeft);
        g.drawText ("NOW", juce::roundToInt (graph.getRight() - 50.0f),
                    juce::roundToInt (graph.getBottom() + 5.0f), 50, 14,
                    juce::Justification::centredRight);

        if (source == Source::none && envelope < 0.012f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.44f));
            g.setFont (interFont (11.0f, true));
            g.drawText ("WAITING FOR LIVE AUDIO", graph.toNearestInt(),
                        juce::Justification::centred);
        }
    }

private:
    enum class Source { none, programme, pcInput };
    static constexpr size_t historySize = 32;
    std::array<float, historySize> history {};
    float envelope = 0.0f;
    float heldPeak = 0.0f;
    int peakHoldFrames = 0;
    Source source = Source::none;
};
]=])
sc220_impact_replace_range(_ui "replace FFT display with programme impact meter"
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
    spectrumDisplay->updateFromLevels (lastStatus.outputPeakDbL,
                                       lastStatus.outputPeakDbR,
                                       lastStatus.pcPeakDbL,
                                       lastStatus.pcPeakDbR,
                                       lastStatus.pcLoopbackRunning);
]=])
sc220_impact_replace_once(_ui "drive impact display from live meter telemetry"
    "${_timer_anchor}" "${_timer_replacement}")

foreach(_required IN ITEMS
        "void updateFromLevels"
        "PROGRAM IMPACT"
        "PC INPUT IMPACT"
        "historySize = 32"
        "peakHoldFrames = 15"
        "500 ms PEAK HOLD"
        "lastStatus.outputPeakDbL"
        "lastStatus.pcPeakDbL")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 impact visualizer authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "updateFromTap (audioEngine.programmeAnalyzerTap()"
        "performFrequencyOnlyForwardTransform"
        "32 BAND  /  15 FPS"
        "NO PROGRAM AUDIO")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete FFT visualizer remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live OBS-style programme impact visualizer generated")
