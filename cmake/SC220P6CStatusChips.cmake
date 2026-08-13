# P6C UI status-chip polish.
# Keep P6B audio/routing behaviour unchanged and make the two important DSP
# states legible at a glance like a modern digital-mixer console.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6C status chips require the generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p6c_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6C anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Replace the tiny status dot/text with one compact illuminated status chip.
# Green = DSP processing active, amber = explicit bypass. The subtle slow pulse
# is paint-only and uses the existing UI refresh cadence.
sc220_p6c_once(_ui "DSP ON/BYPASS illuminated chip"
[=[
    g.setColour (active ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 96),
                   static_cast<float> (enhancerPowerButton.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setFont (interFont (9.0f, true));
    g.drawText (active ? "DSP ON" : "DSP BYPASS", enhancerPowerButton.getX() - 82,
                enhancerPowerButton.getY(), 78, enhancerPowerButton.getHeight(),
                juce::Justification::centredLeft);
]=]
[=[
    const auto dspChipColour = active ? palette::green : palette::amber;
    const auto dspPhase = static_cast<float> (
        std::fmod (juce::Time::getMillisecondCounterHiRes(), 1800.0) / 1800.0);
    const float dspGlow = 0.5f - 0.5f * std::cos (
        juce::MathConstants<float>::twoPi * dspPhase);
    const auto drawIlluminatedPad = [&g] (juce::Rectangle<float> bounds,
                                         juce::Colour lampColour,
                                         float glow)
    {
        const auto bezel = bounds.expanded (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (bezel, 6.0f);

        g.setColour (lampColour.withAlpha (0.10f + 0.08f * glow));
        g.fillRoundedRectangle (bounds.expanded (4.0f), 8.0f);

        g.setGradientFill (juce::ColourGradient (
            lampColour.brighter (0.38f).withAlpha (0.98f),
            bounds.getCentreX(), bounds.getY() + 1.0f,
            lampColour.darker (0.24f).withAlpha (0.98f),
            bounds.getCentreX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, 4.5f);

        g.setColour (juce::Colours::white.withAlpha (0.30f + 0.10f * glow));
        g.drawRoundedRectangle (bounds.reduced (0.75f), 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds.reduced (4.0f, 3.0f).withHeight (3.0f), 1.5f);
    };
    const float dspPadHeight = static_cast<float> (enhancerPowerButton.getHeight());
    const auto dspChip = juce::Rectangle<float> (
        static_cast<float> (enhancerPowerButton.getX() - 116),
        static_cast<float> (enhancerPowerButton.getY()),
        100.0f, dspPadHeight);
    drawIlluminatedPad (dspChip, dspChipColour, dspGlow);
    g.setColour (juce::Colours::white);
    g.setFont (interFont (9.2f, true));
    g.drawFittedText (active ? "DSP ON" : "DSP BYPASS", dspChip.toNearestInt().reduced (9, 4),
                      juce::Justification::centred, 1);
]=])

# Smart Protect is always active in this console path. Present it as the same
# illuminated hardware-style status language rather than a loose dot + text.
sc220_p6c_once(_ui "Smart Protect illuminated chip"
[=[
    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (signatureArea.getCentreX() - 94),
                   static_cast<float> (signatureArea.getBottom() - 42), 8.0f, 8.0f);
    g.setFont (interFont (9.5f, true));
    g.drawText ("SMART PROTECT  /  ON", signatureArea.getCentreX() - 80,
                signatureArea.getBottom() - 52, 170, 28, juce::Justification::centredLeft);
]=]
[=[
    const auto protectPhase = static_cast<float> (
        std::fmod (juce::Time::getMillisecondCounterHiRes() + 310.0, 2200.0) / 2200.0);
    const float protectGlow = 0.5f - 0.5f * std::cos (
        juce::MathConstants<float>::twoPi * protectPhase);
    const float protectPadHeight = static_cast<float> (enhancerPowerButton.getHeight());
    const auto protectChip = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getCentreX() - 82),
        static_cast<float> (signatureArea.getBottom() - 54),
        164.0f, protectPadHeight);
    drawIlluminatedPad (protectChip, palette::green, protectGlow);
    g.setColour (juce::Colours::white);
    g.setFont (interFont (9.4f, true));
    g.drawFittedText ("SMART PROTECT   ON", protectChip.toNearestInt().reduced (12, 4),
                      juce::Justification::centred, 1);
]=])

foreach(_required IN ITEMS
        "const auto dspChipColour = active ? palette::green : palette::amber"
        "const auto drawIlluminatedPad"
        "dspPadHeight = static_cast<float> (enhancerPowerButton.getHeight())"
        "active ? \"DSP ON\" : \"DSP BYPASS\""
        "drawIlluminatedPad (dspChip"
        "const auto protectChip = juce::Rectangle<float>"
        "protectPadHeight = static_cast<float> (enhancerPowerButton.getHeight())"
        "SMART PROTECT   ON"
        "drawIlluminatedPad (protectChip")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6C required status-chip authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 96)"
        "SMART PROTECT  /  ON")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6C legacy dot/text status remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P6C: glowing DSP state + Smart Protect status chips")

# P6D operates on the separately generated application source: compact tray panel
# spacing plus canonical SC220 tray/app artwork. Kept chained here so P6C remains
# the final console-surface pass while P6D owns only tray/application chrome.
include("${CMAKE_CURRENT_LIST_DIR}/SC220P6DTrayPolish.cmake")
