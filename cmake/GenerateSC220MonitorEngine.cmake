# Generate the monitor-enabled LiveAudioEngine implementation from the reviewed
# source. All replacements are exact and fail configure when an anchor changes.

if(NOT DEFINED SC220_ENGINE_SOURCE OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 monitor engine generator requires source and destination")
endif()

file(READ "${SC220_ENGINE_SOURCE}" _engine)

function(sc220_engine_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 monitor engine anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_initialise_anchor [=[
    refreshDeviceStatus();

    if (savedDeviceState == nullptr)
]=])
set(_initialise_replacement [=[
    refreshDeviceStatus();
    monitorOutput.initialise();

    if (savedDeviceState == nullptr)
]=])
sc220_engine_replace_once(_engine "monitor initialisation"
    "${_initialise_anchor}" "${_initialise_replacement}")

set(_shutdown_anchor [=[
void LiveAudioEngine::shutdown()
{
    pcLoopback.stop();
]=])
set(_shutdown_replacement [=[
void LiveAudioEngine::shutdown()
{
    pcLoopback.stop();
    monitorOutput.shutdown();
]=])
sc220_engine_replace_once(_engine "monitor shutdown"
    "${_shutdown_anchor}" "${_shutdown_replacement}")

set(_output_names_anchor [=[
juce::StringArray LiveAudioEngine::getOutputDeviceNames()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (false);
    }
    return {};
}
]=])
set(_output_names_replacement [=[
juce::StringArray LiveAudioEngine::getOutputDeviceNames()
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        type->scanForDevices();
        return type->getDeviceNames (false);
    }
    return {};
}

juce::StringArray LiveAudioEngine::getMonitorOutputDeviceNames()
{
    return monitorOutput.getOutputDeviceNames();
}
]=])
sc220_engine_replace_once(_engine "monitor output enumeration"
    "${_output_names_anchor}" "${_output_names_replacement}")

set(_select_output_anchor [=[
juce::String LiveAudioEngine::selectOutputDevice (const juce::String& name)
{
    return applyDeviceSelection (nullptr, &name);
}
]=])
set(_select_output_replacement [=[
juce::String LiveAudioEngine::selectOutputDevice (const juce::String& name)
{
    return applyDeviceSelection (nullptr, &name);
}

juce::String LiveAudioEngine::selectMonitorOutputDevice (const juce::String& name)
{
    const auto requested = name.trim();
    if (requested.isEmpty())
        return monitorOutput.selectOutputDevice ({}, currentSampleRate.load());

    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    if (requested.equalsIgnoreCase (streamOutput)
        || looksLikeVirtualCable (requested)
        || MonitorOutputClient::isUnsafeVirtualOutput (requested))
        return "Monitor output must be a physical endpoint separate from the VB-CABLE stream output.";

    return monitorOutput.selectOutputDevice (requested, currentSampleRate.load());
}
]=])
sc220_engine_replace_once(_engine "monitor output selection"
    "${_select_output_anchor}" "${_select_output_replacement}")

set(_status_anchor [=[
    status.pcUnderflows = pcLoopback.underflows();
    status.pcOverflows = pcLoopback.overflows();
    status.dropouts = dropoutCount.load();
]=])
set(_status_replacement [=[
    status.pcUnderflows = pcLoopback.underflows();
    status.pcOverflows = pcLoopback.overflows();
    const auto monitorStatus = monitorOutput.getStatus();
    status.monitorRunning = monitorStatus.running;
    status.monitorPrimed = monitorStatus.primed;
    status.monitorBufferedSamples = monitorStatus.bufferedSamples;
    status.monitorUnderflows = monitorStatus.underflows;
    status.monitorOverflows = monitorStatus.overflows;
    status.monitorDevice = monitorStatus.deviceName;
    status.monitorError = monitorStatus.lastError;
    status.dropouts = dropoutCount.load();
]=])
sc220_engine_replace_once(_engine "monitor status telemetry"
    "${_status_anchor}" "${_status_replacement}")

