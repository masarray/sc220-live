# Final release-facing UX pass. This runs after all audio, routing, analyzer and
# loudness generators so it changes presentation only. The release surface keeps
# operational states but removes engineering/debug copy and duplicate labels.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 release UI refinement requires the generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_release_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 release UI anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_release_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 release UI range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n${_suffix}" PARENT_SCOPE)
endfunction()

# A thinner footer gives the console more useful vertical space and removes the
# oversized empty status strip.
sc220_release_replace_once(_ui "lean footer height"
    "    footerArea = area.removeFromBottom (58);"
    "    footerArea = area.removeFromBottom (42);")

# Reduce repeated scale text while preserving the critical unity, headroom and
# floor references used during live operation.
sc220_release_replace_once(_ui "quiet fader calibration scale"
    "    static constexpr std::array<int, 8> values { 12, 6, 0, -6, -18, -30, -42, -60 };"
    "    static constexpr std::array<int, 6> values { 12, 0, -12, -24, -42, -60 };")
sc220_release_replace_once(_ui "smaller fader scale typography"
    "    g.setFont (interFont (9.5f));"
    "    g.setFont (interFont (8.8f));")

# Remove the technical device debug line from the release surface. Endpoint
# details remain accessible in Audio Setup and error dialogs.
sc220_release_replace_range(_ui "remove active endpoint debug copy"
    "    if (! sc220Detected && lastStatus.inputDevice.isNotEmpty())"
    "    g.setColour (palette::graphite);"
    "    g.setColour (palette::graphite);")

# The title and preset already explain the feature. Remove the internal engine
# subtitle and use the released space to tighten the vertical hierarchy.
set(_signature_subtitle_anchor [=[
    g.setColour (palette::muted);
    g.setFont (interFont (9.5f, true));
    g.drawText ("MAS ARI SIGNATURE ENGINE", signatureArea.getX(), signatureArea.getY() + 42,
                signatureArea.getWidth(), 18, juce::Justification::centred);
]=])
sc220_release_replace_once(_ui "remove internal engine subtitle"
    "${_signature_subtitle_anchor}" "")
sc220_release_replace_once(_ui "raise preset selector"
    "    presetBox.setBounds (signatureArea.getCentreX() - 102, signatureArea.getY() + 72, 204, 36);"
    "    presetBox.setBounds (signatureArea.getCentreX() - 102, signatureArea.getY() + 58, 204, 36);")
sc220_release_replace_once(_ui "raise analyzer panel"
    "    spectrumDisplay->setBounds (signatureArea.reduced (34, 0).withTop (signatureArea.getY() + 128)"
    "    spectrumDisplay->setBounds (signatureArea.reduced (34, 0).withTop (signatureArea.getY() + 112)")
sc220_release_replace_once(_ui "tighten analyzer to macro spacing"
    "    macroRow.setTop (spectrumDisplay->getBottom() + 52);"
    "    macroRow.setTop (spectrumDisplay->getBottom() + 42);")

# Keep the power state visible only when bypassed. The green status dot and
# illuminated power button already communicate the normal active condition.
set(_enhancer_state_anchor [=[
    g.setFont (interFont (9.0f, true));
    g.drawText (active ? "ON" : "BYPASS", enhancerPowerButton.getX() - 44,
                enhancerPowerButton.getY(), 42, enhancerPowerButton.getHeight(),
                juce::Justification::centredLeft);
]=])
set(_enhancer_state_replacement [=[
    if (! active)
    {
        g.setFont (interFont (8.8f, true));
        g.drawText ("BYPASS", enhancerPowerButton.getX() - 52,
                    enhancerPowerButton.getY(), 50, enhancerPowerButton.getHeight(),
                    juce::Justification::centredLeft);
    }
]=])
sc220_release_replace_once(_ui "quiet enhancer active state"
    "${_enhancer_state_anchor}" "${_enhancer_state_replacement}")

# Macro values are already printed above each knob. Ten endpoint labels add
# clutter without helping normal operation.
set(_macro_endpoint_anchor [=[
        g.setColour (palette::graphite);
        g.setFont (interFont (10.0f));
        g.drawText ("0%", knob.getX() - 9, knob.getBottom() - 16, 30, 16,
                    juce::Justification::centredLeft);
        g.drawText ("100%", knob.getRight() - 28, knob.getBottom() - 16, 38, 16,
                    juce::Justification::centredRight);
]=])
sc220_release_replace_once(_ui "remove repeated macro endpoint labels"
    "${_macro_endpoint_anchor}" "")

# Replace engineering-style wording with one calm assurance state.
set(_protect_anchor [=[
    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (signatureArea.getCentreX() - 94),
                   static_cast<float> (signatureArea.getBottom() - 42), 8.0f, 8.0f);
    g.setFont (interFont (9.5f, true));
    g.drawText ("SMART PROTECT  /  ON", signatureArea.getCentreX() - 80,
                signatureArea.getBottom() - 52, 170, 28, juce::Justification::centredLeft);
]=])
set(_protect_replacement [=[
    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (signatureArea.getCentreX() - 55),
                   static_cast<float> (signatureArea.getBottom() - 34), 7.0f, 7.0f);
    g.setFont (interFont (8.8f, true));
    g.drawText ("PROTECTED", signatureArea.getCentreX() - 43,
                signatureArea.getBottom() - 43, 98, 24,
                juce::Justification::centredLeft);
]=])
sc220_release_replace_once(_ui "simplify protection state"
    "${_protect_anchor}" "${_protect_replacement}")

