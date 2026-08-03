# Final production polish applied after the aligned and physical-monitor UI
# generators. The final monitor source is rewritten in place so the target keeps
# one implementation while layout and analyzer contracts remain auditable.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 final UI polish requires SC220_UI_MONITOR_GENERATED_SOURCE")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_polish_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 final UI polish anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Give the output/monitor console enough width at normal desktop sizes without
# making the central Signature workspace feel cramped.
set(_panel_ratio_anchor [=[
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.325f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.455f);
]=])
set(_panel_ratio_replacement [=[
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.310f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.450f);
]=])
sc220_polish_replace_once(_ui "balanced panel ratios"
    "${_panel_ratio_anchor}" "${_panel_ratio_replacement}")

# Use the previously empty lower area for longer, easier-to-read console faders.
set(_fader_grid_anchor [=[
    const int sharedFaderTop = inputArea.getY() + 366;
    const int sharedFaderBottom = inputArea.getBottom() - 154;
    const int sharedFaderHeight = juce::jmax (190, sharedFaderBottom - sharedFaderTop);
    const int sourceSelectorY = sharedFaderTop - 78;
]=])
set(_fader_grid_replacement [=[
    const int sharedFaderTop = inputArea.getY() + 366;
    const int sharedFaderBottom = inputArea.getBottom() - 96;
    const int sharedFaderHeight = juce::jmax (248, sharedFaderBottom - sharedFaderTop);
    const int sourceSelectorY = sharedFaderTop - 78;
]=])
sc220_polish_replace_once(_ui "extended shared fader grid"
    "${_fader_grid_anchor}" "${_fader_grid_replacement}")

set(_duck_layout_anchor [=[
    duckAmount.setBounds (pcStripArea.getX() + 24,
                          pcStripArea.getBottom() - 82, 70, 70);
]=])
set(_duck_layout_replacement [=[
    duckAmount.setBounds (pcStripArea.getX() + 25,
                          pcStripArea.getBottom() - 70, 60, 60);
]=])
sc220_polish_replace_once(_ui "compact duck control"
    "${_duck_layout_anchor}" "${_duck_layout_replacement}")

# Two responsive columns with a guaranteed gap eliminate the Stream/Monitor
# dropdown collision. All controls are centred on the same column grid.
set(_master_layout_anchor [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 100,
                                masterArea.getY() + 48, 200, 200);
    auto masterLower = masterArea.reduced (24, 0);
    const int streamColumnWidth = juce::jmin (174, masterLower.getWidth() / 2);
    auto streamColumn = masterLower.removeFromLeft (streamColumnWidth);
    auto monitorColumn = masterLower;

    outputBox.setBounds (streamColumn.getCentreX() - 82,
                         sourceSelectorY, 164, 34);
    streamMeter->setBounds (streamColumn.getX() + 8, sharedFaderTop,
                            sharedMeterWidth, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);

    monitorOutputBox.setBounds (monitorColumn.getCentreX() - 82,
                                sourceSelectorY, 164, 34);
    monitorPowerButton.setBounds (monitorOutputBox.getRight() - 36,
                                  monitorOutputBox.getY() - 48, 34, 34);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 48,
                           sourceSelectorY + 62, 96, 96);
    const int modeY = monitorKnob.getBottom() + 18;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - 82,
                                    modeY, 82, 30);
    musicCueButton.setBounds (monitorColumn.getCentreX(), modeY, 82, 30);
]=])
set(_master_layout_replacement [=[
    loudnessDisplay->setBounds (masterArea.getCentreX() - 96,
                                masterArea.getY() + 44, 192, 192);
    auto masterLower = masterArea.reduced (18, 0);
    constexpr int masterColumnGap = 12;
    const int masterColumnWidth = juce::jmax (108,
        (masterLower.getWidth() - masterColumnGap) / 2);
    auto streamColumn = masterLower.removeFromLeft (masterColumnWidth);
    masterLower.removeFromLeft (masterColumnGap);
    auto monitorColumn = masterLower;
    const int selectorWidth = juce::jmax (108,
        juce::jmin (154, juce::jmin (streamColumn.getWidth(), monitorColumn.getWidth()) - 4));

    outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2,
                         sourceSelectorY, selectorWidth, 34);
    streamMeter->setBounds (streamColumn.getX() + 6, sharedFaderTop,
                            sharedMeterWidth, sharedFaderHeight);
    streamFader.setBounds (streamColumn.getCentreX() - sharedFaderWidth / 2,
                           sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    streamMute.setBounds (streamColumn.getCentreX() - 36,
                          masterArea.getBottom() - 44, 72, 31);

    monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                sourceSelectorY, selectorWidth, 34);
    monitorPowerButton.setBounds (monitorColumn.getRight() - 34,
                                  monitorOutputBox.getY() - 46, 32, 32);
    monitorKnob.setBounds (monitorColumn.getCentreX() - 44,
                           sourceSelectorY + 58, 88, 88);
    const int modeY = monitorKnob.getBottom() + 18;
    const int modeWidth = selectorWidth / 2;
    programMonitorButton.setBounds (monitorColumn.getCentreX() - selectorWidth / 2,
                                    modeY, modeWidth, 30);
    musicCueButton.setBounds (programMonitorButton.getRight(), modeY,
                              selectorWidth - modeWidth, 30);
]=])
sc220_polish_replace_once(_ui "responsive stream and monitor columns"
    "${_master_layout_anchor}" "${_master_layout_replacement}")

# Read from the bridge-owned taps. The previous processor tap was toggled around
# every audio block and could be empty whenever the 30 Hz UI timer arrived.
set(_spectrum_update_anchor [=[
    spectrumDisplay->updateFromTaps (audioEngine.getVoiceEnhancerBridge().processor().scopeTap,
                                     audioEngine.getMusicEnhancerBridge().processor().scopeTap,
                                     lastStatus.sampleRate);
]=])
set(_spectrum_update_replacement [=[
    static int spectrumDivider = 0;
    if (++spectrumDivider >= 2)
    {
        spectrumDivider = 0;
        spectrumDisplay->updateFromTaps (
            audioEngine.getVoiceEnhancerBridge().analysisTap(),
            audioEngine.getMusicEnhancerBridge().analysisTap(),
            lastStatus.sampleRate);
    }
]=])
sc220_polish_replace_once(_ui "bridge-owned analyzer taps"
    "${_spectrum_update_anchor}" "${_spectrum_update_replacement}")

# 1024-point FFT at 15 visual frames per second is ample for this compact
# broadcast display while keeping message-thread CPU effectively negligible.
set(_fft_order_anchor "    static constexpr int fftOrder = 11;")
set(_fft_order_replacement "    static constexpr int fftOrder = 10;")
sc220_polish_replace_once(_ui "lightweight FFT size"
    "${_fft_order_anchor}" "${_fft_order_replacement}")

set(_spectrum_bins_anchor "    std::array<float, 96> smoothed {};")
set(_spectrum_bins_replacement "    std::array<float, 72> smoothed {};")
sc220_polish_replace_once(_ui "lightweight display bins"
    "${_spectrum_bins_anchor}" "${_spectrum_bins_replacement}")

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live final polished UI: ${SC220_UI_MONITOR_GENERATED_SOURCE}")
