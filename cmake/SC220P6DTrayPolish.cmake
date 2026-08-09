# P6D tray/compact-controller polish from hardware review.
# This pass is application-UI only: it does not touch audio/DSP/routing state.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6D tray polish requires the generated application source")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p6d_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6D anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Compact controller header uses the same corrected SC220 96px app icon as the
# Windows tray and executable small icon.
sc220_p6d_once(_app "compact header SC220 icon"
[=[
        if (auto icon = createLucideIcon ("audio_waveform_svg", colours::cyan))
            icon->drawWithin (g, juce::Rectangle<float> (16.0f, 14.0f, 20.0f, 20.0f),
                              juce::RectanglePlacement::centred, 1.0f);
]=]
[=[
        int appIconBytes = 0;
        const auto* appIconData = BinaryData::getNamedResource ("favicon_96x96_png", appIconBytes);
        if (appIconData != nullptr && appIconBytes > 0)
        {
            const auto appIcon = juce::ImageCache::getFromMemory (appIconData, appIconBytes);
            if (appIcon.isValid())
                g.drawImageWithin (appIcon, 14, 10, 28, 28,
                                   juce::RectanglePlacement::centred, false);
        }
]=])

# The PRESET caption previously overlapped Restart Audio and appeared clipped.
# Give the control stack a clear vertical rhythm and slightly wider right column.
sc220_p6d_once(_app "compact controller preset caption"
[=[
        g.drawText ("PRESET", 138, 162, 120, 20, juce::Justification::centredLeft, false);
]=]
[=[
        g.drawText ("PRESET", 146, 184, 148, 18,
                    juce::Justification::centredLeft, false);
]=])

sc220_p6d_once(_app "compact controller geometry"
[=[
        closeButton.setBounds (getWidth() - 44, 7, 36, 36);
        powerButton.setBounds (42, 87, 54, 54);
        openButton.setBounds (136, 62, 138, 36);
        bypassToggle.setBounds (136, 104, 138, 32);
        restartButton.setBounds (136, 138, 138, 36);
        presetBox.setBounds (136, 184, 138, 38);
        outputStatus.setBounds (20, getHeight() - 50, getWidth() - 40, 36);
]=]
[=[
        closeButton.setBounds (getWidth() - 44, 7, 36, 36);
        powerButton.setBounds (42, 87, 54, 54);
        openButton.setBounds (146, 62, 154, 36);
        bypassToggle.setBounds (146, 104, 154, 32);
        restartButton.setBounds (146, 142, 154, 36);
        presetBox.setBounds (146, 204, 154, 38);
        outputStatus.setBounds (20, getHeight() - 52, getWidth() - 40, 36);
]=])

sc220_p6d_once(_app "compact controller window size"
    "            miniWindow->setSize (300, 290);"
    "            miniWindow->setSize (320, 324);")

# Remove the legacy ASK-P/VST fallback. P6E owns tray registration lifetime and
# P6F verifies that these favicon resources are the corrected SC220 app assets.
sc220_p6d_once(_app "canonical SC220 system tray icon"
[=[
        int iconBytes = 0;
        const auto* iconData = BinaryData::getNamedResource ("favicon_96x96_png", iconBytes);
        if (iconData == nullptr)
            iconData = BinaryData::getNamedResource ("askp_app_logo_png", iconBytes);
        if (iconData != nullptr && iconBytes > 0)
        {
            const auto icon = juce::ImageCache::getFromMemory (iconData, iconBytes);
            if (icon.isValid())
                setIconImage (icon, icon);
        }
        setIconTooltip ("SC220 Live - audio continues in the background");
]=]
[=[
        int iconBytes = 0;
        const char* iconData = BinaryData::getNamedResource ("favicon_96x96_png", iconBytes);
        if (iconData == nullptr || iconBytes <= 0)
            iconData = BinaryData::getNamedResource ("favicon_512x512_png", iconBytes);
        if (iconData != nullptr && iconBytes > 0)
        {
            const auto icon = juce::ImageCache::getFromMemory (iconData, iconBytes);
            if (icon.isValid())
                setIconImage (icon, icon);
        }
        setIconTooltip ("SC220 Live - running in background");
]=])

foreach(_required IN ITEMS
        "g.drawText (\"PRESET\", 146, 184, 148, 18"
        "openButton.setBounds (146, 62, 154, 36)"
        "presetBox.setBounds (146, 204, 154, 38)"
        "miniWindow->setSize (320, 324)"
        "BinaryData::getNamedResource (\"favicon_96x96_png\""
        "BinaryData::getNamedResource (\"favicon_512x512_png\""
        "SC220 Live - running in background")
    string(FIND "${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6D required tray authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "askp_app_logo_png"
        "SC220 Live - audio continues in the background"
        "g.drawText (\"PRESET\", 138, 162")
    string(FIND "${_app}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6D legacy tray/compact UI remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P6D: compact tray panel spacing + SC220 app icon path")

# P6E owns Windows tray registration lifetime/retry and commercial policy.
include("${CMAKE_CURRENT_LIST_DIR}/SC220P6ETrayReliabilityAndSoftwareTrial.cmake")

# P6F locks P6E's existing PNG runtime path to the corrected SC220 app assets.
include("${CMAKE_CURRENT_LIST_DIR}/SC220P6FNativeTrayIcon.cmake")