# The top-level channel headings already identify both strips. Remove duplicate
# names underneath the faders and simplify the ducking caption.
set(_duplicate_strip_labels [=[
    g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    g.drawText ("PC AUDIO", pcFader.getX() - 20, pcFader.getBottom() + 2,
                pcFader.getWidth() + 40, 18, juce::Justification::centred);
]=])
sc220_release_replace_once(_ui "remove duplicate strip names"
    "${_duplicate_strip_labels}" "")
string(REPLACE "AUTO DUCK  /  SC220 KEY" "AUTO DUCK" _ui "${_ui}")

# Remove the social-platform target legend and keep the dial focused on its
# actual measurement. LUFS-M and true peak remain as the concise secondary line.
set(_loudness_target_anchor [=[
        g.setColour (palette::orange.withAlpha (0.92f));
        g.setFont (interFont (dial.getWidth() * 0.050f, true));
        g.drawText ("YT / TIKTOK  -14  |  TP <= -1",
                    dial.withTrimmedTop (dial.getHeight() * 0.14f).withHeight (18.0f),
                    juce::Justification::centred);
]=])
sc220_release_replace_once(_ui "remove platform target legend"
    "${_loudness_target_anchor}" "")

# Hide analyzer implementation diagnostics in normal operation. Only a compact
# PAUSED state is shown when telemetry actually stops.
set(_analyzer_status_start [=[
        const bool callbackAlive = lastGeneration > 0 && staleFrames < 6;
]=])
set(_analyzer_status_end [=[
        g.setColour (juce::Colours::white.withAlpha (0.48f));
]=])
set(_analyzer_status_replacement [=[
        const bool callbackAlive = lastGeneration > 0 && staleFrames < 6;
        if (! callbackAlive)
        {
            g.setColour (palette::red.withAlpha (0.78f));
            g.setFont (interFont (8.2f, true));
            g.drawText ("PAUSED",
                        juce::roundToInt (area.getRight() - 94.0f),
                        juce::roundToInt (area.getY() + 6.0f), 60, 16,
                        juce::Justification::centredRight);
        }

        g.setColour (juce::Colours::white.withAlpha (0.48f));
]=])
sc220_release_replace_range(_ui "remove analyzer debug generation and fps text"
    "${_analyzer_status_start}" "${_analyzer_status_end}"
    "${_analyzer_status_replacement}")
string(REPLACE "PROGRAM SILENCE" "NO SIGNAL" _ui "${_ui}")
string(REPLACE "WAITING FOR POST-LIMITER AUDIO" "NO SIGNAL" _ui "${_ui}")

# Output headings and selected endpoints are self-explanatory; the technical
# helper captions underneath duplicate information and consume visual space.
string(REPLACE "VB-CABLE / OBS" "" _ui "${_ui}")
string(REPLACE "HEADPHONES / SPEAKERS" "" _ui "${_ui}")
string(REPLACE "SELECT PHYSICAL OUTPUT" "SELECT MONITOR" _ui "${_ui}")

# Remove the breadcrumb-like signal-chain copy. Keep only compact performance
# diagnostics on the far right for live troubleshooting.
set(_footer_path_anchor [=[
    g.setColour (palette::ink);
    g.setFont (interFont (10.5f, true));

    const auto pathText = "SC220 + PC AUDIO    >    VOICE / MUSIC SIGNATURE    >    VB-CABLE";
    g.drawText (pathText, footerArea.getX() + 42, footerArea.getY(), 560,
                footerArea.getHeight(), juce::Justification::centredLeft);

]=])
set(_footer_path_replacement [=[
    g.setColour (palette::muted);
    g.setFont (interFont (9.2f, true));

]=])
sc220_release_replace_once(_ui "remove footer signal-chain breadcrumb"
    "${_footer_path_anchor}" "${_footer_path_replacement}")

foreach(_forbidden IN ITEMS
        "YT / TIKTOK"
        "TP <= -1"
        "MAS ARI SIGNATURE ENGINE"
        "PINK-FLAT BANDS  #"
        "14 BAND  /  30 FPS"
        "SC220 + PC AUDIO"
        "VOICE / MUSIC SIGNATURE"
        "SMART PROTECT  /  ON"
        "AUTO DUCK  /  SC220 KEY"
        "VB-CABLE / OBS"
        "HEADPHONES / SPEAKERS"
        "ACTIVE: ")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 release UI still contains noisy copy: ${_forbidden}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "PROGRAM ANALYZER"
        "PROTECTED"
        "NO SIGNAL"
        "PLAYBACK CAPTURE ACTIVE"
        "STREAM OUTPUT"
        "MONITOR OUTPUT"
        "MONITOR ACTIVE"
        "LUFS-I")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 release UI lost essential copy: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live premium lean release UI generated")
