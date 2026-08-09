# Final transport correction applied after the endpoint and UI generators.
# It permits a virtual cable as PC Music source when it is a different cable
# from Stream Output, and keeps the programme graph/analyzer alive without an
# SC220 input device.

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE
   OR NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 PC Music stabilizer requires final engine and UI sources")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_pc_music_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 PC Music stabilizer anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_helpers_anchor [=[
namespace askp::live {
namespace {
float dbFromPeak (float peak) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
}
} // namespace
]=])
set(_helpers_replacement [=[
namespace askp::live {
namespace {
float dbFromPeak (float peak) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-9f), -120.0f);
}

juce::String compactEndpointName (juce::String value)
{
    return value.trim().toUpperCase().retainCharacters (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}

juce::String virtualCableIdentity (const juce::String& endpointName)
{
    const auto name = compactEndpointName (endpointName);
    if (name.contains ("CABLEAINPUT") || name.contains ("CABLEAOUTPUT")) return "VBCABLEA";
    if (name.contains ("CABLEBINPUT") || name.contains ("CABLEBOUTPUT")) return "VBCABLEB";
    if (name.contains ("CABLECINPUT") || name.contains ("CABLECOUTPUT")) return "VBCABLEC";
    if (name.contains ("CABLEDINPUT") || name.contains ("CABLEDOUTPUT")) return "VBCABLED";
    if (name.contains ("CABLEINPUT") || name.contains ("CABLEOUTPUT")
        || name.contains ("VBAUDIOVIRTUALCABLE")) return "VBCABLE";
    if (name.contains ("VOICEMEETERAUX")) return "VOICEMEETERAUX";
    if (name.contains ("VOICEMEETERVAIO3")) return "VOICEMEETERVAIO3";
    if (name.contains ("VOICEMEETER")) return "VOICEMEETER";
    return {};
}

juce::String resolvePcPlaybackEndpoint (const juce::String& requestedEndpoint)
{
    auto requested = requestedEndpoint.trim();
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

bool pcEndpointConflictsWithStream (const juce::String& requestedEndpoint,
                                    const juce::String& streamOutput)
{
    if (streamOutput.trim().isEmpty())
        return false;

    const auto source = resolvePcPlaybackEndpoint (requestedEndpoint);
    if (source.isEmpty())
        return false;
    if (PcAudioLoopbackCapture::endpointNamesMatch (source, streamOutput))
        return true;

    const auto sourceCable = virtualCableIdentity (source);
    const auto streamCable = virtualCableIdentity (streamOutput);
    return sourceCable.isNotEmpty() && sourceCable == streamCable;
}

bool looksLikeSc220Endpoint (const juce::String& endpointName)
{
    const auto name = endpointName.trim().toUpperCase();
    return name == "MIXER" || name.startsWith ("MIXER (")
        || name.contains ("SC220") || name.contains ("RECORDING TECH");
}
} // namespace
]=])
sc220_pc_music_replace_once(_engine "endpoint identities and SC220 detection"
    "${_helpers_anchor}" "${_helpers_replacement}")

# The output client is the programme clock. It must be able to run with zero
# hardware inputs so PC Music enhancement, monitoring and spectrum do not depend
# on SC220 being connected.
set(_initialise_anchor "    auto error = deviceManager.initialise (2, 2, savedDeviceState, true);")
set(_initialise_replacement "    auto error = deviceManager.initialise (0, 2, savedDeviceState, true);")
sc220_pc_music_replace_once(_engine "output-first programme graph"
    "${_initialise_anchor}" "${_initialise_replacement}")

set(_channel_policy_anchor [=[
    requested.useDefaultInputChannels = true;
    requested.useDefaultOutputChannels = true;
]=])
set(_channel_policy_replacement [=[
    if (inputName != nullptr)
        requested.useDefaultInputChannels = true;
    else if (requested.inputDeviceName.isEmpty())
        requested.useDefaultInputChannels = false;

    if (outputName != nullptr)
        requested.useDefaultOutputChannels = true;
]=])
sc220_pc_music_replace_once(_engine "do not invent an input for output-only mode"
    "${_channel_policy_anchor}" "${_channel_policy_replacement}")

set(_autoselect_anchor [=[
    if (savedDeviceState == nullptr || ! outputCable.load())
    {
        const auto outputs = getOutputDeviceNames();
        for (const auto& name : outputs)
        {
            if (looksLikeVirtualCable (name) && name.containsIgnoreCase ("input"))
            {
                selectOutputDevice (name);
                break;
            }
        }
    }

    return error.isEmpty();
]=])
set(_autoselect_replacement [=[
    if (savedDeviceState == nullptr || ! outputCable.load())
    {
        const auto outputs = getOutputDeviceNames();
        for (const auto& name : outputs)
        {
            if (looksLikeVirtualCable (name) && name.containsIgnoreCase ("input"))
            {
                selectOutputDevice (name);
                break;
            }
        }
    }

    const auto activeSetup = deviceManager.getAudioDeviceSetup();
    if (! looksLikeSc220Endpoint (activeSetup.inputDeviceName))
    {
        for (const auto& name : getInputDeviceNames())
            if (looksLikeSc220Endpoint (name))
            {
                selectInputDevice (name);
                break;
            }
    }

    return deviceManager.getCurrentAudioDevice() != nullptr;
]=])
sc220_pc_music_replace_once(_engine "optional SC220 auto-selection"
    "${_autoselect_anchor}" "${_autoselect_replacement}")

