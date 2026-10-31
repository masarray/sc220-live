# P6E hardware/licensing polish.
#
# 1) Make the Windows tray icon deterministic even when Explorer is still
#    settling at process startup: render the canonical SC220 favicon to a 32px
#    ARGB surface and re-apply it a few times. Never fall back to ASK-P artwork.
# 2) The physical SC220 is the hardware entitlement. Software-only operation is
#    a 90-day full-feature trial, then the existing polished Midtrans/payment
#    licence panel becomes a required modal gate until SC220 hardware is present
#    or a perpetual activation is installed.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6E requires generated application and UI sources")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p6e_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6E anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Windows system-tray reliability
# ---------------------------------------------------------------------------
sc220_p6e_once(_app "tray startup retry timer"
    "class TrayIcon final : public juce::SystemTrayIconComponent"
    "class TrayIcon final : public juce::SystemTrayIconComponent, private juce::Timer")

sc220_p6e_once(_app "deterministic canonical tray install"
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
]=]
[=[
        installTrayIcon();
        // Explorer may still be rebuilding the notification area during login
        // or after an explorer.exe restart. Re-applying a small native-sized
        // image is cheap and prevents a valid long-lived tray object from
        // remaining icon-less because its first shell registration lost a race.
        startTimer (1200);
]=])

sc220_p6e_once(_app "tray private runtime"
[=[
private:
    LiveAudioEngine& audioEngine;
    std::function<void()> showMainWindow;
    std::function<void()> showMiniController;
    std::function<void()> quitApplication;
};
]=]
[=[
private:
    void timerCallback() override
    {
        installTrayIcon();
        if (++trayRefreshPass >= 3)
            stopTimer();
    }

    void installTrayIcon()
    {
        int iconBytes = 0;
        const char* iconData = BinaryData::getNamedResource ("favicon_96x96_png", iconBytes);
        if (iconData == nullptr || iconBytes <= 0)
            iconData = BinaryData::getNamedResource ("favicon_512x512_png", iconBytes);

        juce::Image source;
        if (iconData != nullptr && iconBytes > 0)
            source = juce::ImageCache::getFromMemory (iconData, iconBytes);

        trayImage = juce::Image (juce::Image::ARGB, 32, 32, true);
        juce::Graphics iconGraphics (trayImage);
        if (source.isValid())
        {
            iconGraphics.drawImageWithin (source, 1, 1, 30, 30,
                                          juce::RectanglePlacement::centred, false);
        }
        else
        {
            // Brand-safe emergency fallback. This is deliberately SC220-only;
            // the removed ASK-P/VST icon can never reappear in the tray.
            iconGraphics.setColour (juce::Colour (0xff0d1922));
            iconGraphics.fillRoundedRectangle ({ 1.0f, 1.0f, 30.0f, 30.0f }, 7.0f);
            iconGraphics.setColour (juce::Colour (0xff13bfe8));
            iconGraphics.drawEllipse ({ 6.0f, 6.0f, 20.0f, 20.0f }, 2.0f);
            iconGraphics.setColour (juce::Colours::white);
            iconGraphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            iconGraphics.drawText ("SC", 5, 7, 22, 18, juce::Justification::centred);
        }

        setIconImage (trayImage, {});
        setIconTooltip ("SC220 Live - running in background");
    }

    LiveAudioEngine& audioEngine;
    std::function<void()> showMainWindow;
    std::function<void()> showMiniController;
    std::function<void()> quitApplication;
    juce::Image trayImage;
    int trayRefreshPass = 0;
};
]=])

# ---------------------------------------------------------------------------
# Hardware entitlement + 90-day software-only gate
# ---------------------------------------------------------------------------
# The main surface must use the bridge entitlement (SC220 OR software licence),
# not the raw ASK-P trial bit by itself.
sc220_p6e_once(_ui "hardware-aware creative editing"
    "    const bool creativeEditing = license.editingAllowed();"
    "    const bool creativeEditing = audioEngine.fullFeaturesAllowed();")

# A software-only expired session is modal: reconnecting SC220 or activating a
# perpetual licence dismisses it. Closing the card while payment is required is
# intentionally ignored rather than leaving a one-second loophole.
sc220_p6e_once(_ui "required licence panel close policy"
[=[
    licensePanel->onClose = [this] { licensePanel->setVisible (false); };
]=]
[=[
    licensePanel->onClose = [this]
    {
        auto& gateLicense = audioEngine.getLicenseManager();
        gateLicense.refresh();
        const bool purchaseRequired = ! sc220DeviceWatcher.isHardwarePresent()
                                   && ! gateLicense.isLicensed()
                                   && gateLicense.state() == askp::LicenseManager::State::ReadOnly;
        if (purchaseRequired)
        {
            licensePanel->setVisible (true);
            licensePanel->toFront (true);
            return;
        }
        licensePanel->setVisible (false);
    };
]=])

sc220_p6e_once(_ui "automatic 90-day software-only payment gate"
[=[
        licenceRefreshDivider = 0;
        refreshAllControls();
    }

    lastStatus = audioEngine.getStatus();
]=]
[=[
        licenceRefreshDivider = 0;
        refreshAllControls();

        auto& gateLicense = audioEngine.getLicenseManager();
        gateLicense.refresh();
        const bool softwareOnlyExpired = ! sc220DeviceWatcher.isHardwarePresent()
                                      && ! gateLicense.isLicensed()
                                      && gateLicense.state() == askp::LicenseManager::State::ReadOnly;
        static bool softwareGateOwnsPanel = false;
        if (softwareOnlyExpired)
        {
            // Expired software-only sessions cannot continue a live stream in
            // the background while the purchase panel is visible.
            audioEngine.setOutputMuted (true);
            goLiveButton.setToggleState (false, juce::dontSendNotification);
            goLiveButton.setButtonText ("GO LIVE");
            licensePanel->setVisible (true);
            licensePanel->toFront (true);
            softwareGateOwnsPanel = true;
        }
        else if (softwareGateOwnsPanel)
        {
            licensePanel->setVisible (false);
            softwareGateOwnsPanel = false;
        }
    }

    lastStatus = audioEngine.getStatus();
]=])

