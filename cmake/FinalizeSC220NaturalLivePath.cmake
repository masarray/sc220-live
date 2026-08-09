# Final SC220 Live authority for natural frozen-profile cleanup and loop-safe monitoring.
#
# Runtime policy:
#   * FFT noise analysis runs only while the user explicitly presses Learn.
#   * The learned floor is frozen; normal music/voice is not spectrally adapted.
#   * The stereo-linked expander runs only when a learned profile exists.
#   * OFF is a real bypass: neither profiler nor expander touches programme audio.
#   * PC Audio Source may never equal Monitor Output, preventing direct+DSP echo.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 natural-live finalizer requires final engine and UI sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_natural_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 natural-live anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_natural_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 natural-live range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

# Resolve the real Windows playback endpoint behind SYSTEM DEFAULT and compare
# it with the selected physical monitor. This is the same safety rule used by
# the proven release branch: never capture the endpoint we are monitoring to.
set(_helper_anchor [=[
bool looksLikeSc220InputName (const juce::String& name) noexcept
{
    return sc220InputPreference (name) > 0;
}
} // namespace
]=])
set(_helper_replacement [=[
bool looksLikeSc220InputName (const juce::String& name) noexcept
{
    return sc220InputPreference (name) > 0;
}

juce::String resolvePcPlaybackEndpointName (const juce::String& requestedEndpoint)
{
    const auto requested = requestedEndpoint.trim();
    const auto endpoints = PcAudioLoopbackCapture::availableEndpoints();
    if (requested.isEmpty()
        || requested.equalsIgnoreCase (PcAudioLoopbackCapture::systemDefaultLabel))
    {
        for (const auto& endpoint : endpoints)
            if (endpoint.isDefault)
                return endpoint.name;
        return {};
    }

    for (const auto& endpoint : endpoints)
        if (endpoint.name.equalsIgnoreCase (requested))
            return endpoint.name;
    return requested;
}

bool pcEndpointConflictsWithMonitor (const juce::String& requestedEndpoint,
                                     const juce::String& monitorOutput)
{
    const auto source = resolvePcPlaybackEndpointName (requestedEndpoint);
    return source.isNotEmpty() && monitorOutput.trim().isNotEmpty()
        && PcAudioLoopbackCapture::endpointNamesMatch (source, monitorOutput);
}
} // namespace
]=])
sc220_natural_replace_once(_engine "PC/monitor identity helper"
    "${_helper_anchor}" "${_helper_replacement}")

# Learn is the only time the short FFT workspace is allowed to touch the voice
# buffer. Once the profile is captured, the main path is zero-latency dry audio.
set(_noise_process_anchor [=[
    noiseClean.process (voiceBlock);
]=])
set(_noise_process_replacement [=[
    if (noiseClean.shouldCaptureProfile())
        noiseClean.process (voiceBlock);
]=])
sc220_natural_replace_once(_engine "profile capture only, never continuous spectral denoise"
    "${_noise_process_anchor}" "${_noise_process_replacement}")

# A learned floor is mandatory. No profile means no hidden fallback expander.
# When OFF, the expander's own enable flag makes this a true unity bypass.
set(_expander_anchor [=[
    expander.setNoiseFloorDb (noiseClean.hasProfile()
                              ? noiseClean.getEstimatedNoiseFloorDb()
                              : -72.0f);
    expander.process (voiceBlock, detectorBlock);
]=])
set(_expander_replacement [=[
    if (noiseClean.hasProfile() && noiseClean.isEnabled() && expander.isEnabled())
    {
        expander.setNoiseFloorDb (noiseClean.getEstimatedNoiseFloorDb());
        expander.process (voiceBlock, detectorBlock);
    }
]=])
sc220_natural_replace_once(_engine "frozen learned floor drives expander only"
    "${_expander_anchor}" "${_expander_replacement}")

