# P3 loudness authority: the large dial is a live 400 ms LUFS-M indicator.
# Integrated LUFS-I remains available as the smaller retained programme history.

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p3_loud_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 loudness anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p3_loud_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 P3 loudness range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

# ProgrammeLoudnessIntegrator now exposes the current overlapping 400 ms block.
sc220_p3_loud_once(_engine "publish live 400 ms LUFS-M"
    "    loudnessLufs.store (loudnessIntegrator.integratedLufs());\n"
    "    sc220P3RuntimeState.momentaryLufs.store (loudnessIntegrator.momentaryLufs());\n    loudnessLufs.store (loudnessIntegrator.integratedLufs());\n")

# Any programme reset also clears the live dial immediately.
string(REPLACE
    "        loudnessLufs.store (-120.0f);"
    "        loudnessLufs.store (-120.0f);\n        sc220P3RuntimeState.momentaryLufs.store (-120.0f);"
    _engine "${_engine}")
string(REPLACE
    "    loudnessLufs.store (-120.0f);"
    "    loudnessLufs.store (-120.0f);\n    sc220P3RuntimeState.momentaryLufs.store (-120.0f);"
    _engine "${_engine}")

set(_loudness_class [=[
class SC220MainComponent::LoudnessDisplay final : public juce::Component
{
public:
    void setReading (float nextIntegrated, float nextMomentary, float nextTruePeak,
                     float nextElapsedSeconds, bool nextReady)
    {
        integratedLufs = nextIntegrated;
        momentaryLufs = nextMomentary;
        truePeak = nextTruePeak;
        elapsedSeconds = nextElapsedSeconds;
        ready = nextReady;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        const float diameter = juce::jmin (area.getWidth(), area.getHeight());
        const auto dial = area.withSizeKeepingCentre (diameter, diameter);

        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (dial.translated (0.0f, 4.0f));
        juce::ColourGradient rim (palette::metalHi, dial.getCentreX(), dial.getY(),
                                  palette::metalLo, dial.getCentreX(), dial.getBottom(), false);
        rim.addColour (0.45, juce::Colour (0xffd1d2ce));
        g.setGradientFill (rim);
        g.fillEllipse (dial);
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.drawEllipse (dial, 1.2f);

        const auto face = dial.reduced (6.0f);
        juce::ColourGradient faceFill (juce::Colour (0xff262929), face.getCentreX(), face.getY(),
                                       juce::Colour (0xff0a0b0b), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (faceFill);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (face.reduced (1.0f), 1.0f);

        constexpr float start = juce::MathConstants<float>::pi * 0.72f;
        constexpr float end = juce::MathConstants<float>::pi * 2.28f;
        const bool programmeActive = std::isfinite (momentaryLufs) && momentaryLufs > -70.0f;
        const float normal = programmeActive
            ? juce::jlimit (0.0f, 1.0f, (momentaryLufs + 36.0f) / 30.0f) : 0.0f;
        const float radius = face.getWidth() * 0.44f;
        const auto centre = dial.getCentre();

        for (int i = 0; i < 32; ++i)
        {
            const float t = static_cast<float> (i) / 31.0f;
            const float angle = start + t * (end - start);
            const auto colour = t > 0.83f ? palette::red
                              : t > 0.62f ? palette::amber : palette::green;
            g.setColour (programmeActive && i <= juce::roundToInt (normal * 31.0f)
                         ? colour : colour.withAlpha (0.16f));
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.83f, angle),
                          centre.getPointOnCircumference (radius, angle) }, 4.0f);
        }

        if (programmeActive)
        {
            const float angle = start + normal * (end - start);
            const auto from = centre.getPointOnCircumference (radius * 0.53f, angle);
            const auto to = centre.getPointOnCircumference (radius * 1.02f, angle);
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.drawLine ({ from.translated (1.0f, 1.5f), to.translated (1.0f, 1.5f) }, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.drawLine ({ from, to }, 2.0f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (interFont (dial.getWidth() * 0.20f, true));
        g.drawText (programmeActive ? juce::String (momentaryLufs, 1) : juce::String ("--"),
                    dial.withTrimmedTop (dial.getHeight() * 0.28f)
                        .withHeight (dial.getHeight() * 0.25f),
                    juce::Justification::centred);
        g.setFont (interFont (dial.getWidth() * 0.070f, true));
        g.drawText ("LUFS-M",
                    dial.withTrimmedTop (dial.getHeight() * 0.52f).withHeight (22.0f),
                    juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (interFont (dial.getWidth() * 0.054f));
        const auto integratedText = integratedLufs <= -100.0f
            ? juce::String ("I --") : "I " + juce::String (integratedLufs, 1);
        const auto peakText = truePeak <= -100.0f
            ? juce::String ("TP --") : "TP " + juce::String (truePeak, 1);
        g.drawText (integratedText + "  /  " + peakText,
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);

        g.setColour (palette::orange.withAlpha (0.92f));
        g.setFont (interFont (dial.getWidth() * 0.050f, true));
        g.drawText ("YT / TIKTOK  -14  |  TP <= -1",
                    dial.withTrimmedTop (dial.getHeight() * 0.14f).withHeight (18.0f),
                    juce::Justification::centred);

        if (! programmeActive)
        {
            g.setColour (juce::Colours::white.withAlpha (0.52f));
            g.setFont (interFont (dial.getWidth() * 0.050f));
            g.drawText (elapsedSeconds < 0.4f && ! ready ? "CALIBRATING" : "PROGRAM SILENCE",
                        dial.withTrimmedTop (dial.getHeight() * 0.78f).withHeight (18.0f),
                        juce::Justification::centred);
        }
    }

private:
    float integratedLufs = -120.0f;
    float momentaryLufs = -120.0f;
    float truePeak = -120.0f;
    float elapsedSeconds = 0.0f;
    bool ready = false;
};
]=])
sc220_p3_loud_range(_ui "live LUFS-M dial"
    "class SC220MainComponent::LoudnessDisplay final : public juce::Component"
    "SC220MainComponent::SC220MainComponent"
    "${_loudness_class}")

sc220_p3_loud_once(_ui "feed live and integrated loudness separately"
[=[
    loudnessDisplay->setReading (lastStatus.loudnessLufs, lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=]
[=[
    loudnessDisplay->setReading (lastStatus.loudnessLufs,
                                 sc220P3RuntimeState.momentaryLufs.load(),
                                 lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=])

foreach(_required IN ITEMS
        "loudnessIntegrator.momentaryLufs()"
        "sc220P3RuntimeState.momentaryLufs"
        "LUFS-M"
        "PROGRAM SILENCE"
        "I \" + juce::String")
    string(FIND "${_engine}${_ui}" "${_required}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "SC220 P3 loudness authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P3 loudness: live LUFS-M with retained LUFS-I history")