# Replace the broad virtual-device ban with exact endpoint/cable identity checks.
string(REPLACE "PcAudioLoopbackCapture::endpointWouldConflict ("
               "pcEndpointConflictsWithStream ("
               _engine "${_engine}")

set(_endpoint_filter_anchor [=[
        // Release policy: PC Audio follows a real playback endpoint. Obvious
        // virtual-cable endpoints are withheld to prevent stream self-capture.
        if (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (endpoint.name)
            || pcEndpointConflictsWithStream (endpoint.name, streamOutput))
            continue;
]=])
set(_endpoint_filter_replacement [=[
        // Physical and virtual playback endpoints are both valid sources. Only
        // the exact endpoint/cable family used by Stream Output is withheld.
        if (pcEndpointConflictsWithStream (endpoint.name, streamOutput))
            continue;
]=])
sc220_pc_music_replace_once(_engine "allow safe virtual PC Music sources"
    "${_endpoint_filter_anchor}" "${_endpoint_filter_replacement}")

set(_selection_guard_anchor [=[
    if (pcEndpointConflictsWithStream (requested, streamOutput)
        || (PcAudioLoopbackCapture::isUnsafeVirtualEndpoint (requested)
            && streamOutput.isNotEmpty()))
    {
        const juce::String message =
            "PC Audio cannot capture the active Stream/VB-CABLE endpoint. "
            "Choose the speakers, headphones, HDMI, or audio-interface playback endpoint.";
]=])
set(_selection_guard_replacement [=[
    if (pcEndpointConflictsWithStream (requested, streamOutput))
    {
        const juce::String message =
            "PC Audio cannot capture the same cable used by Stream Output. "
            "Choose another physical endpoint or a different VB-CABLE/A/B endpoint.";
]=])
sc220_pc_music_replace_once(_engine "same-cable-only selection guard"
    "${_selection_guard_anchor}" "${_selection_guard_replacement}")

set(_output_message_anchor [=[
            "The Stream output cannot be the same endpoint captured by PC Audio. "
            "Keep Stream on VB-CABLE and capture a physical playback endpoint.";
]=])
set(_output_message_replacement [=[
            "The Stream output cannot use the same cable captured by PC Audio. "
            "Choose a different VB-CABLE/A/B endpoint for source and stream.";
]=])
sc220_pc_music_replace_once(_engine "clear same-cable stream warning"
    "${_output_message_anchor}" "${_output_message_replacement}")

# ScopeTap is tiny compared with the DSP graph. Keep it permanently enabled;
# only the 15 fps UI timer stops while hidden. This removes lifecycle races.
set(_destructor_anchor "    audioEngine.setProgrammeAnalyzerEnabled (false);\n")
string(REPLACE "${_destructor_anchor}" "" _ui "${_ui}")

set(_active_anchor "    audioEngine.setProgrammeAnalyzerEnabled (active);\n")
string(REPLACE "${_active_anchor}" "" _ui "${_ui}")

set(_timer_anchor "        audioEngine.setProgrammeAnalyzerEnabled (uiActive);\n")
string(REPLACE "${_timer_anchor}" "" _ui "${_ui}")

set(_prepare_anchor [=[
    musicEnhancerBridge.setGainMatchEnabled (false);

    juce::dsp::ProcessSpec masterSpec;
]=])
set(_prepare_replacement [=[
    musicEnhancerBridge.setGainMatchEnabled (false);
    programmeAnalyzer.setEnabled (true);

    juce::dsp::ProcessSpec masterSpec;
]=])
sc220_pc_music_replace_once(_engine "re-arm analyzer on audio-device start"
    "${_prepare_anchor}" "${_prepare_replacement}")

foreach(_required_engine_token IN ITEMS
        "deviceManager.initialise (0, 2"
        "pcEndpointConflictsWithStream"
        "virtualCableIdentity"
        "looksLikeSc220Endpoint"
        "programmeAnalyzer.setEnabled (true)")
    string(FIND "${_engine}" "${_required_engine_token}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 PC Music engine authority missing: ${_required_engine_token}")
    endif()
endforeach()

foreach(_forbidden_engine_token IN ITEMS
        "deviceManager.initialise (2, 2"
        "Obvious\\n        // virtual-cable endpoints are withheld"
        "&& streamOutput.isNotEmpty())")
    string(FIND "${_engine}" "${_forbidden_engine_token}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 PC Music obsolete policy remains: ${_forbidden_engine_token}")
    endif()
endforeach()

foreach(_forbidden_ui_token IN ITEMS
        "audioEngine.setProgrammeAnalyzerEnabled (false);"
        "audioEngine.setProgrammeAnalyzerEnabled (active);"
        "audioEngine.setProgrammeAnalyzerEnabled (uiActive);")
    string(FIND "${_ui}" "${_forbidden_ui_token}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 analyzer lifecycle race remains: ${_forbidden_ui_token}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live PC Music-only and safe virtual-cable mode generated")