# The menu describes the actual behaviour: profile capture is explicit and the
# finished profile is frozen until Learn is pressed again.
string(REPLACE "NOISE PROFILE: READY" "NOISE PROFILE: FROZEN / READY" _ui "${_ui}")
string(REPLACE
    "Keep the SC220 input silent for 2 seconds. The learned profile will target steady interface/preamp hiss while the stereo-linked expander cleans the remaining noise between phrases."
    "Keep the SC220 input silent for 2 seconds. This capture is the only learning phase. The profile is frozen afterwards; normal music stays untouched while the stereo-linked expander closes naturally around the learned static-noise floor."
    _ui "${_ui}")

# Withhold the PC playback endpoint from Monitor Output choices.
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
sc220_natural_replace_range(_engine "withhold captured PC endpoint from Monitor list"
    "juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()"
    "juce::Array<double> LiveAudioEngine::getAvailableSampleRates() const"
    "${_monitor_names_replacement}")

# Monitor selection is transactional and refuses a physical endpoint currently
# used by PC Audio loopback. That topology creates a direct Windows copy plus a
# delayed DSP copy and is the source of the GO LIVE doubling/echo symptom.
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

    if (pcEndpointConflictsWithMonitor (pcLoopback.requestedEndpointName(), requested))
    {
        const juce::String message =
            "PC Audio Source and Monitor Output resolve to the same physical endpoint. "
            "That route creates direct-plus-DSP doubling/echo. Choose a different PC Audio source, preferably a dedicated virtual cable, or choose another monitor endpoint.";
        monitorOutput.setMuted (true);
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    const auto error = monitorOutput.selectOutputDevice (requested, currentSampleRate.load());
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
sc220_natural_replace_range(_engine "block direct-plus-DSP monitor echo"
    "juce::String LiveAudioEngine::selectMonitorOutputDevice"
    "juce::String LiveAudioEngine::applyDeviceSelection"
    "${_monitor_select_replacement}")

# PC source choices also exclude the active Monitor Output. SYSTEM DEFAULT is
# resolved to its real endpoint before comparison.
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
    if (! PcAudioLoopbackCapture::endpointWouldConflict (
            PcAudioLoopbackCapture::systemDefaultLabel, streamOutput)
        && ! pcEndpointConflictsWithMonitor (
            PcAudioLoopbackCapture::systemDefaultLabel, monitorDevice))
        names.add (PcAudioLoopbackCapture::systemDefaultLabel);

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
sc220_natural_replace_range(_engine "withhold Monitor endpoint from PC source list"
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

    if (PcAudioLoopbackCapture::endpointWouldConflict (requested, streamOutput)
        || (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (requested)
            && streamOutput.isNotEmpty()))
    {
        const juce::String message =
            "PC Audio cannot capture the active Stream/VB-CABLE endpoint. "
            "Choose the speakers, headphones, HDMI, or audio-interface playback endpoint.";
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError = message;
        return message;
    }

    if (pcEndpointConflictsWithMonitor (requested, monitorDevice))
    {
        const juce::String message =
            "PC Audio Source and Monitor Output cannot use the same physical endpoint. "
            "That route produces direct-plus-DSP doubling/echo. Turn monitoring off or select a different playback source.";
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
    return error;
}
]=])
sc220_natural_replace_range(_engine "reject PC source equal to Monitor Output"
    "juce::String LiveAudioEngine::selectPcAudioEndpoint"
    "juce::String LiveAudioEngine::getPcAudioEndpointName() const"
    "${_pc_select_replacement}")

foreach(_required_engine IN ITEMS
        "noiseClean.shouldCaptureProfile()"
        "noiseClean.hasProfile() && noiseClean.isEnabled() && expander.isEnabled()"
        "pcEndpointConflictsWithMonitor"
        "direct-plus-DSP doubling/echo"
        "monitorOutput.flush()")
    string(FIND "${_engine}" "${_required_engine}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 natural-live authority missing: ${_required_engine}")
    endif()
endforeach()

foreach(_forbidden_engine IN ITEMS
        "? noiseClean.getEstimatedNoiseFloorDb()\n                              : -72.0f")
    string(FIND "${_engine}" "${_forbidden_engine}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 fallback/adaptive noise floor remains in programme path")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live natural frozen-profile cleanup and loop-safe monitor route generated")
