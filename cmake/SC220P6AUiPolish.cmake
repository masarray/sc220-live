# P6A UI-only authority from hardware review.
# Preserve P5 audio behaviour and only refine the live console surface:
# - visually centre the supplied SVG fader thumb on the rail
# - use the user-supplied headphones SVG above PC AUDIO
# - lower PC AUDIO title/status for balanced spacing
# - pulse active MUTE buttons so a live operator cannot miss a muted bus

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6A UI polish requires the generated UI source")
endif()

get_filename_component(_p6a_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_p6a_pc_icon "${_p6a_root}/assets/ui/pc_audio_headphones.svg")
if(NOT EXISTS "${_p6a_pc_icon}")
    message(FATAL_ERROR "SC220 P6A PC Audio SVG is missing: ${_p6a_pc_icon}")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${_p6a_pc_icon}" _pc_icon_svg)

function(sc220_p6a_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6A anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# The artwork's optical centre is ~4 px to the right of its viewBox centre.
# Shift only the visible SVG cap; JUCE's native slider hit area/value mapping
# remains exactly centred on the rail.
sc220_p6a_once(_ui "optically centred SVG fader thumb"
[=[
        const auto cap = juce::Rectangle<float> (centreX - 20.0f, sliderPos - 34.0f, 40.0f, 68.0f);
]=]
[=[
        const auto cap = juce::Rectangle<float> (centreX - 16.0f, sliderPos - 34.0f, 40.0f, 68.0f);
]=])

# Replace the temporary Lucide speaker cue with the exact headphones SVG supplied
# for this review. Keep it as a static Drawable so no object is created per frame.
string(CONCAT _pc_audio_heading
[=[
    static constexpr char kSc220PcAudioHeadphonesSvg[] = R"SC220PCAUDIO(]=]
"${_pc_icon_svg}"
[=[)SC220PCAUDIO";
    static auto pcAudioHeadphones = juce::Drawable::createFromImageData (
        kSc220PcAudioHeadphonesSvg, sizeof (kSc220PcAudioHeadphonesSvg) - 1);
    if (pcAudioHeadphones != nullptr)
        pcAudioHeadphones->drawWithin (
            g, juce::Rectangle<float> (static_cast<float> (pcStripArea.getCentreX() - 15),
                                       static_cast<float> (pcStripArea.getY() + 4), 30.0f, 30.0f),
            juce::RectanglePlacement::centred, 1.0f);
    g.setColour (palette::graphite);
    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 37,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
]=])

sc220_p6a_once(_ui "user-supplied PC Audio headphones SVG"
[=[
    static auto pcSpeakerIcon = createLucideIcon ("volume_2_svg", palette::graphite);
    if (pcSpeakerIcon != nullptr)
        pcSpeakerIcon->drawWithin (
            g, juce::Rectangle<float> (static_cast<float> (pcStripArea.getCentreX() - 9),
                                       static_cast<float> (pcStripArea.getY() + 5), 18.0f, 18.0f),
            juce::RectanglePlacement::centred, 1.0f);
    g.setColour (palette::graphite);
    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 26,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
]=]
"${_pc_audio_heading}")

# Lower the playback-capture state together with the heading so icon/title/state
# read as one compact vertical stack and do not visually crowd the top edge.
sc220_p6a_once(_ui "PC Audio status LED vertical alignment"
    "static_cast<float> (pcStripArea.getY() + 70), 8.0f, 8.0f);"
    "static_cast<float> (pcStripArea.getY() + 88), 8.0f, 8.0f);")
sc220_p6a_once(_ui "PC Audio status text vertical alignment"
    "pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63,"
    "pcStripArea.getCentreX() - 58, pcStripArea.getY() + 81,")

# Digital-mixer style safety indication: when a MUTE toggle is active the button
# breathes between deep and bright red and shows a pulsing white status LED.
# Animation is paint-only; no audio/DSP state is touched.
sc220_p6a_once(_ui "active MUTE pulse state"
[=[
        const bool liveButton = button.getButtonText() == "GO LIVE" || button.getButtonText() == "LIVE";
        auto fill = button.getToggleState() ? palette::ink : palette::surface;
        if (liveButton)
            fill = button.getToggleState() ? palette::green : palette::orange;
]=]
[=[
        const bool liveButton = button.getButtonText() == "GO LIVE" || button.getButtonText() == "LIVE";
        const bool muteButton = button.getButtonText() == "MUTE";
        const bool muteActive = muteButton && button.getToggleState();
        const auto mutePhase = static_cast<float> (
            std::fmod (juce::Time::getMillisecondCounterHiRes(), 900.0) / 900.0);
        const float mutePulse = 0.5f - 0.5f * std::cos (
            juce::MathConstants<float>::twoPi * mutePhase);

        auto fill = button.getToggleState() ? palette::ink : palette::surface;
        if (liveButton)
            fill = button.getToggleState() ? palette::green : palette::orange;
        if (muteActive)
            fill = palette::red.darker (0.18f)
                       .interpolatedWith (palette::red.brighter (0.32f), mutePulse);
]=])

sc220_p6a_once(_ui "active MUTE pulse ring and LED"
[=[
        g.setColour (button.getToggleState() || liveButton ? juce::Colours::transparentBlack
                                                           : palette::border);
        g.drawRoundedRectangle (area, 5.0f, 1.0f);
]=]
[=[
        g.setColour (button.getToggleState() || liveButton ? juce::Colours::transparentBlack
                                                           : palette::border);
        g.drawRoundedRectangle (area, 5.0f, 1.0f);
        if (muteActive)
        {
            g.setColour (palette::red.withAlpha (0.30f + 0.40f * mutePulse));
            g.drawRoundedRectangle (area.expanded (1.4f), 6.0f, 1.4f + 0.8f * mutePulse);
            g.setColour (juce::Colours::white.withAlpha (0.52f + 0.48f * mutePulse));
            g.fillEllipse (area.getX() + 7.0f, area.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
]=])

# Parent repaint does not guarantee child Button paint invalidation on every JUCE
# backend, so explicitly refresh the two MUTE buttons at the existing 30 Hz UI
# timer. This is UI-only and allocation free.
sc220_p6a_once(_ui "MUTE animation repaint"
[=[
    streamMeter->setLevels (lastStatus.outputPeakDbL, lastStatus.outputPeakDbR);
]=]
[=[
    streamMeter->setLevels (lastStatus.outputPeakDbL, lastStatus.outputPeakDbR);
    sc220Mute.repaint();
    streamMute.repaint();
]=])

foreach(_required IN ITEMS
        "centreX - 16.0f"
        "kSc220PcAudioHeadphonesSvg"
        "pcStripArea.getY() + 37"
        "pcStripArea.getY() + 88"
        "pcStripArea.getY() + 81"
        "muteActive"
        "mutePulse"
        "sc220Mute.repaint()"
        "streamMute.repaint()")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6A required UI authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "createLucideIcon (\"volume_2_svg\""
        "centreX - 20.0f, sliderPos - 34.0f")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6A stale UI implementation remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P6A: centred faders + supplied PC Audio SVG + blinking MUTE safety UI")
