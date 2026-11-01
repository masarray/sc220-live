# P14 - ASIO FIRST / BEST-EFFORT WASAPI BROADCAST
# Immutable contract: ASIO callback outputs processed samples directly.
# No WASAPI API call, mutex acquisition, waiting, SRC or device opening there.
# A second independent WASAPI renderer receives ONLY a post-master copy.
foreach(_needed SC220_ENGINE_GENERATED_SOURCE SC220_UI_MONITOR_GENERATED_SOURCE SC220_APP_GENERATED_SOURCE)
  if(NOT DEFINED ${D}{_needed})
    message(FATAL_ERROR "SC220 P14 missing generated source ${D}{_needed}")
  endif()
endforeach()
file(READ "${D}{SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${D}{SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${D}{SC220_APP_GENERATED_SOURCE}" _app)
function(sc220_p14_once variable label anchor replacement)
  set(_s "${D}{${D}{variable}}")
  string(FIND "${D}{_s}" "${D}{anchor}" _p)
  if(_p EQUAL -1)
    message(FATAL_ERROR "SC220 P14 missing anchor: ${D}{label}")
  endif()
  string(REPLACE "${D}{anchor}" "${D}{replacement}" _s "${D}{_s}")
  set(${D}{variable} "${D}{_s}" PARENT_SCOPE)
endfunction()
function(sc220_p14_range variable label before after replacement)
  set(_s "${D}{${D}{variable}}")
  string(FIND "${D}{_s}" "${D}{before}" _a)
  string(FIND "${D}{_s}" "${D}{after}" _b)
  if(_a EQUAL -1 OR _b EQUAL -1 OR _b LESS _a)
    message(FATAL_ERROR "SC220 P14 bad range: ${D}{label}")
  endif()
  string(SUBSTRING "${D}{_s}" 0 ${D}{_a} _left)
  string(SUBSTRING "${D}{_s}" ${D}{_b} -1 _right)
  set(${D}{variable} "${D}{_left}${D}{replacement}${D}{_right}" PARENT_SCOPE)
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

sc220_p14_range(_engine "remove incorrect startup auto reroute to VB-CABLE"
[=[    if (savedDeviceState == nullptr)
    {
        const auto outputs = getOutputDeviceNames();]=]
[=[    // P0: SC220 Live is a dedicated hardware companion.]=]
[=[    // ASIO must retain its own input/output device. NEVER automatically switch
    // the primary JUCE output to VB-CABLE at first launch. The separate
    // Broadcast Output (WASAPI) is restored/configured independently.

]=])

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

sc220_p14_once(_engine "nonblocking post-FX broadcast tee"
[=[    updateMeters (voiceBlock, preLimiterPeakDb);]=]
[=[    updateMeters (voiceBlock, preLimiterPeakDb);

    // The only additional realtime work is a bounded planar stereo copy into
    // a preallocated SPSC FIFO. NEVER call the WASAPI APIs here.
    // Overflow / disconnected endpoint -> frame is dropped, not ASIO delayed.
    if (! outputMuted.load (std::memory_order_relaxed))
        broadcastOutput.push (voiceBlock, numSamples);]=])

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

# Effective generated-code acceptance. Fail closed on accidental re-routing.
foreach(_must "broadcastOutput.push (voiceBlock, numSamples)"
              "broadcastOutput.prepareSource (sampleRate, blockSize)"
              "broadcastOutput.shutdown()"
              "LiveAudioEngine::selectBroadcastOutputDevice"
              "juce::FloatVectorOperations::copy (outputChannelData[channel]"
              "if (! outputMuted.load (std::memory_order_relaxed))")
  string(FIND "${D}{_engine}" "${D}{_must}" _i)
  if(_i EQUAL -1)
    message(FATAL_ERROR "SC220 P14 missing critical audio isolation: ${D}{_must}")
  endif()
endforeach()
foreach(_forbid "audioEngine.selectOutputDevice (" "audioEngine.getOutputDeviceNames()")
  string(FIND "${D}{_ui}" "${D}{_forbid}" _i)
  if(NOT _i EQUAL -1)
    message(FATAL_ERROR "SC220 P14 GUI still reconfigures ASIO for broadcast: ${D}{_forbid}")
  endif()
endforeach()
file(WRITE "${D}{SC220_ENGINE_GENERATED_SOURCE}" "${D}{_engine}")
file(WRITE "${D}{SC220_UI_MONITOR_GENERATED_SOURCE}" "${D}{_ui}")
file(WRITE "${D}{SC220_APP_GENERATED_SOURCE}" "${D}{_app}")
message(STATUS "SC220 P14: ASIO headphone direct + independent best-effort WASAPI broadcast")