set(_buffer_guard_anchor [=[
    if (processBuffer.getNumSamples() < numSamples
        || detectorBuffer.getNumSamples() < numSamples
        || pcAudioBuffer.getNumSamples() < numSamples)
]=])
set(_buffer_guard_replacement [=[
    if (processBuffer.getNumSamples() < numSamples
        || detectorBuffer.getNumSamples() < numSamples
        || pcAudioBuffer.getNumSamples() < numSamples
        || musicCueBuffer.getNumSamples() < numSamples)
]=])
sc220_engine_replace_once(_engine "music cue buffer guard"
    "${_buffer_guard_anchor}" "${_buffer_guard_replacement}")

set(_buffer_view_anchor [=[
    juce::AudioBuffer<float> musicBlock (pcAudioBuffer.getArrayOfWritePointers(),
                                         processingChannels, numSamples);
]=])
set(_buffer_view_replacement [=[
    juce::AudioBuffer<float> musicBlock (pcAudioBuffer.getArrayOfWritePointers(),
                                         processingChannels, numSamples);
    juce::AudioBuffer<float> musicCueBlock (musicCueBuffer.getArrayOfWritePointers(),
                                            processingChannels, numSamples);
]=])
sc220_engine_replace_once(_engine "music cue buffer view"
    "${_buffer_view_anchor}" "${_buffer_view_replacement}")

set(_music_lane_anchor [=[
    pcLoopback.read (musicBlock, numSamples);
    if (! pcAudioEnabled.load() || pcAudioMuted.load())
    {
        musicBlock.clear();
        duckGainLinear = 1.0f;
        duckGainDb.store (0.0f);
    }
    else
    {
        // Program audio receives its own ASK-P Signature pass before ducking,
        // so its bass/detail/stereo character remains stable while voice talks.
        musicMidi.clear();
        musicEnhancerBridge.process (musicBlock, musicMidi);
        // Preserve the PC Music FX chain through the generated Music Cue path.
        for (auto& insert : musicVst3) insert.process (musicBlock, musicMidi);

        const float keyPeak = juce::jmax (sc220PeakL, sc220PeakR);
        const bool voicePresent = dbFromPeak (keyPeak) > -36.0f;
        const float targetDuck = duckingEnabled.load() && voicePresent
                               ? juce::Decibels::decibelsToGain (duckingDepthDb.load())
                               : 1.0f;
        const double seconds = targetDuck < duckGainLinear ? 0.020 : 0.400;
        const float coefficient = static_cast<float> (std::exp (
            -static_cast<double> (numSamples)
            / (juce::jmax (1.0, currentSampleRate.load()) * seconds)));
        const float nextDuck = targetDuck + coefficient * (duckGainLinear - targetDuck);

        pcGainLinear.setTargetValue (juce::Decibels::decibelsToGain (pcAudioGainDb.load()));
        const float pcGainStart = pcGainLinear.getCurrentValue() * duckGainLinear;
        const float pcGainEnd = pcGainLinear.skip (numSamples) * nextDuck;
        musicBlock.applyGainRamp (0, numSamples, pcGainStart, pcGainEnd);
        duckGainLinear = nextDuck;
        duckGainDb.store (juce::Decibels::gainToDecibels (juce::jmax (nextDuck, 1.0e-6f), -120.0f));
    }
]=])
set(_music_lane_replacement [=[
    pcLoopback.read (musicBlock, numSamples);
    musicCueBlock.clear();
    if (! pcAudioEnabled.load() || pcAudioMuted.load())
    {
        musicBlock.clear();
        duckGainLinear = 1.0f;
        duckGainDb.store (0.0f);
    }
    else
    {
        // Program audio receives its own ASK-P Signature pass. Music Cue takes
        // the enhanced, user-fader level before voice ducking; Program keeps the
        // same tone and fader but applies the voice-priority gain envelope.
        musicMidi.clear();
        musicEnhancerBridge.process (musicBlock, musicMidi);
        // Preserve the PC Music FX chain through the generated Music Cue path.
        for (auto& insert : musicVst3) insert.process (musicBlock, musicMidi);
        for (int channel = 0; channel < processingChannels; ++channel)
            musicCueBlock.copyFrom (channel, 0, musicBlock, channel, 0, numSamples);

        const float keyPeak = juce::jmax (sc220PeakL, sc220PeakR);
        const bool voicePresent = dbFromPeak (keyPeak) > -36.0f;
        const float targetDuck = duckingEnabled.load() && voicePresent
                               ? juce::Decibels::decibelsToGain (duckingDepthDb.load())
                               : 1.0f;
        const double seconds = targetDuck < duckGainLinear ? 0.020 : 0.400;
        const float coefficient = static_cast<float> (std::exp (
            -static_cast<double> (numSamples)
            / (juce::jmax (1.0, currentSampleRate.load()) * seconds)));
        const float nextDuck = targetDuck + coefficient * (duckGainLinear - targetDuck);

        pcGainLinear.setTargetValue (juce::Decibels::decibelsToGain (pcAudioGainDb.load()));
        const float pcFaderStart = pcGainLinear.getCurrentValue();
        const float pcFaderEnd = pcGainLinear.skip (numSamples);
        musicCueBlock.applyGainRamp (0, numSamples, pcFaderStart, pcFaderEnd);
        musicBlock.applyGainRamp (0, numSamples,
                                  pcFaderStart * duckGainLinear,
                                  pcFaderEnd * nextDuck);
        duckGainLinear = nextDuck;
        duckGainDb.store (juce::Decibels::gainToDecibels (
            juce::jmax (nextDuck, 1.0e-6f), -120.0f));
    }
]=])
sc220_engine_replace_once(_engine "enhanced music cue before ducking"
    "${_music_lane_anchor}" "${_music_lane_replacement}")

