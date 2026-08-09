# Apply OBS-style do-not-self-monitor routing policy after PC endpoint and final
# UI generation. Capturing and monitoring the same render endpoint necessarily
# duplicates direct Windows playback and recursively captures the DSP monitor.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 PC/monitor guard requires final engine and UI sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_self_monitor_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 self-monitor guard anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_self_monitor_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 self-monitor guard range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

set(_helper_anchor [=[
bool looksLikeSc220Endpoint (const juce::String& endpointName)
]=])
set(_helper_replacement [=[
bool pcEndpointConflictsWithMonitor (const juce::String& requestedEndpoint,
                                     const juce::String& monitorOutput)
{
    if (monitorOutput.trim().isEmpty())
        return false;

    const auto source = resolvePcPlaybackEndpoint (requestedEndpoint);
    const auto monitor = resolvePcPlaybackEndpoint (monitorOutput);
    return source.isNotEmpty() && monitor.isNotEmpty()
        && PcAudioLoopbackCapture::endpointNamesMatch (source, monitor);
}

bool looksLikeSc220Endpoint (const juce::String& endpointName)
]=])
sc220_self_monitor_replace_once(_engine "PC versus monitor endpoint identity"
    "${_helper_anchor}" "${_helper_replacement}")

set(_monitor_names_replacement [=[
juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()
{
    auto names = monitorOutput.getOutputDeviceNames();
    const auto pcSource = pcLoopback.requestedEndpointName();
    for (int index = names.size(); --index >= 0;)
        if (pcEndpointConflictsWithMonitor (pcSource, names[index]))
            names.remove (index);
    return names;
}
]=])
sc220_self_monitor_replace_range(_engine "withhold the captured endpoint from monitor choices"
    "juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()"
    "juce::Array<double> LiveAudioEngine::getAvailableSampleRates() const"
    "${_monitor_names_replacement}")

set(_monitor_select_replacement [=[
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

    if (pcEndpointConflictsWithMonitor (
            pcLoopback.requestedEndpointName(), requested))
    {
        const juce::String message =
            "PC Audio Source and Monitor Output resolve to the same physical endpoint. "
            "Windows will play the original signal directly and SC220 Live will return a delayed DSP copy, causing echo and recursive feedback. "
            "Route the player to a different VB-CABLE/A/B source, then monitor to the headphones.";
        monitorOutput.setMuted (true);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    const auto error = monitorOutput.selectOutputDevice (
        requested, currentSampleRate.load());
    if (error.isEmpty())
    {
        pcAudioLoopBlocked.store (false);
        const juce::ScopedLock lock (statusLock);
        if (pcLoopback.lastError().isEmpty())
            pcAudioRoutingError.clear();
    }
    return error;
}
]=])
sc220_self_monitor_replace_range(_engine "reject recursive physical self-monitoring"
    "juce::String LiveAudioEngine::selectMonitorOutputDevice"
    "juce::String LiveAudioEngine::applyDeviceSelection"
    "${_monitor_select_replacement}")

set(_pc_names_replacement [=[
juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;

    juce::StringArray names;
    if (! pcEndpointConflictsWithStream (
            PcAudioLoopbackCapture::systemDefaultLabel, streamOutput)
        && ! pcEndpointConflictsWithMonitor (
            PcAudioLoopbackCapture::systemDefaultLabel, monitorDevice))
        names.add (PcAudioLoopbackCapture::systemDefaultLabel);

    for (const auto& endpoint : PcAudioLoopbackCapture::availableEndpoints())
    {
        // A different physical endpoint or different virtual-cable family is a
        // valid source. The stream endpoint and active monitor endpoint are not.
        if (pcEndpointConflictsWithStream (endpoint.name, streamOutput)
            || pcEndpointConflictsWithMonitor (endpoint.name, monitorDevice))
            continue;
        names.addIfNotAlreadyThere (endpoint.name);
    }
    return names;
}
]=])
sc220_self_monitor_replace_range(_engine "withhold stream and monitor endpoints from PC source choices"
    "juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "${_pc_names_replacement}")

set(_pc_select_replacement [=[
juce::String LiveAudioEngine::selectPcAudioEndpoint (const juce::String& name)
{
    const auto requested = name.trim().isNotEmpty()
        ? name.trim()
        : juce::String (PcAudioLoopbackCapture::systemDefaultLabel);

    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;

    if (pcEndpointConflictsWithStream (requested, streamOutput))
    {
        const juce::String message =
            "PC Audio cannot capture the same cable used by Stream Output. "
            "Choose another physical endpoint or a different VB-CABLE/A/B endpoint.";
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    if (pcEndpointConflictsWithMonitor (requested, monitorDevice))
    {
        const juce::String message =
            "PC Audio Source and Monitor Output cannot use the same physical endpoint. "
            "That route creates direct-plus-DSP doubling and captures the monitor back into itself. "
            "Choose a different VB-CABLE/A/B source or turn monitoring off first.";
        monitorOutput.setMuted (true);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    const auto previous = pcLoopback.requestedEndpointName();
    const double rate = currentSampleRate.load();
    pcLoopback.stop();
    monitorOutput.flush();
    auto error = pcLoopback.selectEndpoint (requested);
    if (error.isNotEmpty())
    {
        pcLoopback.selectEndpoint (previous);
        if (rate > 0.0)
        {
            pcLoopback.prepare (rate);
            pcLoopback.start();
        }
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = error;
        return error;
    }

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
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError = error;
        }
    }
    loudnessResetRequested.store (true, std::memory_order_release);
    return error;
}
]=])
sc220_self_monitor_replace_range(_engine "transactional PC endpoint restart and monitor flush"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "juce::String LiveAudioEngine::getPcAudioEndpointName() const"
    "${_pc_select_replacement}")

set(_startup_replacement [=[
    pcLoopback.prepare (sampleRate);
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    const auto monitorDevice = monitorOutput.getStatus().deviceName;
    if (pcEndpointConflictsWithStream (
            pcLoopback.requestedEndpointName(), streamOutput)
        || pcEndpointConflictsWithMonitor (
            pcLoopback.requestedEndpointName(), monitorDevice))
    {
        monitorOutput.setMuted (true);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio capture was blocked because its endpoint is also used by Stream Output or Monitor Output.";
    }
    else
    {
        pcAudioLoopBlocked.store (false);
        {
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError.clear();
        }
        if (! pcLoopback.start())
        {
            const juce::ScopedLock lock (statusLock);
            pcAudioRoutingError = pcLoopback.lastError();
        }
    }
]=])
sc220_self_monitor_replace_range(_engine "revalidate source and monitor after device restart"
    "    pcLoopback.prepare (sampleRate);"
    "    const int voiceLatency ="
    "${_startup_replacement}")

set(_monitor_power_anchor [=[
    monitorPowerButton.setClickingTogglesState (true);
    monitorPowerButton.onClick = [this]
    {
        audioEngine.setMonitorMuted (! monitorPowerButton.getToggleState());
        repaint (masterArea);
    };
]=])
set(_monitor_power_replacement [=[
    monitorPowerButton.setClickingTogglesState (true);
    monitorPowerButton.onClick = [this]
    {
        if (! monitorPowerButton.getToggleState())
        {
            audioEngine.setMonitorMuted (true);
            repaint (masterArea);
            return;
        }

        if (monitorOutputBox.getSelectedItemIndex() < 0)
        {
            monitorPowerButton.setToggleState (false, juce::dontSendNotification);
            audioEngine.setMonitorMuted (true);
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Select Monitor Output",
                "Choose a physical Monitor Output that is different from PC Audio Source.");
            repaint (masterArea);
            return;
        }

        const auto error = audioEngine.selectMonitorOutputDevice (
            monitorOutputBox.getText());
        if (error.isNotEmpty())
        {
            monitorPowerButton.setToggleState (false, juce::dontSendNotification);
            audioEngine.setMonitorMuted (true);
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Unsafe Monitor Routing", error);
        }
        else
        {
            audioEngine.setMonitorMuted (false);
        }
        refreshAllControls();
        repaint (masterArea);
    };
]=])
sc220_self_monitor_replace_once(_ui "validate endpoint identity before monitor enable"
    "${_monitor_power_anchor}" "${_monitor_power_replacement}")

