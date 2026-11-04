# P14 - ASIO FIRST / BEST-EFFORT WASAPI BROADCAST
# Immutable contract: ASIO callback outputs processed samples directly.
# No WASAPI API call, mutex acquisition, waiting, SRC or device opening there.
# A second independent WASAPI renderer receives ONLY a post-master copy.
foreach(_needed SC220_ENGINE_GENERATED_SOURCE SC220_UI_MONITOR_GENERATED_SOURCE SC220_APP_GENERATED_SOURCE)
  if(NOT DEFINED ${_needed})
    message(FATAL_ERROR "SC220 P14 missing generated source ${_needed}")
  endif()
endforeach()
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)
function(sc220_p14_once variable label anchor replacement)
  set(_s "${${variable}}")
  string(FIND "${_s}" "${anchor}" _p)
  if(_p EQUAL -1)
    message(FATAL_ERROR "SC220 P14 missing anchor: ${label}")
  endif()
  string(REPLACE "${anchor}" "${replacement}" _s "${_s}")
  set(${variable} "${_s}" PARENT_SCOPE)
endfunction()
function(sc220_p14_range variable label before after replacement)
  set(_s "${${variable}}")
  string(FIND "${_s}" "${before}" _a)
  string(FIND "${_s}" "${after}" _b)
  if(_a EQUAL -1 OR _b EQUAL -1 OR _b LESS _a)
    message(FATAL_ERROR "SC220 P14 bad range: ${label}")
  endif()
  string(SUBSTRING "${_s}" 0 ${_a} _left)
  string(SUBSTRING "${_s}" ${_b} -1 _right)
  set(${variable} "${_left}${replacement}${_right}" PARENT_SCOPE)
endfunction()

sc220_p14_once(_engine "independent WASAPI broadcast startup"
[=[    monitorOutput.initialise();]=]
[=[    monitorOutput.initialise(); // Auxiliary physical monitor.
    broadcastOutput.initialise(); // Isolated virtual cable best effort. No ASIO change.]=])

sc220_p14_once(_engine "broadcast thread clean shutdown"
[=[    monitorOutput.shutdown();]=]
[=[    broadcastOutput.shutdown();
    monitorOutput.shutdown();]=])

sc220_p14_once(_engine "broadcast frame clock preparation"
[=[    monitorOutput.prepareSource (sampleRate, blockSize);]=]
[=[    monitorOutput.prepareSource (sampleRate, blockSize);
    broadcastOutput.prepareSource (sampleRate, blockSize);]=])

sc220_p14_once(_engine "reset independent output on ASIO stop"
[=[    monitorOutput.flush();]=]
[=[    monitorOutput.flush();
    broadcastOutput.flush();]=])

# Some earlier generators have reformatted the legacy startup device scan.
# Its single side effect must never call the JUCE/ASIO output selector. If
# present, redirect it onto the separate WASAPI broadcast sink instead.
string(FIND "${_engine}" "selectOutputDevice (name);" _legacy_call)
if(NOT _legacy_call EQUAL -1)
  sc220_p14_once(_engine "startup cannot steal ASIO output"
[=[selectOutputDevice (name);]=]
[=[selectBroadcastOutputDevice (name);]=])
endif()

sc220_p14_once(_engine "add independent broadcast WASAPI selection"
[=[juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()]=]
[=[juce::StringArray LiveAudioEngine::getBroadcastOutputDeviceNames()
{
    return broadcastOutput.getOutputDeviceNames();
}

juce::String LiveAudioEngine::selectBroadcastOutputDevice (const juce::String& name)
{
    const auto requested = name.trim();
    if (requested.isNotEmpty()
        && PcAudioLoopbackCapture::endpointWouldConflict (
            pcLoopback.requestedEndpointName(), requested))
        return "Broadcast cannot output to the endpoint captured as PC Music. "
               "Change PC Music capture to physical playback to avoid feedback.";
    // This method is on the message thread. Endpoint opening takes place on a
    // separate WASAPI worker; this never reconfigures the ASIO device.
    return broadcastOutput.selectOutputDevice (requested, currentSampleRate.load());
}

juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()]=])

sc220_p14_once(_engine "independent broadcast status telemetry"
[=[    status.monitorError = monitorStatus.lastError;]=]
[=[    status.monitorError = monitorStatus.lastError;
    const auto broadcastStatus = broadcastOutput.getStatus();
    status.broadcastRunning = broadcastStatus.running;
    status.broadcastPrimed = broadcastStatus.primed;
    status.broadcastUnderflows = broadcastStatus.underflows;
    status.broadcastOverflows = broadcastStatus.overflows;
    status.broadcastDevice = broadcastStatus.deviceName;
    status.broadcastError = broadcastStatus.lastError;
    status.outputIsVirtualCable = status.broadcastRunning;]=])

sc220_p14_range(_engine "ASIO headphones always independent of GO LIVE"
[=[    if (! outputMuted.load())
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)]=]
[=[    const auto elapsed =]=]
[=[    // Performer ASIO feed is ALWAYS independent of GO LIVE / Windows audio.
    // This is not WASAPI monitoring; it is the native ASIO output path.
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] == nullptr)
            continue;
        const int sourceChannel = juce::jmin (channel, processingChannels - 1);
        juce::FloatVectorOperations::copy (outputChannelData[channel],
                                           voiceBlock.getReadPointer (sourceChannel),
                                           numSamples);
    }

    // ASIO buffer has already been filled. Only now copy for the best-effort
    // audience path; never delay writing native headphone output with WASAPI.
    // A disconnected/full FIFO simply drops this audience block.
    if (! outputMuted.load (std::memory_order_relaxed))
        broadcastOutput.push (voiceBlock, numSamples);

]=])