set(_monitor_push_anchor [=[
    updateMeters (voiceBlock, preLimiterPeakDb);

    if (! outputMuted.load())
]=])
set(_monitor_push_replacement [=[
    updateMeters (voiceBlock, preLimiterPeakDb);

    // Confidence monitoring is independent of GO LIVE. Program hears the exact
    // protected stream mix; Music Cue hears enhanced PC music before ducking.
    if (monitorSource.load() == MonitorSource::musicCue)
        monitorOutput.push (musicCueBlock, numSamples);
    else
        monitorOutput.push (voiceBlock, numSamples);

    if (! outputMuted.load())
]=])
sc220_engine_replace_once(_engine "monitor source publication"
    "${_monitor_push_anchor}" "${_monitor_push_replacement}")

set(_prepare_buffers_anchor [=[
    processBuffer.setSize (channels, blockSize, false, true, false);
    detectorBuffer.setSize (channels, blockSize, false, true, false);
    pcAudioBuffer.setSize (channels, blockSize, false, true, false);
    processBuffer.clear();
    detectorBuffer.clear();
    pcAudioBuffer.clear();
]=])
set(_prepare_buffers_replacement [=[
    processBuffer.setSize (channels, blockSize, false, true, false);
    detectorBuffer.setSize (channels, blockSize, false, true, false);
    pcAudioBuffer.setSize (channels, blockSize, false, true, false);
    musicCueBuffer.setSize (channels, blockSize, false, true, false);
    processBuffer.clear();
    detectorBuffer.clear();
    pcAudioBuffer.clear();
    musicCueBuffer.clear();
    monitorOutput.prepareSource (sampleRate, blockSize);
]=])
sc220_engine_replace_once(_engine "monitor source preparation"
    "${_prepare_buffers_anchor}" "${_prepare_buffers_replacement}")

set(_stopped_anchor [=[
void LiveAudioEngine::audioDeviceStopped()
{
    pcLoopback.stop();
]=])
set(_stopped_replacement [=[
void LiveAudioEngine::audioDeviceStopped()
{
    pcLoopback.stop();
    monitorOutput.flush();
]=])
sc220_engine_replace_once(_engine "monitor flush on stream stop"
    "${_stopped_anchor}" "${_stopped_replacement}")

get_filename_component(_generated_directory "${SC220_ENGINE_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live monitor engine source: ${SC220_ENGINE_GENERATED_SOURCE}")