# ---------------------------------------------------------------------------
# Reuse the existing polished Midtrans licence panel with SC220-aware copy.
# ---------------------------------------------------------------------------
get_filename_component(_p6e_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_license_panel_source "${_p6e_root}/live/LiveLicensePanel.cpp")
file(READ "${_license_panel_source}" _license_panel)

string(REPLACE
    "SC220 LIVE FULL VERSION"
    "LICENSE & ACTIVATION"
    _license_panel "${_license_panel}")
string(REPLACE
    "Full creative control is free for 365 days. Unlock permanently with one optional QRIS payment."
    "Choose secure QRIS payment or enter a signed offline activation code."
    _license_panel "${_license_panel}")

sc220_p6e_once(_license_panel "hardware-aware licence state"
[=[
    const auto state = license.state();
    const auto snap = checkout.snapshot();

    if (license.isLicensed())
]=]
[=[
    const auto state = license.state();
    const auto snap = checkout.snapshot();
    const bool hardwareAccess = engine.isHardwareEntitled();

    if (license.isLicensed())
]=])

sc220_p6e_once(_license_panel "SC220 hardware entitlement card"
[=[
        buyButton.setButtonText ("FULL VERSION ACTIVE");
        buyButton.setEnabled (false);
    }
    else
    {
        const bool expired = state == askp::LicenseManager::State::ReadOnly;
]=]
[=[
        buyButton.setButtonText ("FULL VERSION ACTIVE");
        buyButton.setEnabled (false);
        closeButton.setVisible (true);
    }
    else if (hardwareAccess)
    {
        trialBadge.setText ("SC220 HARDWARE ACCESS", juce::dontSendNotification);
        trialBadge.setColour (juce::Label::textColourId, colours::green);
        price.setText ("INCLUDED", juce::dontSendNotification);
        price.setColour (juce::Label::textColourId, colours::green);
        priceCaption.setText ("SC220 HARDWARE CONNECTED / SOFTWARE-ONLY LICENCE OPTIONAL",
                              juce::dontSendNotification);
        buyButton.setButtonText (snap.checkoutCanOpen ? "OPEN QRIS PAYMENT PAGE"
                                                       : snap.hasPendingOrder ? "VERIFY SAVED PURCHASE"
                                                                              : "BUY SOFTWARE-ONLY LICENCE - QRIS");
        buyButton.setEnabled (! snap.busy);
        closeButton.setVisible (true);
    }
    else
    {
        const bool expired = state == askp::LicenseManager::State::ReadOnly;
]=])

sc220_p6e_once(_license_panel "software-only trial badge copy"
[=[
        trialBadge.setText (clockIssue ? "CHECK SYSTEM CLOCK"
                                      : expired ? "FREE PERIOD ENDED"
                                                : "FULL FEATURES ACTIVE",
]=]
[=[
        trialBadge.setText (clockIssue ? "CHECK SYSTEM CLOCK"
                                      : expired ? "90-DAY SOFTWARE TRIAL ENDED"
                                                : "SOFTWARE-ONLY TRIAL ACTIVE",
]=])

sc220_p6e_once(_license_panel "required panel cannot close after expiry"
[=[
        buyButton.setEnabled (! snap.busy);
    }

    checkButton.setVisible (snap.hasPendingOrder && ! license.isLicensed());
]=]
[=[
        buyButton.setEnabled (! snap.busy);
        closeButton.setVisible (! expired);
    }

    checkButton.setVisible (snap.hasPendingOrder && ! license.isLicensed());
]=])

if(NOT CMAKE_SCRIPT_MODE_FILE)
    # Independent SC220 licence source: preserve 90-day software trial
    # without importing or rewriting private ASK-P-generated headers.
    set(_sc220_license_header "${CMAKE_CURRENT_SOURCE_DIR}/live/LicenseManager.h")
    file(READ "${_sc220_license_header}" _license_header)
    if(NOT _license_header MATCHES "betaDurationDays = 90")
        message(FATAL_ERROR "SC220 software-only trial must remain exactly 90 days")
    endif()

    set(_generated_panel "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-p6e/LiveLicensePanel_P6E.cpp")
    get_filename_component(_generated_panel_dir "${_generated_panel}" DIRECTORY)
    file(MAKE_DIRECTORY "${_generated_panel_dir}")
    file(WRITE "${_generated_panel}" "${_license_panel}")
    set_source_files_properties("${_license_panel_source}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE "${_generated_panel}")
endif()

foreach(_required IN ITEMS
        "class TrayIcon final : public juce::SystemTrayIconComponent, private juce::Timer"
        "trayImage = juce::Image (juce::Image::ARGB, 32, 32, true)"
        "setIconImage (trayImage, {})"
        "startTimer (1200)"
        "audioEngine.fullFeaturesAllowed()"
        "90-DAY SOFTWARE TRIAL ENDED"
        "SC220 HARDWARE ACCESS"
        "softwareOnlyExpired"
        "gateLicense.state() == askp::LicenseManager::State::ReadOnly"
        "BUY SOFTWARE-ONLY LICENCE - QRIS")
    string(FIND "${_app}${_ui}${_license_panel}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6E required authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "askp_app_logo_png"
        "Full creative control is free for 365 days")
    string(FIND "${_app}${_license_panel}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6E stale tray/licence authority remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P6E: reliable tray + SC220 hardware entitlement + 90-day software-only licence")
