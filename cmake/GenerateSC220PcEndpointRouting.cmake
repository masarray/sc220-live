# Add selectable Windows PC playback endpoint capture and explicit anti-loop
# policy to the already generated engine, UI and persisted application sources.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 PC endpoint generator requires engine, UI and app sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_pc_endpoint_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 PC endpoint anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_endpoint_api_anchor [=[
juce::Array<int> LiveAudioEngine::getAvailableBufferSizes() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();
    return { 64, 128, 256, 512 };
}
]=])
set(_endpoint_api_replacement [=[
juce::Array<int> LiveAudioEngine::getAvailableBufferSizes() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getAvailableBufferSizes();
    return { 64, 128, 256, 512 };
}

juce::StringArray LiveAudioEngine::getPcAudioEndpointNames()
{
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }

    juce::StringArray names;
    if (! PcAudioLoopbackCapture::endpointWouldConflict (
            PcAudioLoopbackCapture::systemDefaultLabel, streamOutput))
        names.add (PcAudioLoopbackCapture::systemDefaultLabel);

    for (const auto& endpoint : PcAudioLoopbackCapture::availableEndpoints())
    {
        // Release policy: PC Audio follows a real playback endpoint. Obvious
        // virtual-cable endpoints are withheld to prevent stream self-capture.
        if (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (endpoint.name)
            || PcAudioLoopbackCapture::endpointWouldConflict (endpoint.name, streamOutput))
            continue;
        names.addIfNotAlreadyThere (endpoint.name);
    }
    return names;
}

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

    const auto previous = pcLoopback.requestedEndpointName();
    const double rate = currentSampleRate.load();
    pcLoopback.stop();
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

juce::String LiveAudioEngine::getPcAudioEndpointName() const
{
    return pcLoopback.selectedEndpointName();
}
]=])
sc220_pc_endpoint_replace_once(_engine "PC endpoint engine API"
    "${_endpoint_api_anchor}" "${_endpoint_api_replacement}")

