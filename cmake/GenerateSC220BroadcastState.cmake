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
