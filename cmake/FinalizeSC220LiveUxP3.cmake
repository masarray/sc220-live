# Final hardware-test authority for SC220 Live P3.
#
# Goals:
#   * PC Audio is opt-in, never a hidden SYSTEM DEFAULT loopback source.
#   * GO LIVE / Stream unmute revalidates PC capture so Stream cannot feed back
#     into the physical monitor as a delayed second copy.
#   * Monitor wins routing conflicts: a conflicting PC capture is disabled.
#   * Noise learning is visible/direct, explicit-only and frozen afterwards.
#   * Dual Mono exposes two independent channel faders while retaining L/R meters.
#   * The loudness dial is live LUFS-M; LUFS-I remains a retained secondary value.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P3 finalizer requires engine, UI and app sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p3_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p3_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 P3 range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Shared lock-free P3 runtime state.
# -----------------------------------------------------------------------------
sc220_p3_replace_once(_engine "engine P3 state include"
    "#include \"ProgrammeBandTelemetry.h\"\n"
    "#include \"ProgrammeBandTelemetry.h\"\n#include \"SC220P3RuntimeState.h\"\n")
sc220_p3_replace_once(_ui "UI P3 state include"
    "#include \"ProgrammeBandTelemetry.h\"\n"
    "#include \"ProgrammeBandTelemetry.h\"\n#include \"SC220P3RuntimeState.h\"\n")

# Never start with a hidden SYSTEM DEFAULT capture. A saved explicit endpoint can
# opt PC Audio back in before the audio device starts.
sc220_p3_replace_once(_engine "PC Audio defaults off"
    "    programmeAnalyzer.setEnabled (true);\n"
    "    programmeAnalyzer.setEnabled (true);\n    pcAudioEnabled.store (false);\n")

# -----------------------------------------------------------------------------
# PC Audio / Monitor / Stream topology. Physical Monitor is authoritative.
# -----------------------------------------------------------------------------
set(_monitor_select [=[
juce::String LiveAudioEngine::selectMonitorOutputDevice (const juce::String& name)
{
    const auto requested = name.trim();
    if (requested.isEmpty())
    {
        monitorOutput.setMuted (true);
        return monitorOutput.selectOutputDevice ({}, currentSampleRate.load());
    }

    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    if (requested.equalsIgnoreCase (streamOutput)
        || looksLikeVirtualCable (requested)
        || MonitorOutputClient::isUnsafeVirtualOutput (requested))
        return "Monitor output must be a physical endpoint separate from the VB-CABLE stream output.";

    bool disabledConflictingPcSource = false;
    if (pcAudioEnabled.load()
        && pcEndpointConflictsWithMonitor (pcLoopback.selectedEndpointName(), requested))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        disabledConflictingPcSource = true;
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio was switched OFF because it resolved to the selected Monitor Output. "
            "This prevents direct-plus-DSP doubling/echo. Select a different PC Audio playback endpoint if music capture is required.";
    }

    const auto error = monitorOutput.selectOutputDevice (requested, currentSampleRate.load());
    if (error.isEmpty() && ! disabledConflictingPcSource)
    {
        pcAudioLoopBlocked.store (false);
        const juce::ScopedLock lock (statusLock);
        if (pcLoopback.lastError().isEmpty())
            pcAudioRoutingError.clear();
    }
    return error;
}
]=])
sc220_p3_replace_range(_engine "Monitor wins PC-source conflicts"
    "juce::String LiveAudioEngine::selectMonitorOutputDevice"
    "juce::String LiveAudioEngine::applyDeviceSelection"
    "${_monitor_select}")