set(_output_guard_anchor [=[
    if (inputName == nullptr && outputName == nullptr)
        return {};
]=])
set(_output_guard_replacement [=[
    if (inputName == nullptr && outputName == nullptr)
        return {};

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
sc220_pc_endpoint_replace_once(_engine "Stream versus PC endpoint guard"
    "${_output_guard_anchor}" "${_output_guard_replacement}")

set(_status_endpoint_anchor [=[
    status.musicDspLatencySamples = musicDspLatencySamples.load();

    const juce::ScopedLock lock (statusLock);
    status.inputDevice = currentInputName;
    status.outputDevice = currentOutputName;
    status.lastError = lastError;
]=])
set(_status_endpoint_replacement [=[
    status.musicDspLatencySamples = musicDspLatencySamples.load();
    status.pcAudioLoopBlocked = pcAudioLoopBlocked.load();
    status.pcAudioDevice = pcLoopback.selectedEndpointName();
    status.pcAudioEndpointId = pcLoopback.selectedEndpointId();

    const juce::ScopedLock lock (statusLock);
    status.inputDevice = currentInputName;
    status.outputDevice = currentOutputName;
    status.pcAudioError = pcAudioRoutingError.isNotEmpty()
        ? pcAudioRoutingError : pcLoopback.lastError();
    status.lastError = lastError;
]=])
sc220_pc_endpoint_replace_once(_engine "PC endpoint status telemetry"
    "${_status_endpoint_anchor}" "${_status_endpoint_replacement}")

set(_loopback_start_anchor [=[
    pcLoopback.prepare (sampleRate);
    pcLoopback.start();
]=])
set(_loopback_start_replacement [=[
    pcLoopback.prepare (sampleRate);
    juce::String streamOutput;
    {
        const juce::ScopedLock lock (statusLock);
        streamOutput = currentOutputName;
    }
    if (PcAudioLoopbackCapture::endpointWouldConflict (
            pcLoopback.requestedEndpointName(), streamOutput))
    {
        pcAudioLoopBlocked.store (true);
        const juce::ScopedLock lock (statusLock);
        pcAudioRoutingError =
            "PC Audio capture was blocked because it resolves to the active Stream/VB-CABLE endpoint.";
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
sc220_pc_endpoint_replace_once(_engine "safe PC loopback start"
    "${_loopback_start_anchor}" "${_loopback_start_replacement}")

set(_pc_source_setup_anchor [=[
    pcSourceBox.addItem ("SYSTEM MIX", 1);
    pcSourceBox.setSelectedId (1, juce::dontSendNotification);
]=])
set(_pc_source_setup_replacement [=[
    pcSourceBox.addItemList (audioEngine.getPcAudioEndpointNames(), 1);
    pcSourceBox.setTextWhenNothingSelected ("SELECT PC AUDIO");
    pcSourceBox.onChange = [this]
    {
        if (pcSourceBox.getSelectedItemIndex() < 0)
            return;
        const auto error = audioEngine.selectPcAudioEndpoint (pcSourceBox.getText());
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Unsafe PC Audio routing", error);
        refreshAllControls();
    };
]=])
sc220_pc_endpoint_replace_once(_ui "selectable PC Audio source"
    "${_pc_source_setup_anchor}" "${_pc_source_setup_replacement}")

set(_pc_source_refresh_anchor [=[
    const auto status = audioEngine.getStatus();
    const auto outputIndex = audioEngine.getOutputDeviceNames().indexOf (status.outputDevice);
]=])
set(_pc_source_refresh_replacement [=[
    const auto status = audioEngine.getStatus();

    const auto pcEndpoints = audioEngine.getPcAudioEndpointNames();
    bool endpointListChanged = pcEndpoints.size() != pcSourceBox.getNumItems();
    if (! endpointListChanged)
        for (int i = 0; i < pcSourceBox.getNumItems(); ++i)
            if (pcSourceBox.getItemText (i) != pcEndpoints[i])
            {
                endpointListChanged = true;
                break;
            }
    if (endpointListChanged)
    {
        pcSourceBox.clear (juce::dontSendNotification);
        pcSourceBox.addItemList (pcEndpoints, 1);
    }
    int pcEndpointIndex = -1;
    for (int i = 0; i < pcSourceBox.getNumItems(); ++i)
        if (pcSourceBox.getItemText (i).equalsIgnoreCase (
                audioEngine.getPcAudioEndpointName()))
            pcEndpointIndex = i;
    if (pcEndpointIndex >= 0)
        pcSourceBox.setSelectedItemIndex (pcEndpointIndex, juce::dontSendNotification);
    else
        pcSourceBox.setSelectedId (0, juce::dontSendNotification);

    const auto outputIndex = audioEngine.getOutputDeviceNames().indexOf (status.outputDevice);
]=])
sc220_pc_endpoint_replace_once(_ui "refresh PC endpoint list and selection"
    "${_pc_source_refresh_anchor}" "${_pc_source_refresh_replacement}")

set(_pc_status_anchor [=[
    g.setColour (lastStatus.pcLoopbackRunning ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 73),
                   static_cast<float> (pcStripArea.getY() + 70), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.8f, true));
    g.drawText (lastStatus.pcLoopbackRunning ? "LOOPBACK ACTIVE" : "LOOPBACK STARTING",
                pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63, 142, 22,
                juce::Justification::centredLeft);
]=])
set(_pc_status_replacement [=[
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
    g.drawFittedText (pcStateText,
                      pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63,
                      142, 22, juce::Justification::centredLeft, 1);
]=])
sc220_pc_endpoint_replace_once(_ui "honest PC capture routing status"
    "${_pc_status_anchor}" "${_pc_status_replacement}")

set(_persist_load_anchor [=[
            audioEngine->setMonitorSource (
                file->getIntValue ("monitorSource", 0) == 1
                    ? LiveAudioEngine::MonitorSource::musicCue
                    : LiveAudioEngine::MonitorSource::program);
]=])
set(_persist_load_replacement [=[
            audioEngine->setMonitorSource (
                file->getIntValue ("monitorSource", 0) == 1
                    ? LiveAudioEngine::MonitorSource::musicCue
                    : LiveAudioEngine::MonitorSource::program);
            audioEngine->selectPcAudioEndpoint (file->getValue (
                "pcAudioEndpoint", PcAudioLoopbackCapture::systemDefaultLabel));
]=])
sc220_pc_endpoint_replace_once(_app "restore selected PC endpoint"
    "${_persist_load_anchor}" "${_persist_load_replacement}")

set(_persist_save_anchor [=[
            file->setValue ("monitorSource",
                audioEngine->getMonitorSource() == LiveAudioEngine::MonitorSource::musicCue
                    ? 1 : 0);
            file->saveIfNeeded();
]=])
set(_persist_save_replacement [=[
            file->setValue ("monitorSource",
                audioEngine->getMonitorSource() == LiveAudioEngine::MonitorSource::musicCue
                    ? 1 : 0);
            file->setValue ("pcAudioEndpoint", audioEngine->getPcAudioEndpointName());
            file->saveIfNeeded();
]=])
sc220_pc_endpoint_replace_once(_app "persist selected PC endpoint"
    "${_persist_save_anchor}" "${_persist_save_replacement}")

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 Live selectable PC endpoint routing generated")
