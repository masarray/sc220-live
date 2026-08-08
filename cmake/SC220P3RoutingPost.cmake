# After the routing core, a disabled capture may still retain its last requested
# endpoint name inside PcAudioLoopbackCapture. Do not let that stale identity hide
# a valid physical Monitor device or report a phantom active source.

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(p3rp_range variable_name label begin_marker end_marker replacement)
    set(_v "${${variable_name}}")
    string(FIND "${_v}" "${begin_marker}" _b)
    string(FIND "${_v}" "${end_marker}" _e)
    if(_b EQUAL -1 OR _e EQUAL -1 OR _e LESS_EQUAL _b)
        message(FATAL_ERROR "SC220 P3 routing-post range changed: ${label}")
    endif()
    string(SUBSTRING "${_v}" 0 ${_b} _prefix)
    string(SUBSTRING "${_v}" ${_e} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

set(_monitor_names [=[
juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()
{
    auto names = monitorOutput.getOutputDeviceNames();
    if (! pcAudioEnabled.load())
        return names;

    const auto pcSource = pcLoopback.selectedEndpointName();
    for (int index = names.size(); --index >= 0;)
        if (pcEndpointConflictsWithMonitor (pcSource, names[index]))
            names.remove (index);
    return names;
}
]=])
p3rp_range(_engine "disabled PC source does not hide Monitor endpoint"
    "juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()"
    "juce::Array<double> LiveAudioEngine::getAvailableSampleRates() const"
    "${_monitor_names}")

# Status remains truthful even though the loopback object retains the last
# requested endpoint internally for potential future re-selection.
string(REPLACE
    "    status.pcAudioDevice = pcLoopback.selectedEndpointName();"
    "    status.pcAudioDevice = pcAudioEnabled.load()\n        ? pcLoopback.selectedEndpointName() : juce::String();"
    _engine "${_engine}")

foreach(_required IN ITEMS
        "if (! pcAudioEnabled.load())"
        "status.pcAudioDevice = pcAudioEnabled.load()")
    string(FIND "${_engine}" "${_required}" _p)
    if(_p EQUAL -1)
        message(FATAL_ERROR "SC220 P3 routing-post authority missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 P3 routing post: stale disabled PC source identity removed")