set(_pc_names [=[
juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;

    // Deterministic live policy: no SYSTEM DEFAULT entry. The user explicitly
    // chooses one real Windows playback endpoint, or leaves PC Audio OFF.
    juce::StringArray names;
    for (const auto& endpoint : PcAudioLoopbackCapture::availableEndpoints())
    {
        if (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (endpoint.name)
            || PcAudioLoopbackCapture::endpointWouldConflict (endpoint.name, streamOutput)
            || pcEndpointConflictsWithMonitor (endpoint.name, monitorDevice))
            continue;
        names.addIfNotAlreadyThere (endpoint.name);
    }
    return names;
}
]=])
sc220_p3_replace_range(_engine "explicit-only PC Audio endpoint list"
    "juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "${_pc_names}")

set(_pc_select [=[
juce::String LiveAudioEngine::selectPcAudioEndpoint (const juce::String& name)
{
    const auto requested = name.trim();
    if (requested.isEmpty()
        || requested.equalsIgnoreCase (PcAudioLoopbackCapture::systemDefaultLabel))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (false);
        monitorOutput.flush();
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError.clear();
        return {};
    }

    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;

    if (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (requested)
        || PcAudioLoopbackCapture::endpointWouldConflict (requested, streamOutput))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        const juce::String message =
            "PC Audio cannot capture the active Stream/VB-CABLE path. Capture is OFF to prevent a delayed feedback copy.";
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    if (pcEndpointConflictsWithMonitor (requested, monitorDevice))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        const juce::String message =
            "PC Audio Source cannot equal Monitor Output. Capture is OFF to prevent direct-plus-DSP doubling/echo.";
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    if (pcAudioEnabled.load() && pcLoopback.isRunning()
        && PcAudioLoopbackCapture::endpointNamesMatch (
            pcLoopback.selectedEndpointName(), requested))
    {
        pcAudioLoopBlocked.store (false);
        return {};
    }

    const auto previous = pcLoopback.requestedEndpointName();
    const double rate = currentSampleRate.load();
    pcLoopback.stop();
    monitorOutput.flush();
    auto error = pcLoopback.selectEndpoint (requested);
    if (error.isNotEmpty())
    {
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = error;
        return error;
    }

    pcAudioEnabled.store (true);
    pcAudioLoopBlocked.store (false);
    {
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError.clear();
    }

    if (rate > 0.0)
    {
        pcLoopback.prepare (rate);
        if (! pcLoopback.start())
        {
            error = pcLoopback.lastError();
            pcAudioEnabled.store (false);
            pcAudioLoopBlocked.store (true);
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError = error;
        }
    }
    loudnessResetRequested.store (true, std::memory_order_release);
    return error;
}
]=])
sc220_p3_replace_range(_engine "explicit opt-in PC Audio selection"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "juce::String LiveAudioEngine::getPcAudioEndpointName() const"
    "${_pc_select}")

sc220_p3_replace_once(_engine "blank endpoint name when PC Audio is off"
[=[
juce::String LiveAudioEngine::getPcAudioEndpointName() const
{
    return pcLoopback.selectedEndpointName();
}
]=]
[=[
juce::String LiveAudioEngine::getPcAudioEndpointName() const
{
    return pcAudioEnabled.load() ? pcLoopback.selectedEndpointName() : juce::String();
}
]=])

# Selecting a Stream endpoint is allowed. If an old PC capture points at that
# endpoint, disable the capture instead of rejecting the Stream change.
set(_old_stream_guard [=[
    if (outputName != nullptr
        && PcAudioLoopbackCapture::endpointWouldConflict (
            pcLoopback.requestedEndpointName(), outputName->trim()))
    {
        const juce::String message =
            "The Stream output cannot be the same endpoint captured by PC Audio. "
            "Keep Stream on VB-CABLE and capture a physical playback endpoint.";
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }
]=])
set(_new_stream_guard [=[
    if (outputName != nullptr && pcAudioEnabled.load()
        && PcAudioLoopbackCapture::endpointWouldConflict (
            pcLoopback.selectedEndpointName(), outputName->trim()))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio was switched OFF because the new Stream Output would be captured back into the mix.";
    }
]=])
sc220_p3_replace_once(_engine "Stream change disables conflicting PC capture"
    "${_old_stream_guard}" "${_new_stream_guard}")

