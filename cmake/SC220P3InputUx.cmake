# P3 SC220 input authority: direct frozen-profile noise controls and real
# independent Dual Mono channel trims, while preserving the proven analyzer.

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p3_input_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 input anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Shared lock-free dual-mono fader state. The 14-band visualizer include is a
# stable final-chain anchor because this pass intentionally runs after it.
sc220_p3_input_once(_engine "engine P3 runtime state include"
    "#include \"ProgrammeBandTelemetry.h\"\n"
    "#include \"ProgrammeBandTelemetry.h\"\n#include \"SC220P3RuntimeState.h\"\n")
sc220_p3_input_once(_ui "UI P3 runtime state include"
    "#include \"ProgrammeBandTelemetry.h\"\n"
    "#include \"ProgrammeBandTelemetry.h\"\n#include \"SC220P3RuntimeState.h\"\n")

# Learned-floor cleanup should be clearly audible in silence, yet open quickly
# and leave active music/voice at unity. No spectral processing runs after Learn.
sc220_p3_input_once(_engine "natural learned-floor expander tuning"
    "    expander.prepare (sampleRate);\n"
    "    expander.prepare (sampleRate);\n    expander.setAmount (0.78f);\n    expander.setThresholdOffsetDb (5.5f);\n")

# Stereo keeps one linked fader. Dual Mono keeps the real L/R channels and gives
# each one an independent gain ramp; the existing meter already shows L/R peaks.
set(_linked_gain [=[
    sc220MixGainLinear.setTargetValue (juce::Decibels::decibelsToGain (sc220FaderDb.load()));
    const float sc220MuteGain = sc220Muted.load() ? 0.0f : 1.0f;
    const float sc220GainStart = sc220MixGainLinear.getCurrentValue() * sc220MuteGain;
    const float sc220GainEnd = sc220MixGainLinear.skip (numSamples) * sc220MuteGain;
    voiceBlock.applyGainRamp (0, numSamples, sc220GainStart, sc220GainEnd);
]=])
set(_dual_gain [=[
    const float sc220MuteGain = sc220Muted.load() ? 0.0f : 1.0f;
    if (inputChannelMode.load() == 2)
    {
        sc220MixGainLinear.setTargetValue (
            juce::Decibels::decibelsToGain (sc220FaderDb.load()));
        const float sc220GainStart = sc220MixGainLinear.getCurrentValue() * sc220MuteGain;
        const float sc220GainEnd = sc220MixGainLinear.skip (numSamples) * sc220MuteGain;
        voiceBlock.applyGainRamp (0, numSamples, sc220GainStart, sc220GainEnd);
    }
    else
    {
        static float dualLeftGain = 1.0f;
        static float dualRightGain = 1.0f;
        const float leftTarget = juce::Decibels::decibelsToGain (
            sc220P3RuntimeState.dualMonoLeftFaderDb.load()) * sc220MuteGain;
        const float rightTarget = juce::Decibels::decibelsToGain (
            sc220P3RuntimeState.dualMonoRightFaderDb.load()) * sc220MuteGain;
        voiceBlock.applyGainRamp (0, 0, numSamples, dualLeftGain, leftTarget);
        voiceBlock.applyGainRamp (1, 0, numSamples, dualRightGain, rightTarget);
        dualLeftGain = leftTarget;
        dualRightGain = rightTarget;
    }
]=])
sc220_p3_input_once(_engine "independent Dual Mono gain ramps"
    "${_linked_gain}" "${_dual_gain}")

# Add all new header-owned controls to JUCE visibility/LookAndFeel setup.
set(_visible_old [=[
             &stereoButton, &dualMonoButton, &pcSourceBox,
             &programMonitorButton, &musicCueButton, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob,
]=])
set(_visible_new [=[
             &stereoButton, &dualMonoButton, &noiseCleanButton, &learnNoiseButton,
             &pcSourceBox, &programMonitorButton, &musicCueButton, &inputTrim,
             &sc220Fader, &sc220MonoLeftFader, &sc220MonoRightFader,
             &pcFader, &duckAmount, &streamFader, &monitorKnob,
]=])
sc220_p3_input_once(_ui "new SC220 controls are visible"
    "${_visible_old}" "${_visible_new}")

sc220_p3_input_once(_ui "configure both mono faders"
[=[
    configureFader (sc220Fader);
    configureFader (pcFader);
]=]
[=[
    configureFader (sc220Fader);
    configureFader (sc220MonoLeftFader);
    configureFader (sc220MonoRightFader);
    configureFader (pcFader);
]=])

