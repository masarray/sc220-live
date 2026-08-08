# Replace ordinal VB-CABLE aliases with the exact active Windows render-endpoint
# friendly names. Stream Output is a render sink (for example CABLE Input); OBS
# opens the paired capture endpoint (for example CABLE Output).

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 real stream endpoint pass requires the generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_real_stream_replace_once label anchor replacement)
    string(FIND "${_ui}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 real stream endpoint anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _updated "${_ui}")
    set(_ui "${_updated}" PARENT_SCOPE)
endfunction()

set(_stream_list_alias [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
        if (name.containsIgnoreCase ("CABLE"))
            streamOutputs.add (name);
    for (int i = 0; i < streamOutputs.size(); ++i)
        outputBox.addItem (i == 0 ? "VB-CABLE"
                                  : "VB-CABLE " + juce::String (i + 1), i + 1);
    outputBox.setTextWhenNothingSelected ("SELECT VB-CABLE");
]=])
set(_stream_list_real [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
    {
        // getOutputDeviceNames() returns Windows render endpoints. Preserve the
        // exact friendly name so users can distinguish standard VB-CABLE,
        // Hi-Fi Cable, multi-channel cable pins, and other installed sinks.
        const bool isVirtualStreamSink =
               name.containsIgnoreCase ("CABLE")
            || name.containsIgnoreCase ("VOICEMEETER INPUT")
            || name.containsIgnoreCase ("VIRTUAL AUDIO");
        if (isVirtualStreamSink)
            streamOutputs.addIfNotAlreadyThere (name);
    }
    outputBox.addItemList (streamOutputs, 1);
    outputBox.setTextWhenNothingSelected ("SELECT STREAM DEVICE");
]=])
sc220_real_stream_replace_once("truthful initial Stream Output list"
    "${_stream_list_alias}" "${_stream_list_real}")

set(_stream_change_alias [=[
    outputBox.onChange = [this]
    {
        const int selectedIndex = outputBox.getSelectedItemIndex();
        if (selectedIndex < 0)
            return;
        juce::StringArray streamOutputs;
        for (const auto& name : audioEngine.getOutputDeviceNames())
            if (name.containsIgnoreCase ("CABLE"))
                streamOutputs.add (name);
        if (! juce::isPositiveAndBelow (selectedIndex, streamOutputs.size()))
            return;
        const auto error = audioEngine.selectOutputDevice (streamOutputs[selectedIndex]);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Output unavailable", error);
    };
]=])
set(_stream_change_real [=[
    outputBox.onChange = [this]
    {
        const auto selectedEndpoint = outputBox.getText().trim();
        if (selectedEndpoint.isEmpty())
            return;

        bool endpointStillAvailable = false;
        for (const auto& name : audioEngine.getOutputDeviceNames())
            if (name.equalsIgnoreCase (selectedEndpoint))
            {
                endpointStillAvailable = true;
                break;
            }

        if (! endpointStillAvailable)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Stream device changed",
                "The selected Windows Stream Output endpoint is no longer available. Refresh the device list and choose an installed virtual audio render endpoint.");
            refreshAllControls();
            return;
        }

        const auto error = audioEngine.selectOutputDevice (selectedEndpoint);
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Output unavailable", error);
        else
            outputBox.setTooltip (selectedEndpoint);
        refreshAllControls();
    };
]=])
sc220_real_stream_replace_once("select exact Windows Stream Output endpoint"
    "${_stream_change_alias}" "${_stream_change_real}")

set(_stream_refresh_alias [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
        if (name.containsIgnoreCase ("CABLE"))
            streamOutputs.add (name);
    const int outputIndex = streamOutputs.indexOf (status.outputDevice);
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
    else
        outputBox.setSelectedId (0, juce::dontSendNotification);
]=])
set(_stream_refresh_real [=[
    juce::StringArray streamOutputs;
    for (const auto& name : audioEngine.getOutputDeviceNames())
    {
        const bool isVirtualStreamSink =
               name.containsIgnoreCase ("CABLE")
            || name.containsIgnoreCase ("VOICEMEETER INPUT")
            || name.containsIgnoreCase ("VIRTUAL AUDIO");
        if (isVirtualStreamSink)
            streamOutputs.addIfNotAlreadyThere (name);
    }

    bool streamListChanged = streamOutputs.size() != outputBox.getNumItems();
    if (! streamListChanged)
        for (int i = 0; i < outputBox.getNumItems(); ++i)
            if (outputBox.getItemText (i) != streamOutputs[i])
            {
                streamListChanged = true;
                break;
            }

    if (streamListChanged)
    {
        outputBox.clear (juce::dontSendNotification);
        outputBox.addItemList (streamOutputs, 1);
    }

    int outputIndex = -1;
    for (int i = 0; i < streamOutputs.size(); ++i)
        if (streamOutputs[i].equalsIgnoreCase (status.outputDevice))
        {
            outputIndex = i;
            break;
        }

    if (outputIndex >= 0)
    {
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
        outputBox.setTooltip (streamOutputs[outputIndex]);
    }
    else
    {
        outputBox.setSelectedId (0, juce::dontSendNotification);
        outputBox.setTooltip ({});
    }
]=])
sc220_real_stream_replace_once("refresh exact Windows Stream Output devices"
    "${_stream_refresh_alias}" "${_stream_refresh_real}")

foreach(_forbidden IN ITEMS
        "i == 0 ? \"VB-CABLE\""
        "\"VB-CABLE \" + juce::String"
        "VB-CABLE 2"
        "VB-CABLE 3")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 generated UI still contains a synthetic Stream Output alias: ${_forbidden}")
    endif()
endforeach()

foreach(_required IN ITEMS
        "outputBox.addItemList (streamOutputs, 1)"
        "SELECT STREAM DEVICE"
        "outputBox.getText().trim()"
        "addIfNotAlreadyThere (name)"
        "streamListChanged"
        "outputBox.setTooltip (selectedEndpoint)")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 real Stream Output contract lost: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live real Windows Stream Output endpoint UI generated")
