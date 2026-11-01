# SC220 P12: free public Windows preview and professional DAW mixer surface.
# Runs after legacy P3-P11 generators so no later stage undoes the final UI.
if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P12 requires the generated final mixer UI")
endif()
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p12_once label anchor replacement)
    string(FIND "${_ui}" "${anchor}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "SC220 P12 anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _updated "${_ui}")
    set(_ui "${_updated}" PARENT_SCOPE)
endfunction()

function(sc220_p12_range label begin_marker end_marker replacement)
    string(FIND "${_ui}" "${begin_marker}" _a)
    string(FIND "${_ui}" "${end_marker}" _b)
    if(_a EQUAL -1 OR _b EQUAL -1 OR _b LESS _a)
        message(FATAL_ERROR "SC220 P12 range changed: ${label}")
    endif()
    string(SUBSTRING "${_ui}" 0 ${_a} _prefix)
    string(SUBSTRING "${_ui}" ${_b} -1 _suffix)
    set(_ui "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# The paid UI must be absent, not merely invisible. The old licence state
# remains compatible on disk, but the public build does not compile its checkout
# client or present a paid activation modal.
sc220_p12_once("remove paid overlay initialiser"
[=[      loudnessDisplay (std::make_unique<LoudnessDisplay>()),
      licensePanel (std::make_unique<LiveLicensePanel> (engine.getEnhancerBridge()))]=]
[=[      loudnessDisplay (std::make_unique<LoudnessDisplay>())]=])
sc220_p12_once("remove paid overlay from children"
[=[    addChildComponent (*licensePanel);]=]
[=[]=])
sc220_p12_once("disable licence overlay callbacks"
[=[    licensePanel->onClose = [this] { licensePanel->setVisible (false); };
    licensePanel->onLicenseChanged = [this] { refreshAllControls(); };]=]
[=[]=])
sc220_p12_range("settings opens native audio setup only"
[=[void SC220MainComponent::showSettingsMenu()]=]
[=[class VoiceFxEditorWindow final]=]
[=[void SC220MainComponent::showSettingsMenu()
{
    showAudioSettings();
}

]=])
sc220_p12_once("remove paid overlay bounds"
[=[    licensePanel->setBounds (getLocalBounds());]=]
[=[]=])
sc220_p12_once("remove trial-gated UI refresh"
[=[    auto& license = audioEngine.getLicenseManager();
    license.refresh();
    const bool creativeEditing = license.editingAllowed();
    presetBox.setEnabled (creativeEditing);
    for (auto& knob : macroKnobs)
        knob.setEnabled (creativeEditing);]=]
[=[    // Public/free application controls: no trial expiry and no payments.
    presetBox.setEnabled (true);
    for (auto& knob : macroKnobs)
        knob.setEnabled (true);]=])
sc220_p12_once("clean settings tooltip"
[=[    settingsButton.setTooltip ("Audio Setup (WASAPI / ASIO) and License");]=]
[=[    settingsButton.setTooltip ("Audio devices, ASIO and Stream / Monitor routing");]=])

# Maintain real input/meter control locations: only redistribute content and
# compositor chrome. No audio callbacks or DSP code are touched.
sc220_p12_once("balanced compact 3-zone desktop geometry"
[=[    const int inputWidth = juce::roundToInt (area.getWidth() * 0.440f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.310f);]=]
[=[    // Two equal-width DAW input strips; spectrum / Signature and master
    // buses remain readable even at 1180 x 720.
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.442f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.305f);]=])

# Dual Mono sliders and L/R meters must not occupy the same pixels.
sc220_p12_once("move Voice meter away from dual-mono rail"
[=[            sc220MonoLeftFader.setBounds (strip.getX() + 11, sharedFaderTop,
                                          42, sharedFaderHeight);
            sc220MonoRightFader.setBounds (strip.getX() + 60, sharedFaderTop,
                                           42, sharedFaderHeight);
            sc220Meter->setBounds (strip.getX() + 3, sharedFaderTop + 4,
                                    18, sharedFaderHeight - 8);]=]
[=[            sc220MonoLeftFader.setBounds (strip.getX() + 23, sharedFaderTop,
                                          35, sharedFaderHeight);
            sc220MonoRightFader.setBounds (strip.getX() + 65, sharedFaderTop,
                                           35, sharedFaderHeight);
            sc220Meter->setBounds (strip.getX() + 4, sharedFaderTop + 4,
                                    14, sharedFaderHeight - 8);]=])

# Explicitly retain original thumb render; validate it after all stages.
sc220_p12_once("replace checkout footer with clear free edition status"
[=[    const auto pathText = "SC220 + PC AUDIO    >    VOICE / MUSIC SIGNATURE    >    VB-CABLE";]=]
[=[    const auto pathText = "FREE PUBLIC PREVIEW    |    VOICE + PC MUSIC    >    FX + SIGNATURE    >    STREAM / MONITOR";]=])

foreach(_required IN ITEMS
    "channelInsertPowers" "channelInsertMenus" "channelRackAreas"
    "sc220MonoLeftFader.setBounds" "getChannelFxName (channel, slot)"
    "getChannelFxProcessor (channel, slot)" "kSc220FaderThumbSvg")
    string(FIND "${_ui}" "${_required}" _i)
    if(_i EQUAL -1)
        message(FATAL_ERROR "SC220 P12 required professional mixer feature missing: ${_required}")
    endif()
endforeach()
foreach(_forbidden IN ITEMS "licensePanel" "License & Activation" "BUY FULL VERSION" "90-DAY SOFTWARE TRIAL")
    string(FIND "${_ui}" "${_forbidden}" _i)
    if(NOT _i EQUAL -1)
        message(FATAL_ERROR "SC220 P12 paid/trial interface remains: ${_forbidden}")
    endif()
endforeach()
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P12: public free edition, no checkout GUI, DAW mixer controls")
