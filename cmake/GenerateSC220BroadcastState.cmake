# Generate the application source with persisted Monitor Bus and Music Signature
# state. The original application remains the reviewed source authority.

if(NOT DEFINED SC220_APP_SOURCE OR NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 broadcast-state generator requires source and destination")
endif()

file(READ "${SC220_APP_SOURCE}" _app)

function(sc220_state_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 broadcast-state anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_load_anchor [=[
            audioEngine->setBypassed (file->getBoolValue ("enhancerBypassed", false));
            audioEngine->setEngineEnabled (file->getBoolValue ("engineEnabled", true));
]=])
set(_load_replacement [=[
            audioEngine->setBypassed (file->getBoolValue ("enhancerBypassed", false));
            audioEngine->setEngineEnabled (file->getBoolValue ("engineEnabled", true));

            audioEngine->setMusicPresetIndex (file->getIntValue (
                "musicPresetIndex", audioEngine->getMusicPresetIndex()));
            audioEngine->setMusicEnhance (static_cast<float> (file->getDoubleValue (
                "musicEnhance", audioEngine->getMusicEnhance())));
            audioEngine->setMusicSmartBass (static_cast<float> (file->getDoubleValue (
                "musicSmartBass", audioEngine->getMusicSmartBass())));
            audioEngine->setMusicSmartTreble (static_cast<float> (file->getDoubleValue (
                "musicSmartTreble", audioEngine->getMusicSmartTreble())));
            audioEngine->setMusicVocalBody (static_cast<float> (file->getDoubleValue (
                "musicVocalBody", audioEngine->getMusicVocalBody())));
            audioEngine->setMusicStereoMagic (static_cast<float> (file->getDoubleValue (
                "musicStereoMagic", audioEngine->getMusicStereoMagic())));

            audioEngine->setMonitorGainDb (static_cast<float> (file->getDoubleValue (
                "monitorGainDb", 0.0)));
            audioEngine->setMonitorMuted (file->getBoolValue ("monitorMuted", true));
            audioEngine->setMonitorSource (
                file->getIntValue ("monitorSource", 0) == 1
                    ? LiveAudioEngine::MonitorSource::musicCue
                    : LiveAudioEngine::MonitorSource::program);
]=])
sc220_state_replace_once(_app "load monitor and music settings"
    "${_load_anchor}" "${_load_replacement}")

set(_initialise_anchor [=[
        audioEngine->initialise (deviceState.get());

        mainWindow = std::make_unique<LiveMainWindow> (
]=])
set(_initialise_replacement [=[
        audioEngine->initialise (deviceState.get());
        if (auto* file = properties.getUserSettings())
        {
            const auto monitorDevice = file->getValue ("monitorDevice");
            if (monitorDevice.isNotEmpty())
                audioEngine->selectMonitorOutputDevice (monitorDevice);
        }

        mainWindow = std::make_unique<LiveMainWindow> (
]=])
sc220_state_replace_once(_app "restore physical monitor device"
    "${_initialise_anchor}" "${_initialise_replacement}")

set(_save_anchor [=[
            file->setValue ("enhancerBypassed", audioEngine->isBypassed());
            file->setValue ("engineEnabled", audioEngine->isEngineEnabled());
            file->saveIfNeeded();
]=])
set(_save_replacement [=[
            file->setValue ("enhancerBypassed", audioEngine->isBypassed());
            file->setValue ("engineEnabled", audioEngine->isEngineEnabled());

            file->setValue ("musicPresetIndex", audioEngine->getMusicPresetIndex());
            file->setValue ("musicEnhance", audioEngine->getMusicEnhance());
            file->setValue ("musicSmartBass", audioEngine->getMusicSmartBass());
            file->setValue ("musicSmartTreble", audioEngine->getMusicSmartTreble());
            file->setValue ("musicVocalBody", audioEngine->getMusicVocalBody());
            file->setValue ("musicStereoMagic", audioEngine->getMusicStereoMagic());

            const auto monitorStatus = audioEngine->getStatus();
            file->setValue ("monitorDevice", monitorStatus.monitorDevice);
            file->setValue ("monitorGainDb", audioEngine->getMonitorGainDb());
            file->setValue ("monitorMuted", audioEngine->isMonitorMuted());
            file->setValue ("monitorSource",
                audioEngine->getMonitorSource() == LiveAudioEngine::MonitorSource::musicCue
                    ? 1 : 0);
            file->saveIfNeeded();
]=])
sc220_state_replace_once(_app "save monitor and music settings"
    "${_save_anchor}" "${_save_replacement}")

get_filename_component(_generated_directory "${SC220_APP_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 Live broadcast-state source: ${SC220_APP_GENERATED_SOURCE}")

# The monitor engine, UI and persisted application now exist. Add selectable
# PC playback capture and anti-loop policy, then apply final visual polish.
include("${CMAKE_CURRENT_LIST_DIR}/GenerateSC220PcEndpointRouting.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")

# Final stream-selector authority: never display ordinal aliases such as
# "VB-CABLE 2" or "VB-CABLE 3". Preserve the exact currently enumerated
# Windows render-endpoint friendly names and select by that exact identity.
include("${CMAKE_CURRENT_LIST_DIR}/UseSC220RealStreamEndpoints.cmake")

# Voice-clean UI/order authority from Sonkupik. A later finalizer narrows the
# runtime policy to frozen-profile learning plus a learned-floor expander.
include("${CMAKE_CURRENT_LIST_DIR}/IntegrateSC220StaticNoiseClean.cmake")

# Runtime clock authority: publish the actual open-device sample rate and buffer
# size. Visual telemetry must never receive a cosmetic/default 0 Hz value.
include("${CMAKE_CURRENT_LIST_DIR}/FixSC220RuntimeClockStatus.cmake")

# First repair any legacy analyzer routing so the proven visualizer restore can
# consume a known post-master source.
include("${CMAKE_CURRENT_LIST_DIR}/RestoreSC220ProgrammeAnalyzerRoute.cmake")

# Final audio policy: Learn is explicit/frozen, normal programme audio does not
# run the spectral workspace, and PC-source/Monitor self-routing is rejected.
include("${CMAKE_CURRENT_LIST_DIR}/FinalizeSC220NaturalLivePath.cmake")

# Restore the previously proven responsive post-limiter visualizer. This removes
# the queued ScopeTap/32-band FFT lifecycle and uses atomic 14-band telemetry.
include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")

# Hardware-test P3 authority. This pass restores explicit routing, direct
# frozen-profile Noise controls, independent Dual Mono and live LUFS-M.
include("${CMAKE_CURRENT_LIST_DIR}/FinalizeSC220LiveUxP3.cmake")

# P4 is deliberately last: keep the approved P3 console/visualizer intact while
# replacing raw-peak ducking with sustained speech presence and cleaning the
# fixed YT/TikTok target caption from the loudness dial.
include("${CMAKE_CURRENT_LIST_DIR}/SC220P4SmartDucking.cmake")