# Audio start honours the explicit PC Audio enable state. No hidden default
# capture thread exists simply because the engine is running.
set(_pc_start [=[
    pcLoopback.prepare (sampleRate);
    if (pcAudioEnabled.load())
    {
        juce::String streamOutput;
        {
            const juce::ScopedLock lock (statusLock);
            streamOutput = currentOutputName;
        }
        const auto monitorDevice = monitorOutput.getStatus().deviceName;
        const auto source = pcLoopback.requestedEndpointName();
        if (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (source)
            || PcAudioLoopbackCapture::endpointWouldConflict (source, streamOutput)
            || pcEndpointConflictsWithMonitor (source, monitorDevice))
        {
            pcAudioEnabled.store (false);
            pcAudioLoopBlocked.store (true);
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError =
                "PC Audio capture stayed OFF because its saved endpoint conflicts with Stream or Monitor routing.";
        }
        else if (! pcLoopback.start())
        {
            pcAudioEnabled.store (false);
            pcAudioLoopBlocked.store (true);
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError = pcLoopback.lastError();
        }
    }
    else
    {
        pcAudioLoopBlocked.store (false);
    }
]=])
sc220_p3_replace_range(_engine "start PC loopback only when explicitly enabled"
    "    pcLoopback.prepare (sampleRate);"
    "    const int voiceLatency ="
    "${_pc_start}")

# Generated application migration: older builds persisted SYSTEM DEFAULT even
# when the selector looked empty. Treat that legacy value as OFF. Only a named,
# explicit physical endpoint is restored.
set(_legacy_pc_restore [=[
            audioEngine->selectPcAudioEndpoint (file->getValue (
                "pcAudioEndpoint", PcAudioLoopbackCapture::systemDefaultLabel));
]=])
set(_safe_pc_restore [=[
            const auto savedPcAudioEndpoint = file->getValue ("pcAudioEndpoint").trim();
            if (savedPcAudioEndpoint.isNotEmpty()
                && ! savedPcAudioEndpoint.equalsIgnoreCase (
                    PcAudioLoopbackCapture::systemDefaultLabel))
                audioEngine->selectPcAudioEndpoint (savedPcAudioEndpoint);
            else
                audioEngine->setPcAudioEnabled (false);
]=])
sc220_p3_replace_once(_app "migrate hidden SYSTEM DEFAULT capture to OFF"
    "${_legacy_pc_restore}" "${_safe_pc_restore}")

# -----------------------------------------------------------------------------
# Frozen-profile denoise + smooth learned-floor expander.
# -----------------------------------------------------------------------------
sc220_p3_replace_once(_engine "natural silence cleanup tuning"
    "    expander.prepare (sampleRate);\n"
    "    expander.prepare (sampleRate);\n    expander.setAmount (0.78f);\n    expander.setThresholdOffsetDb (5.5f);\n")

