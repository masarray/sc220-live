# Final SC220 voice-lane noise-clean integration.
#
# The MusicalNoiseClean / SmoothDownwardExpander DSP sources are already the
# exact Sonkupik-Live implementation. This pass fixes the SC220 integration so
# the expander follows the Smart Enhancer (while using the cleaned pre-enhancer
# detector) and exposes the required learned-noise workflow without adding a
# second DSP implementation.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 static-noise integration requires final engine and UI sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_noise_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 static-noise anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Sonkupik-Live authority:
#   Noise Clean -> detector copy -> Smart Enhancer -> linked expander.
# The detector intentionally sees clean pre-enhancer audio, but expander gain is
# applied after enhancement so static hiss lifted by the enhancer is suppressed.
set(_voice_noise_anchor [=[
    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (voiceBlock, detectorBlock);

    voiceMidi.clear();
    voiceVst3.process (voiceBlock, voiceMidi); // Voice insert before native enhancer.
    voiceEnhancerBridge.process (voiceBlock, voiceMidi);
]=])
set(_voice_noise_replacement [=[
    voiceMidi.clear();
    voiceVst3.process (voiceBlock, voiceMidi); // Voice insert before native enhancer.
    voiceEnhancerBridge.process (voiceBlock, voiceMidi);

    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (voiceBlock, detectorBlock);
]=])
sc220_noise_replace_once(_engine "post-enhancer expander order"
    "${_voice_noise_anchor}" "${_voice_noise_replacement}")

# Keep the main console lean: Noise Clean is controlled from the existing gear
# menu. The spectral reducer only becomes effective after a two-second learned
# static-noise profile; the expander remains useful with its conservative
# fallback floor before a profile exists.
set(_settings_anchor [=[
void SC220MainComponent::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Audio Setup (WASAPI / ASIO)");
    menu.addItem (2, "License & Activation");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
                        {
                            if (result == 1)
                                showAudioSettings();
                            else if (result == 2)
                            {
                                licensePanel->setVisible (true);
                                licensePanel->toFront (true);
                            }
                        });
}
]=])
set(_settings_replacement [=[
void SC220MainComponent::showSettingsMenu()
{
    auto& noise = audioEngine.getNoiseClean();
    auto& expander = audioEngine.getExpander();
    const bool noiseEnabled = noise.isEnabled() && expander.isEnabled();

    juce::PopupMenu menu;
    menu.addItem (1, "Audio Setup (WASAPI / ASIO)");
    menu.addSeparator();
    menu.addItem (2, "Noise Clean", true, noiseEnabled);
    menu.addItem (3, "Learn Static Noise (2 sec)", noiseEnabled && ! noise.isLearning());

    juce::String profileStatus;
    if (noise.isLearning())
        profileStatus = "NOISE PROFILE: LEARNING "
                      + juce::String (juce::roundToInt (noise.getLearningProgress() * 100.0f)) + "%";
    else if (noise.hasProfile())
        profileStatus = "NOISE PROFILE: READY";
    else
        profileStatus = "NOISE PROFILE: NOT LEARNED";
    menu.addSectionHeader (profileStatus);
    menu.addSeparator();
    menu.addItem (4, "License & Activation");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
                        {
                            if (result == 1)
                            {
                                showAudioSettings();
                            }
                            else if (result == 2)
                            {
                                auto& noise = audioEngine.getNoiseClean();
                                auto& expander = audioEngine.getExpander();
                                const bool enable = ! (noise.isEnabled() && expander.isEnabled());
                                noise.setEnabled (enable);
                                expander.setEnabled (enable);
                            }
                            else if (result == 3)
                            {
                                auto& noise = audioEngine.getNoiseClean();
                                auto& expander = audioEngine.getExpander();
                                noise.setEnabled (true);
                                expander.setEnabled (true);
                                noise.startLearning (2.0);
                                juce::AlertWindow::showMessageBoxAsync (
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Learning SC220 static noise",
                                    "Keep the SC220 input silent for 2 seconds. The learned profile will target steady interface/preamp hiss while the stereo-linked expander cleans the remaining noise between phrases.");
                            }
                            else if (result == 4)
                            {
                                licensePanel->setVisible (true);
                                licensePanel->toFront (true);
                            }
                        });
}
]=])
sc220_noise_replace_once(_ui "Noise Clean gear-menu workflow"
    "${_settings_anchor}" "${_settings_replacement}")

# Fail closed if the final generated engine regresses to the earlier SC220 order.
string(FIND "${_engine}" "voiceEnhancerBridge.process (voiceBlock, voiceMidi);" _enhancer_pos)
string(FIND "${_engine}" "expander.process (voiceBlock, detectorBlock);" _expander_pos)
if(_enhancer_pos EQUAL -1 OR _expander_pos EQUAL -1 OR _expander_pos LESS _enhancer_pos)
    message(FATAL_ERROR "SC220 voice expander must run after the Smart Enhancer")
endif()

foreach(_required_ui IN ITEMS
        "Learn Static Noise (2 sec)"
        "noise.startLearning (2.0)"
        "NOISE PROFILE: READY"
        "noise.setEnabled (enable)"
        "expander.setEnabled (enable)")
    string(FIND "${_ui}" "${_required_ui}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 static-noise UI contract lost: ${_required_ui}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live Sonkupik static-noise clean path integrated")
