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
    const auto dspChip = juce::Rectangle<float> (
        static_cast<float> (enhancerPowerButton.getX() - 112),
        static_cast<float> (enhancerPowerButton.getBounds().getCentreY() - 10),
        96.0f, 20.0f);
    g.setColour (dspChipColour.withAlpha (0.10f + 0.10f * dspGlow));
    g.fillRoundedRectangle (dspChip.expanded (2.0f), 7.0f);
    g.setColour (dspChipColour.darker (0.12f)
                    .interpolatedWith (dspChipColour.brighter (0.18f), 0.30f * dspGlow));
    g.fillRoundedRectangle (dspChip, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.16f + 0.18f * dspGlow));
    g.drawRoundedRectangle (dspChip.reduced (0.7f), 5.4f, 1.0f);
    g.setColour (juce::Colours::white);
    g.setFont (interFont (9.0f, true));
    g.drawFittedText (active ? "DSP ON" : "DSP BYPASS", dspChip.toNearestInt().reduced (8, 2),
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
    const auto protectChip = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getCentreX() - 82),
        static_cast<float> (signatureArea.getBottom() - 49),
        164.0f, 22.0f);
    g.setColour (palette::green.withAlpha (0.08f + 0.10f * protectGlow));
    g.fillRoundedRectangle (protectChip.expanded (2.0f), 7.0f);
    g.setColour (palette::green.darker (0.10f)
                    .interpolatedWith (palette::green.brighter (0.16f), 0.24f * protectGlow));
    g.fillRoundedRectangle (protectChip, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f + 0.16f * protectGlow));
    g.drawRoundedRectangle (protectChip.reduced (0.7f), 5.4f, 1.0f);
    g.setColour (juce::Colours::white);
    g.setFont (interFont (9.2f, true));
    g.drawFittedText ("SMART PROTECT   ON", protectChip.toNearestInt().reduced (10, 2),
                      juce::Justification::centred, 1);
]=])

foreach(_required IN ITEMS
        "const auto dspChipColour = active ? palette::green : palette::amber"
        "active ? \"DSP ON\" : \"DSP BYPASS\""
        "g.fillRoundedRectangle (dspChip"
        "const auto protectChip = juce::Rectangle<float>"
        "SMART PROTECT   ON"
        "g.fillRoundedRectangle (protectChip")
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