# -----------------------------------------------------------------------------
# Dual Mono: retain two real input channels but give each an independent fader.
# Existing StereoMeter continues to show independent L/R bars.
# -----------------------------------------------------------------------------
set(_linked_sc220_gain [=[
    sc220MixGainLinear.setTargetValue (juce::Decibels::decibelsToGain (sc220FaderDb.load()));
    const float sc220MuteGain = sc220Muted.load() ? 0.0f : 1.0f;
    const float sc220GainStart = sc220MixGainLinear.getCurrentValue() * sc220MuteGain;
    const float sc220GainEnd = sc220MixGainLinear.skip (numSamples) * sc220MuteGain;
    voiceBlock.applyGainRamp (0, numSamples, sc220GainStart, sc220GainEnd);
]=])
set(_dual_sc220_gain [=[
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
sc220_p3_replace_once(_engine "independent Dual Mono faders"
    "${_linked_sc220_gain}" "${_dual_sc220_gain}")

# -----------------------------------------------------------------------------
# Loudness: preserve integrated history but publish live 400 ms LUFS-M for the
# dial. Silence therefore clears the live value after about 400 ms.
# -----------------------------------------------------------------------------
sc220_p3_replace_once(_engine "publish momentary loudness"
    "    loudnessLufs.store (loudnessIntegrator.integratedLufs());\n"
    "    sc220P3RuntimeState.momentaryLufs.store (loudnessIntegrator.momentaryLufs());\n    loudnessLufs.store (loudnessIntegrator.integratedLufs());\n")
string(REPLACE
    "        loudnessLufs.store (-120.0f);"
    "        loudnessLufs.store (-120.0f);\n        sc220P3RuntimeState.momentaryLufs.store (-120.0f);"
    _engine "${_engine}")
string(REPLACE
    "    loudnessLufs.store (-120.0f);"
    "    loudnessLufs.store (-120.0f);\n    sc220P3RuntimeState.momentaryLufs.store (-120.0f);"
    _engine "${_engine}")

# -----------------------------------------------------------------------------
# Direct SC220 controls: DENOISE / LEARN plus two Dual-Mono faders.
# -----------------------------------------------------------------------------
set(_visible_controls_anchor [=[
             &stereoButton, &dualMonoButton, &pcSourceBox,
             &programMonitorButton, &musicCueButton, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob,
]=])
set(_visible_controls_replacement [=[
             &stereoButton, &dualMonoButton, &noiseCleanButton, &learnNoiseButton,
             &pcSourceBox, &programMonitorButton, &musicCueButton, &inputTrim,
             &sc220Fader, &sc220MonoLeftFader, &sc220MonoRightFader,
             &pcFader, &duckAmount, &streamFader, &monitorKnob,
]=])
sc220_p3_replace_once(_ui "visible denoise and Dual Mono controls"
    "${_visible_controls_anchor}" "${_visible_controls_replacement}")

sc220_p3_replace_once(_ui "configure Dual Mono faders"
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

set(_mode_callbacks [=[
    stereoButton.onClick = [this] { audioEngine.setInputChannelMode (2); };
    dualMonoButton.onClick = [this] { audioEngine.setInputChannelMode (1); };
]=])
set(_mode_noise_callbacks [=[
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
    noiseCleanButton.setTooltip ("Frozen learned-noise expander on / off");
    noiseCleanButton.onClick = [this]
    {
        const bool enabled = noiseCleanButton.getToggleState();
        audioEngine.getNoiseClean().setEnabled (enabled);
        audioEngine.getExpander().setEnabled (enabled);
        repaint (sc220StripArea);
    };

    learnNoiseButton.setTooltip ("Capture 2 seconds of silent SC220 static noise");
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
sc220_p3_replace_once(_ui "mode and direct Noise Clean callbacks"
    "${_mode_callbacks}" "${_mode_noise_callbacks}")

sc220_p3_replace_once(_ui "Dual Mono fader callbacks"
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

# GO LIVE and the separate Stream MUTE button both revalidate an active PC
# capture. Any unsafe route is switched OFF before stream samples are released.
set(_go_live_old [=[
    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
        if (goLiveButton.getToggleState())
            audioEngine.resetProgrammeLoudness();
        audioEngine.setOutputMuted (! goLiveButton.getToggleState());
        goLiveButton.setButtonText (goLiveButton.getToggleState() ? "LIVE" : "GO LIVE");
    };
]=])
set(_go_live_new [=[
    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
        if (goLiveButton.getToggleState())
        {
            if (audioEngine.isPcAudioEnabled())
            {
                const auto routeError = audioEngine.selectPcAudioEndpoint (
                    audioEngine.getPcAudioEndpointName());
                if (routeError.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "PC Audio switched off", routeError);
            }
            audioEngine.resetProgrammeLoudness();
        }
        audioEngine.setOutputMuted (! goLiveButton.getToggleState());
        goLiveButton.setButtonText (goLiveButton.getToggleState() ? "LIVE" : "GO LIVE");
        refreshAllControls();
    };
]=])
sc220_p3_replace_once(_ui "GO LIVE topology revalidation"
    "${_go_live_old}" "${_go_live_new}")

sc220_p3_replace_once(_ui "Stream MUTE topology revalidation"
[=[
    streamMute.setClickingTogglesState (true);
    streamMute.onClick = [this] { audioEngine.setOutputMuted (streamMute.getToggleState()); };
]=]
[=[
    streamMute.setClickingTogglesState (true);
    streamMute.onClick = [this]
    {
        if (! streamMute.getToggleState() && audioEngine.isPcAudioEnabled())
        {
            const auto routeError = audioEngine.selectPcAudioEndpoint (
                audioEngine.getPcAudioEndpointName());
            if (routeError.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "PC Audio switched off", routeError);
        }
        audioEngine.setOutputMuted (streamMute.getToggleState());
        refreshAllControls();
    };
]=])

# Refresh direct controls and switch between one linked stereo fader and two
# independent Dual Mono faders.
sc220_p3_replace_once(_ui "refresh Dual Mono values"
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

sc220_p3_replace_once(_ui "refresh mode and denoise states"
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

# Hide hidden/default capture state in the UI. An empty selector must mean OFF.
set(_pc_state_old [=[
    const bool pcReady = lastStatus.pcLoopbackRunning
                      && ! lastStatus.pcAudioLoopBlocked
                      && lastStatus.pcAudioError.isEmpty();
    g.setColour (pcReady ? palette::green
                         : lastStatus.pcAudioLoopBlocked ? palette::red : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 73),
                   static_cast<float> (pcStripArea.getY() + 70), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.8f, true));
    const auto pcStateText = pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : lastStatus.pcAudioError.isNotEmpty()
                               ? "CAPTURE ERROR / RETRYING"
                               : "PLAYBACK CAPTURE STARTING";
]=])
set(_pc_state_new [=[
    const bool pcEnabled = audioEngine.isPcAudioEnabled();
    const bool pcReady = pcEnabled && lastStatus.pcLoopbackRunning
                      && ! lastStatus.pcAudioLoopBlocked
                      && lastStatus.pcAudioError.isEmpty();
    g.setColour (! pcEnabled ? palette::muted
                 : pcReady ? palette::green
                 : lastStatus.pcAudioLoopBlocked ? palette::red : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 73),
                   static_cast<float> (pcStripArea.getY() + 70), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.8f, true));
    const auto pcStateText = ! pcEnabled ? "PC AUDIO OFF"
                           : pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : lastStatus.pcAudioError.isNotEmpty()
                               ? "CAPTURE ERROR / RETRYING"
                               : "PLAYBACK CAPTURE STARTING";
]=])
sc220_p3_replace_once(_ui "honest PC Audio OFF state"
    "${_pc_state_old}" "${_pc_state_new}")

# Responsive SC220 strip layout. Noise controls occupy the deliberate 50 px gap
# between the Stereo/Dual-Mono row and the fader area.
set(_sc220_layout_old [=[
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
set(_sc220_layout_new [=[
    stereoButton.setBounds (sc220StripArea.getX() + 10, sourceSelectorY,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 42,
                         sourceSelectorY - 94, 84, 84);

    const int noiseRowY = sourceSelectorY + 38;
    const int noiseWidth = (sc220StripArea.getWidth() - 26) / 2;
    noiseCleanButton.setBounds (sc220StripArea.getX() + 10, noiseRowY,
                                noiseWidth, 27);
    learnNoiseButton.setBounds (noiseCleanButton.getRight() + 6, noiseRowY,
                                noiseWidth, 27);

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
sc220_p3_replace_once(_ui "direct Noise / Dual Mono geometry"
    "${_sc220_layout_old}" "${_sc220_layout_new}")

# Draw a compact profile progress bar and channel-specific Dual Mono labels.
sc220_p3_replace_once(_ui "noise profile progress and mode-aware fader scale"
[=[
    drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());
]=]
[=[
    const auto& noise = audioEngine.getNoiseClean();
    auto noiseProgress = juce::Rectangle<float> (
        static_cast<float> (noiseCleanButton.getX()),
        static_cast<float> (noiseCleanButton.getBottom() + 3),
        static_cast<float> (learnNoiseButton.getRight() - noiseCleanButton.getX()), 8.0f);
    g.setColour (palette::ink.withAlpha (0.12f));
    g.fillRoundedRectangle (noiseProgress, 4.0f);
    const float profileProgress = noise.isLearning() ? noise.getLearningProgress()
                                : noise.hasProfile() ? 1.0f : 0.0f;
    if (profileProgress > 0.0f)
    {
        g.setColour (noise.isLearning() ? palette::orange : palette::green);
        g.fillRoundedRectangle (noiseProgress.withWidth (
            noiseProgress.getWidth() * juce::jlimit (0.0f, 1.0f, profileProgress)), 4.0f);
    }
    g.setColour (noise.isLearning() ? palette::orange
                 : noise.hasProfile() ? palette::green : palette::muted);
    g.setFont (interFont (7.4f, true));
    const auto noiseState = noise.isLearning()
        ? "LEARNING STATIC NOISE  "
          + juce::String (juce::roundToInt (noise.getLearningProgress() * 100.0f)) + "%"
        : noise.hasProfile()
            ? "PROFILE FROZEN  " + juce::String (noise.getEstimatedNoiseFloorDb(), 1) + " dB"
            : juce::String ("PRESS LEARN IN SILENCE");
    g.drawFittedText (noiseState,
                      noiseCleanButton.getX(), noiseCleanButton.getBottom() + 11,
                      learnNoiseButton.getRight() - noiseCleanButton.getX(), 13,
                      juce::Justification::centred, 1);

    if (audioEngine.getInputChannelMode() == 2)
        drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());
]=])

sc220_p3_replace_once(_ui "mode-aware SC220 fader labels"
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
        g.drawText ("CH 1  " + juce::String (sc220MonoLeftFader.getValue(), 1) + " dB",
                    sc220MonoLeftFader.getX() - 16, sc220MonoLeftFader.getBottom() + 2,
                    sc220MonoLeftFader.getWidth() + 32, 18, juce::Justification::centred);
        g.drawText ("CH 2  " + juce::String (sc220MonoRightFader.getValue(), 1) + " dB",
                    sc220MonoRightFader.getX() - 16, sc220MonoRightFader.getBottom() + 2,
                    sc220MonoRightFader.getWidth() + 32, 18, juce::Justification::centred);
    }
]=])

# Replace the retained LUFS-I-only dial with live LUFS-M movement. LUFS-I remains
# visible as history, but silence clears the large number/needle after 400 ms.
set(_loudness_class [=[
class SC220MainComponent::LoudnessDisplay final : public juce::Component
{
public:
    void setReading (float integratedValue, float momentaryValue, float nextTruePeak,
                     float nextElapsedSeconds, bool nextReady)
    {
        integratedLufs = integratedValue;
        momentaryLufs = momentaryValue;
        truePeak = nextTruePeak;
        elapsedSeconds = nextElapsedSeconds;
        ready = nextReady;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        const auto dial = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                                       juce::jmin (area.getWidth(), area.getHeight()));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (dial.translated (0.0f, 4.0f));
        juce::ColourGradient rim (palette::metalHi, dial.getCentreX(), dial.getY(),
                                  palette::metalLo, dial.getCentreX(), dial.getBottom(), false);
        rim.addColour (0.45, juce::Colour (0xffd1d2ce));
        g.setGradientFill (rim);
        g.fillEllipse (dial);
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.drawEllipse (dial, 1.2f);

        const auto face = dial.reduced (6.0f);
        juce::ColourGradient faceFill (juce::Colour (0xff262929), face.getCentreX(), face.getY(),
                                       juce::Colour (0xff0a0b0b), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (faceFill);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (face.reduced (1.0f), 1.0f);

        constexpr float start = juce::MathConstants<float>::pi * 0.72f;
        constexpr float end = juce::MathConstants<float>::pi * 2.28f;
        const bool liveProgramme = std::isfinite (momentaryLufs) && momentaryLufs > -70.0f;
        const float liveValue = liveProgramme ? momentaryLufs : -36.0f;
        const float normal = liveProgramme
            ? juce::jlimit (0.0f, 1.0f, (liveValue + 36.0f) / 30.0f) : 0.0f;
        const float radius = face.getWidth() * 0.44f;
        const auto centre = dial.getCentre();
        for (int i = 0; i < 32; ++i)
        {
            const float t = static_cast<float> (i) / 31.0f;
            const float angle = start + t * (end - start);
            const auto colour = t > 0.83f ? palette::red : t > 0.62f ? palette::amber : palette::green;
            g.setColour (liveProgramme && i <= juce::roundToInt (normal * 31.0f)
                         ? colour : colour.withAlpha (0.16f));
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.83f, angle),
                          centre.getPointOnCircumference (radius, angle) }, 4.0f);
        }

        if (liveProgramme)
        {
            const auto needleAngle = start + normal * (end - start);
            const auto needleStart = centre.getPointOnCircumference (radius * 0.53f, needleAngle);
            const auto needleEnd = centre.getPointOnCircumference (radius * 1.02f, needleAngle);
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.drawLine ({ needleStart.translated (1.0f, 1.5f),
                          needleEnd.translated (1.0f, 1.5f) }, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.drawLine ({ needleStart, needleEnd }, 2.0f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (interFont (dial.getWidth() * 0.20f, true));
        g.drawText (liveProgramme ? juce::String (momentaryLufs, 1) : juce::String ("--"),
                    dial.withTrimmedTop (dial.getHeight() * 0.28f).withHeight (dial.getHeight() * 0.25f),
                    juce::Justification::centred);
        g.setFont (interFont (dial.getWidth() * 0.070f, true));
        g.drawText ("LUFS-M", dial.withTrimmedTop (dial.getHeight() * 0.52f).withHeight (22.0f),
                    juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (interFont (dial.getWidth() * 0.054f));
        const auto integratedText = integratedLufs <= -100.0f
            ? juce::String ("I --") : "I " + juce::String (integratedLufs, 1);
        const auto peakText = truePeak <= -100.0f
            ? juce::String ("TP --") : "TP " + juce::String (truePeak, 1);
        g.drawText (integratedText + "  /  " + peakText,
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);

        g.setColour (palette::orange.withAlpha (0.92f));
        g.setFont (interFont (dial.getWidth() * 0.050f, true));
        g.drawText ("YT / TIKTOK  -14  |  TP <= -1",
                    dial.withTrimmedTop (dial.getHeight() * 0.14f).withHeight (18.0f),
                    juce::Justification::centred);

        if (! liveProgramme)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (interFont (dial.getWidth() * 0.050f));
            const auto state = elapsedSeconds < 0.4f && ! ready
                ? juce::String ("CALIBRATING") : juce::String ("PROGRAM SILENCE");
            g.drawText (state,
                        dial.withTrimmedTop (dial.getHeight() * 0.78f).withHeight (18.0f),
                        juce::Justification::centred);
        }
    }

private:
    float integratedLufs = -120.0f;
    float momentaryLufs = -120.0f;
    float truePeak = -120.0f;
    float elapsedSeconds = 0.0f;
    bool ready = false;
};
]=])
sc220_p3_replace_range(_ui "live momentary loudness display"
    "class SC220MainComponent::LoudnessDisplay final : public juce::Component"
    "SC220MainComponent::SC220MainComponent"
    "${_loudness_class}")

sc220_p3_replace_once(_ui "feed LUFS-M and LUFS-I"
[=[
    loudnessDisplay->setReading (lastStatus.loudnessLufs, lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=]
[=[
    loudnessDisplay->setReading (lastStatus.loudnessLufs,
                                 sc220P3RuntimeState.momentaryLufs.load(),
                                 lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
]=])

# Final semantic checks: fail configuration if the generated product drifts back
# toward hidden PC capture, adaptive denoise, linked-only Dual Mono or stale LUFS.
foreach(_required IN ITEMS
        "SC220P3RuntimeState.h"
        "pcAudioEnabled.store (false)"
        "PC AUDIO OFF"
        "DENOISE ON"
        "LEARNING STATIC NOISE"
        "PROFILE FROZEN"
        "dualMonoLeftFaderDb"
        "sc220MonoLeftFader"
        "sc220MonoRightFader"
        "momentaryLufs()"
        "LUFS-M"
        "PROGRAM SILENCE"
        "direct-plus-DSP doubling/echo")
    string(FIND "${_engine}${_ui}${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "pcAudioEndpoint\", PcAudioLoopbackCapture::systemDefaultLabel")
    string(FIND "${_app}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 legacy hidden PC capture remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 Live P3: explicit PC capture, loop-safe monitor, direct denoise, Dual Mono and live LUFS generated")