set(_mode_old [=[
    stereoButton.onClick = [this] { audioEngine.setInputChannelMode (2); };
    dualMonoButton.onClick = [this] { audioEngine.setInputChannelMode (1); };
]=])
set(_mode_new [=[
    stereoButton.onClick = [this]
    {
        const float linked = 0.5f * (
            sc220P3RuntimeState.dualMonoLeftFaderDb.load()
          + sc220P3RuntimeState.dualMonoRightFaderDb.load());
        audioEngine.setSc220FaderDb (linked);
        audioEngine.setInputChannelMode (2);
        refreshAllControls();
    };
    dualMonoButton.onClick = [this]
    {
        if (audioEngine.getInputChannelMode() == 2)
        {
            const float linked = audioEngine.getSc220FaderDb();
            sc220P3RuntimeState.dualMonoLeftFaderDb.store (linked);
            sc220P3RuntimeState.dualMonoRightFaderDb.store (linked);
        }
        audioEngine.setInputChannelMode (1);
        refreshAllControls();
    };

    noiseCleanButton.setClickingTogglesState (true);
    noiseCleanButton.setTooltip ("Frozen learned-noise cleanup on / off");
    noiseCleanButton.onClick = [this]
    {
        const bool enabled = noiseCleanButton.getToggleState();
        audioEngine.getNoiseClean().setEnabled (enabled);
        audioEngine.getExpander().setEnabled (enabled);
        repaint (sc220StripArea);
    };

    learnNoiseButton.setTooltip ("Learn 2 seconds of SC220 static noise while silent");
    learnNoiseButton.onClick = [this]
    {
        auto& noise = audioEngine.getNoiseClean();
        if (noise.isLearning())
            return;
        noise.setEnabled (true);
        audioEngine.getExpander().setEnabled (true);
        noiseCleanButton.setToggleState (true, juce::dontSendNotification);
        noise.startLearning (2.0);
        repaint (sc220StripArea);
    };
]=])
sc220_p3_input_once(_ui "direct DENOISE and LEARN callbacks"
    "${_mode_old}" "${_mode_new}")

sc220_p3_input_once(_ui "independent Dual Mono fader callbacks"
[=[
    sc220Fader.onValueChange = [this] { audioEngine.setSc220FaderDb (static_cast<float> (sc220Fader.getValue())); };
    pcFader.onValueChange = [this] { audioEngine.setPcAudioGainDb (static_cast<float> (pcFader.getValue())); };
]=]
[=[
    sc220Fader.onValueChange = [this] { audioEngine.setSc220FaderDb (static_cast<float> (sc220Fader.getValue())); };
    sc220MonoLeftFader.onValueChange = [this]
    {
        sc220P3RuntimeState.dualMonoLeftFaderDb.store (
            static_cast<float> (sc220MonoLeftFader.getValue()));
    };
    sc220MonoRightFader.onValueChange = [this]
    {
        sc220P3RuntimeState.dualMonoRightFaderDb.store (
            static_cast<float> (sc220MonoRightFader.getValue()));
    };
    pcFader.onValueChange = [this] { audioEngine.setPcAudioGainDb (static_cast<float> (pcFader.getValue())); };
]=])

sc220_p3_input_once(_ui "refresh independent mono values"
[=[
    sc220Fader.setValue (audioEngine.getSc220FaderDb(), juce::dontSendNotification);
    pcFader.setValue (audioEngine.getPcAudioGainDb(), juce::dontSendNotification);
]=]
[=[
    sc220Fader.setValue (audioEngine.getSc220FaderDb(), juce::dontSendNotification);
    sc220MonoLeftFader.setValue (sc220P3RuntimeState.dualMonoLeftFaderDb.load(),
                                 juce::dontSendNotification);
    sc220MonoRightFader.setValue (sc220P3RuntimeState.dualMonoRightFaderDb.load(),
                                  juce::dontSendNotification);
    pcFader.setValue (audioEngine.getPcAudioGainDb(), juce::dontSendNotification);
]=])

sc220_p3_input_once(_ui "refresh mode visibility and Noise state"
[=[
    const bool stereo = audioEngine.getInputChannelMode() == 2;
    stereoButton.setToggleState (stereo, juce::dontSendNotification);
    dualMonoButton.setToggleState (! stereo, juce::dontSendNotification);
]=]
[=[
    const bool stereo = audioEngine.getInputChannelMode() == 2;
    stereoButton.setToggleState (stereo, juce::dontSendNotification);
    dualMonoButton.setToggleState (! stereo, juce::dontSendNotification);
    sc220Fader.setVisible (stereo);
    sc220MonoLeftFader.setVisible (! stereo);
    sc220MonoRightFader.setVisible (! stereo);

    const bool denoiseEnabled = audioEngine.getNoiseClean().isEnabled()
                             && audioEngine.getExpander().isEnabled();
    noiseCleanButton.setToggleState (denoiseEnabled, juce::dontSendNotification);
    noiseCleanButton.setButtonText (denoiseEnabled ? "DENOISE ON" : "DENOISE OFF");
    learnNoiseButton.setButtonText (audioEngine.getNoiseClean().isLearning()
                                    ? "LEARNING..." : "LEARN");
]=])

