# Explicit PC-capture / loop-safe Stream + Monitor routing.
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(p3r_once variable_name label anchor replacement)
    set(_v "${${variable_name}}")
    string(FIND "${_v}" "${anchor}" _p)
    if(_p EQUAL -1)
        message(FATAL_ERROR "SC220 P3 routing anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _v "${_v}")
    set(${variable_name} "${_v}" PARENT_SCOPE)
endfunction()

function(p3r_range variable_name label begin_marker end_marker replacement)
    set(_v "${${variable_name}}")
    string(FIND "${_v}" "${begin_marker}" _b)
    string(FIND "${_v}" "${end_marker}" _e)
    if(_b EQUAL -1 OR _e EQUAL -1 OR _e LESS_EQUAL _b)
        message(FATAL_ERROR "SC220 P3 routing range changed: ${label}")
    endif()
    string(SUBSTRING "${_v}" 0 ${_b} _prefix)
    string(SUBSTRING "${_v}" ${_e} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

# A blank PC source must really be OFF. Older builds started SYSTEM DEFAULT even
# when the selector looked empty, which can recapture Stream when GO LIVE opens.
p3r_once(_engine "PC capture defaults off"
    "    programmeAnalyzer.setEnabled (true);\n"
    "    programmeAnalyzer.setEnabled (true);\n    pcAudioEnabled.store (false);\n")

set(_monitor [=[
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
        return "Monitor output must be a physical endpoint separate from Stream Output.";

    bool disabledPc = false;
    if (pcAudioEnabled.load()
        && pcEndpointConflictsWithMonitor (pcLoopback.selectedEndpointName(), requested))
    {
        pcLoopback.stop();
        pcAudioEnabled.store (false);
        pcAudioLoopBlocked.store (true);
        disabledPc = true;
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio switched OFF because Source equals Monitor Output. This prevents direct-plus-DSP doubling/echo.";
    }

    const auto error = monitorOutput.selectOutputDevice (requested, currentSampleRate.load());
    if (error.isEmpty() && ! disabledPc)
    {
        pcAudioLoopBlocked.store (false);
        const juce::ScopedLock lock (statusLock);
        if (pcLoopback.lastError().isEmpty())
            pcAudioRoutingError.clear();
    }
    return error;
}
]=])
p3r_range(_engine "Monitor wins a source conflict"
    "juce::String LiveAudioEngine::selectMonitorOutputDevice"
    "juce::String LiveAudioEngine::applyDeviceSelection"
    "${_monitor}")

set(_names [=[
juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;

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
p3r_range(_engine "PC list contains explicit safe endpoints only"
    "juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "${_names}")

set(_select [=[
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
            "PC Audio capture is OFF because the selected source overlaps Stream Output.";
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
            "PC Audio capture is OFF because Source equals Monitor Output. This prevents direct-plus-DSP doubling/echo.";
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    if (pcAudioEnabled.load() && pcLoopback.isRunning()
        && PcAudioLoopbackCapture::endpointNamesMatch (
            pcLoopback.selectedEndpointName(), requested))
        return {};

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
p3r_range(_engine "PC capture is explicit and transaction-safe"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "juce::String LiveAudioEngine::getPcAudioEndpointName() const"
    "${_select}")

p3r_once(_engine "PC source name is blank when disabled"
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

# The old primary-output guard rejected Stream. The correct live policy keeps
# Stream authoritative and shuts down a stale conflicting capture instead.
set(_old_guard [=[
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
set(_new_guard [=[
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
p3r_once(_engine "Stream selection disables a stale capture"
    "${_old_guard}" "${_new_guard}")

set(_start [=[
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
            pcAudioRoutingError = "Saved PC Audio source is OFF because it conflicts with Stream or Monitor.";
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
p3r_range(_engine "audio start respects PC Audio OFF"
    "    pcLoopback.prepare (sampleRate);"
    "    publishLatencyEstimate();"
    "${_start}")

# Migrate legacy SYSTEM DEFAULT pseudo-selection to OFF.
set(_legacy [=[
            audioEngine->selectPcAudioEndpoint (file->getValue (
                "pcAudioEndpoint", PcAudioLoopbackCapture::systemDefaultLabel));
]=])
set(_safe [=[
            const auto savedPcAudioEndpoint = file->getValue ("pcAudioEndpoint").trim();
            if (savedPcAudioEndpoint.isNotEmpty()
                && ! savedPcAudioEndpoint.equalsIgnoreCase (
                    PcAudioLoopbackCapture::systemDefaultLabel))
                audioEngine->selectPcAudioEndpoint (savedPcAudioEndpoint);
            else
                audioEngine->setPcAudioEnabled (false);
]=])
p3r_once(_app "legacy default capture migrates to OFF" "${_legacy}" "${_safe}")

# GO LIVE and direct Stream unmute both validate an active source first.
set(_go_old [=[
    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
        if (goLiveButton.getToggleState())
            audioEngine.resetProgrammeLoudness();
        audioEngine.setOutputMuted (! goLiveButton.getToggleState());
        goLiveButton.setButtonText (goLiveButton.getToggleState() ? "LIVE" : "GO LIVE");
    };
]=])
set(_go_new [=[
    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
        if (goLiveButton.getToggleState())
        {
            if (audioEngine.isPcAudioEnabled())
            {
                const auto error = audioEngine.selectPcAudioEndpoint (audioEngine.getPcAudioEndpointName());
                if (error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon, "PC Audio switched off", error);
            }
            audioEngine.resetProgrammeLoudness();
        }
        audioEngine.setOutputMuted (! goLiveButton.getToggleState());
        goLiveButton.setButtonText (goLiveButton.getToggleState() ? "LIVE" : "GO LIVE");
        refreshAllControls();
    };
]=])
p3r_once(_ui "GO LIVE routing check" "${_go_old}" "${_go_new}")

p3r_once(_ui "Stream MUTE routing check"
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
            const auto error = audioEngine.selectPcAudioEndpoint (audioEngine.getPcAudioEndpointName());
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "PC Audio switched off", error);
        }
        audioEngine.setOutputMuted (streamMute.getToggleState());
        refreshAllControls();
    };
]=])

# Generated PC status block from GenerateSC220PcEndpointRouting.cmake.
set(_state_old [=[
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
set(_state_new [=[
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
p3r_once(_ui "truthful PC AUDIO OFF state" "${_state_old}" "${_state_new}")

foreach(_required IN ITEMS
        "pcAudioEnabled.store (false)"
        "PC AUDIO OFF"
        "PC Audio switched OFF"
        "direct-plus-DSP doubling/echo")
    string(FIND "${_engine}${_ui}${_app}" "${_required}" _p)
    if(_p EQUAL -1)
        message(FATAL_ERROR "SC220 P3 routing authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P3 routing: PC Audio is explicit and Stream/Monitor is loop-safe")