sc220_p14_once(_ui "broadcast output list sourced from independent renderer"
[=[audioEngine.getOutputDeviceNames()]=]
[=[audioEngine.getBroadcastOutputDeviceNames()]=])

sc220_p14_once(_ui "broadcast selection never alters ASIO"
[=[audioEngine.selectOutputDevice (]=]
[=[audioEngine.selectBroadcastOutputDevice (]=])

sc220_p14_once(_ui "broadcast status reported independently"
[=[status.outputDevice]=]
[=[status.broadcastDevice]=])

sc220_p14_once(_ui "clear broadcast selector semantics"
[=["SELECT STREAM DEVICE"]=]
[=["BROADCAST / VB-CABLE"]=])

sc220_p14_once(_ui "clarify output section ASIO primary"
[=["STREAM", outputBox.getX()]=]
[=["BROADCAST", outputBox.getX()]=])

sc220_p14_once(_ui "mark secondary auxiliary monitor"
[=["MONITOR", monitorOutputBox.getX()]=]
[=["AUX MONITOR", monitorOutputBox.getX()]=])

sc220_p14_once(_app "persist independent broadcast selection"
[=[            file->setValue ("monitorDevice", monitorStatus.monitorDevice);]=]
[=[            file->setValue ("monitorDevice", monitorStatus.monitorDevice);
            file->setValue ("broadcastDevice", monitorStatus.broadcastDevice);]=])

sc220_p14_once(_app "restore separate WASAPI output without touching ASIO"
[=[            if (monitorDevice.isNotEmpty())
                audioEngine->selectMonitorOutputDevice (monitorDevice);]=]
[=[            if (monitorDevice.isNotEmpty())
                audioEngine->selectMonitorOutputDevice (monitorDevice);
            const auto broadcastDevice = file->getValue ("broadcastDevice");
            if (broadcastDevice.isNotEmpty())
                audioEngine->selectBroadcastOutputDevice (broadcastDevice);]=])

# Virtual CABLE playback must also be identified as the stream endpoint
# for the existing PC Music loopback anti-feedback guard. ASIO's physical
# output is not the broadcast sink and must not be used for this comparison.
sc220_p14_once(_engine "PC capture sees actual broadcast output"
[=[juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }]=]
[=[juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    const auto streamOutput = broadcastOutput.getStatus().deviceName;
]=])
# Legacy selectPcAudioEndpoint is rewritten by another downstream generator.
# Its virtual-endpoint block remains active; the new broadcast selector also
# explicitly rejects a PC capture endpoint conflict before selecting VB-CABLE.

# Effective generated-code acceptance. Fail closed on accidental re-routing.
foreach(_must "broadcastOutput.push (voiceBlock, numSamples)"
              "broadcastOutput.prepareSource (sampleRate, blockSize)"
              "broadcastOutput.shutdown()"
              "LiveAudioEngine::selectBroadcastOutputDevice"
              "juce::FloatVectorOperations::copy (outputChannelData[channel]"
              "if (! outputMuted.load (std::memory_order_relaxed))")
  string(FIND "${_engine}" "${_must}" _i)
  if(_i EQUAL -1)
    message(FATAL_ERROR "SC220 P14 missing critical audio isolation: ${_must}")
  endif()
endforeach()
foreach(_forbid "audioEngine.selectOutputDevice (" "audioEngine.getOutputDeviceNames()")
  string(FIND "${_ui}" "${_forbid}" _i)
  if(NOT _i EQUAL -1)
    message(FATAL_ERROR "SC220 P14 GUI still reconfigures ASIO for broadcast: ${_forbid}")
  endif()
endforeach()
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P14: ASIO headphone direct + independent best-effort WASAPI broadcast")