# The 51 px gap below the Stereo/Dual-Mono row is reserved for DENOISE / LEARN;
# the progress bar sits immediately below and remains above the fader grid.
set(_layout_old [=[
    stereoButton.setBounds (sc220StripArea.getX() + 10, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 42,
                         sourceSelectorY - 94, 84, 84);
    sc220Meter->setBounds (sc220StripArea.getX() + 18, sharedFaderTop,
                           sc220StripArea.getWidth() - 36, sharedFaderHeight);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - sharedFaderWidth / 2,
                          sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         sc220StripArea.getBottom() - 42, 72, 31);
]=])
set(_layout_new [=[
    stereoButton.setBounds (sc220StripArea.getX() + 10, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 42,
                         sourceSelectorY - 94, 84, 84);

    const int noiseRowY = sourceSelectorY + 37;
    const int noiseWidth = (sc220StripArea.getWidth() - 26) / 2;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 10, noiseRowY,
                                noiseWidth, 25);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + 6, noiseRowY,
                                noiseWidth, 25);

    sc220Meter->setBounds (sc220StripArea.getX() + 18, sharedFaderTop,
                           sc220StripArea.getWidth() - 36, sharedFaderHeight);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - sharedFaderWidth / 2,
                          sharedFaderTop, sharedFaderWidth, sharedFaderHeight);
    sc220MonoLeftFader.setBounds (sc220StripArea.getCentreX() - 58,
                                  sharedFaderTop, 46, sharedFaderHeight);
    sc220MonoRightFader.setBounds (sc220StripArea.getCentreX() + 12,
                                   sharedFaderTop, 46, sharedFaderHeight);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36,
                         sc220StripArea.getBottom() - 42, 72, 31);
]=])
sc220_p3_input_once(_ui "DENOISE and Dual Mono responsive geometry"
    "${_layout_old}" "${_layout_new}")

# Keep PC scale unchanged. In Dual Mono the two faders use compact CH1/CH2 dB
# labels so two full printed scales do not collide visually.
sc220_p3_input_once(_ui "mode-aware fader scale plus Learn progress"
[=[
    drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());
]=]
[=[
    const auto& noise = audioEngine.getNoiseClean();
    auto progress = juce::Rectangle<float> (
        static_cast<float> (noiseCleanButton.getX()),
        static_cast<float> (noiseCleanButton.getBottom() + 3),
        static_cast<float> (learnNoiseButton.getRight() - noiseCleanButton.getX()), 7.0f);
    g.setColour (palette::ink.withAlpha (0.12f));
    g.fillRoundedRectangle (progress, 3.5f);
    const float learned = noise.isLearning() ? noise.getLearningProgress()
                        : noise.hasProfile() ? 1.0f : 0.0f;
    if (learned > 0.0f)
    {
        g.setColour (noise.isLearning() ? palette::orange : palette::green);
        g.fillRoundedRectangle (progress.withWidth (
            progress.getWidth() * juce::jlimit (0.0f, 1.0f, learned)), 3.5f);
    }
    g.setColour (noise.isLearning() ? palette::orange
                 : noise.hasProfile() ? palette::green : palette::muted);
    g.setFont (interFont (7.2f, true));
    const auto profileText = noise.isLearning()
        ? "LEARNING " + juce::String (juce::roundToInt (noise.getLearningProgress() * 100.0f)) + "%"
        : noise.hasProfile()
            ? "FROZEN  " + juce::String (noise.getEstimatedNoiseFloorDb(), 1) + " dB"
            : juce::String ("LEARN IN SILENCE");
    g.drawFittedText (profileText,
                      noiseCleanButton.getX(), noiseCleanButton.getBottom() + 10,
                      learnNoiseButton.getRight() - noiseCleanButton.getX(), 12,
                      juce::Justification::centred, 1);

    if (audioEngine.getInputChannelMode() == 2)
        drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());
]=])

sc220_p3_input_once(_ui "CH1 CH2 Dual Mono labels"
[=[
    g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
]=]
[=[
    if (audioEngine.getInputChannelMode() == 2)
    {
        g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                    sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    }
    else
    {
        g.drawText ("CH1  " + juce::String (sc220MonoLeftFader.getValue(), 1) + " dB",
                    sc220MonoLeftFader.getX() - 15, sc220MonoLeftFader.getBottom() + 2,
                    sc220MonoLeftFader.getWidth() + 30, 18, juce::Justification::centred);
        g.drawText ("CH2  " + juce::String (sc220MonoRightFader.getValue(), 1) + " dB",
                    sc220MonoRightFader.getX() - 15, sc220MonoRightFader.getBottom() + 2,
                    sc220MonoRightFader.getWidth() + 30, 18, juce::Justification::centred);
    }
]=])

foreach(_required IN ITEMS
        "SC220P3RuntimeState.h"
        "DENOISE ON"
        "noise.startLearning (2.0)"
        "LEARNING "
        "FROZEN  "
        "sc220MonoLeftFader"
        "sc220MonoRightFader"
        "dualMonoLeftFaderDb"
        "expander.setAmount (0.78f)")
    string(FIND "${_engine}${_ui}" "${_required}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "SC220 P3 input authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P3 input: direct frozen DENOISE/LEARN and independent Dual Mono")