set(_pc_state_anchor [=[
    const auto pcStateText = pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : "PLAYBACK CAPTURE STARTING";
]=])
set(_pc_state_replacement [=[
    const auto pcStateText = pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : lastStatus.pcAudioError.isNotEmpty()
                               ? "CAPTURE ERROR / RETRYING"
                               : "PLAYBACK CAPTURE STARTING";
]=])
sc220_self_monitor_replace_once(_ui "show asynchronous capture failure instead of frozen starting"
    "${_pc_state_anchor}" "${_pc_state_replacement}")

set(_monitor_label_anchor [=[
    const bool monitorHealthy = lastStatus.monitorRunning
                             && lastStatus.monitorError.isEmpty();
    g.setColour (monitorHealthy ? palette::green : palette::muted);
    g.setFont (interFont (8.5f, true));
    g.drawText (monitorHealthy ? "MONITOR ACTIVE" : "SELECT PHYSICAL OUTPUT",
                monitorOutputBox.getX(), musicCueButton.getBottom() + 7,
                monitorOutputBox.getWidth(), 18, juce::Justification::centred);
]=])
set(_monitor_label_replacement [=[
    const bool selfMonitorBlocked = lastStatus.pcAudioLoopBlocked
        && lastStatus.pcAudioError.containsIgnoreCase ("same physical endpoint");
    const bool monitorHealthy = lastStatus.monitorRunning
                             && lastStatus.monitorError.isEmpty()
                             && ! selfMonitorBlocked;
    g.setColour (selfMonitorBlocked ? palette::red
                 : monitorHealthy ? palette::green : palette::muted);
    g.setFont (interFont (selfMonitorBlocked ? 7.5f : 8.5f, true));
    const auto monitorStateText = selfMonitorBlocked
        ? "SOURCE = MONITOR / USE VIRTUAL SOURCE"
        : monitorHealthy ? "MONITOR ACTIVE" : "SELECT PHYSICAL OUTPUT";
    g.drawFittedText (monitorStateText,
                      monitorOutputBox.getX(), musicCueButton.getBottom() + 7,
                      monitorOutputBox.getWidth(), 18,
                      juce::Justification::centred, 1);
]=])
sc220_self_monitor_replace_once(_ui "explain direct-plus-DSP self-monitor block"
    "${_monitor_label_anchor}" "${_monitor_label_replacement}")

foreach(_required_engine_token IN ITEMS
        "pcEndpointConflictsWithMonitor"
        "direct-plus-DSP doubling"
        "monitorOutput.flush();"
        "same physical endpoint"
        "loudnessResetRequested.store")
    string(FIND "${_engine}" "${_required_engine_token}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 self-monitor engine authority missing: ${_required_engine_token}")
    endif()
endforeach()

foreach(_required_ui_token IN ITEMS
        "Unsafe Monitor Routing"
        "CAPTURE ERROR / RETRYING"
        "SOURCE = MONITOR / USE VIRTUAL SOURCE")
    string(FIND "${_ui}" "${_required_ui_token}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 self-monitor UI authority missing: ${_required_ui_token}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live OBS-style PC/monitor self-routing guard generated")
