# P3 routing authority: PC playback capture is explicit opt-in and may never
# become a delayed second copy of Stream or Monitor audio.

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p3_route_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P3 routing anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p3_route_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 P3 routing range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

# Header default is intentionally overridden at runtime because older settings
# files could look visually unselected while SYSTEM DEFAULT loopback was active.
sc220_p3_route_once(_engine "PC capture starts disabled"
    "    programmeAnalyzer.setEnabled (true);\n"
    "    programmeAnalyzer.setEnabled (true);\n    pcAudioEnabled.store (false);\n")

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

    bool pcWasDisabled = false;
    if (pcAudioEnabled.load()
        && pcEndpointConflictsWithMonitor (pcLoopback.selectedEndpointName(), requested))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        pcWasDisabled = true;
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio switched OFF because it matched Monitor Output. This prevents direct-plus-DSP doubling/echo.";
    }

    const auto error = monitorOutput.selectOutputDevice (requested, currentSampleRate.load());
    if (error.isEmpty() && ! pcWasDisabled)
    {
        pcAudioLoopBlocked.store (false);
        const juce::ScopedLock lock (statusLock);
        if (pcLoopback.lastError().isEmpty())
            pcAudioRoutingError.clear();
    }
    return error;
}
]=])
sc220_p3_route_range(_engine "Monitor wins conflicting PC capture"
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

    // No SYSTEM DEFAULT pseudo-source in live mode. One explicit playback
    // endpoint is either selected by the user, or PC Audio is OFF.
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
sc220_p3_route_range(_engine "explicit PC playback endpoint list"
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
            "PC Audio capture is OFF because this endpoint overlaps the Stream/VB-CABLE path.";
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
            "PC Audio capture is OFF because Source equals Monitor Output. That route creates direct-plus-DSP doubling/echo.";
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
sc220_p3_route_range(_engine "explicit opt-in PC capture transaction"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "juce::String LiveAudioEngine::getPcAudioEndpointName() const"
    "${_pc_select}")

sc220_p3_route_once(_engine "empty PC source name means OFF"
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

# Do not reject a valid Stream selection because of stale PC capture. Stream is
# authoritative; disable the conflicting loopback and continue the transaction.
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
            "PC Audio switched OFF because the new Stream Output would be captured back into the mix.";
    }
]=])
sc220_p3_route_once(_engine "Stream disables stale conflicting capture"
    "${_old_stream_guard}" "${_new_stream_guard}")

# At audio-device start, a loopback worker exists only for an explicitly enabled
# endpoint. This is the critical fix for a blank selector that previously still
# displayed PLAYBACK CAPTURE ACTIVE.
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
                "Saved PC Audio endpoint stayed OFF because it conflicts with Stream or Monitor routing.";
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
sc220_p3_route_range(_engine "start loopback only when PC Audio is enabled"
    "    pcLoopback.prepare (sampleRate);"
    "    const int voiceLatency ="
    "${_pc_start}")

# Migrate the old pseudo-selection to OFF. Users who actually need PC music now
# make an explicit, visible source choice.
set(_legacy_restore [=[
            audioEngine->selectPcAudioEndpoint (file->getValue (
                "pcAudioEndpoint", PcAudioLoopbackCapture::systemDefaultLabel));
]=])
set(_safe_restore [=[
            const auto savedPcAudioEndpoint = file->getValue ("pcAudioEndpoint").trim();
            if (savedPcAudioEndpoint.isNotEmpty()
                && ! savedPcAudioEndpoint.equalsIgnoreCase (
                    PcAudioLoopbackCapture::systemDefaultLabel))
                audioEngine->selectPcAudioEndpoint (savedPcAudioEndpoint);
            else
                audioEngine->setPcAudioEnabled (false);
]=])
sc220_p3_route_once(_app "legacy SYSTEM DEFAULT migrates to OFF"
    "${_legacy_restore}" "${_safe_restore}")

# The user can release Stream from either GO LIVE or the Stream MUTE button.
# Re-selecting the same explicit PC endpoint is a zero-work validation when safe;
# an unsafe endpoint is stopped before stream samples leave the app.
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
sc220_p3_route_once(_ui "GO LIVE revalidates capture topology"
    "${_go_live_old}" "${_go_live_new}")

sc220_p3_route_once(_ui "Stream MUTE revalidates capture topology"
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

# Honest status: no source selection means PC Audio OFF, never hidden capture.
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
                           : "PLAYBACK CAPTURE STARTING";
]=])
sc220_p3_route_once(_ui "PC selector OFF state is truthful"
    "${_pc_state_old}" "${_pc_state_new}")

foreach(_required IN ITEMS
        "pcAudioEnabled.store (false)"
        "PC AUDIO OFF"
        "No SYSTEM DEFAULT pseudo-source"
        "direct-plus-DSP doubling/echo"
        "GO LIVE revalidates")
    string(FIND "${_engine}${_ui}${_app}${CMAKE_CURRENT_LIST_FILE}" "${_required}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "SC220 P3 routing authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P3 routing: explicit PC capture and loop-safe Stream/Monitor")
