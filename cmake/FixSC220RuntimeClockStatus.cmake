# Keep the generated SC220 engine's runtime clock telemetry truthful.
#
# The main UI and the FFT programme analyzer read Status::sampleRate and
# Status::bufferSize. The reviewed engine had atomics for those fields but never
# published the actual JUCE device values, so Status stayed at 0/0. The header
# therefore showed its cosmetic 48 kHz / 128-sample fallback while the Audio
# Setup dialog could be running at e.g. 48 kHz / 480 samples. More importantly,
# LiveSpectrumDisplay::updateFromTap() deliberately rejects sampleRate <= 0,
# which made a healthy post-master audio bus look like "NO PROGRAM AUDIO".

if(NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 runtime clock fix requires generated engine source")
endif()

file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_clock_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 runtime clock anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Publish the real callback clock before any DSP/visual telemetry is prepared.
set(_start_anchor [=[
    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = juce::jmax (64, device->getCurrentBufferSizeSamples());
    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    constexpr int channels = 2;
]=])
set(_start_replacement [=[
    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = juce::jmax (64, device->getCurrentBufferSizeSamples());
    currentSampleRate.store (sampleRate);
    currentBufferSize.store (blockSize);
    constexpr int channels = 2;
]=])
sc220_clock_replace_once(_engine "publish device clock on audio start"
    "${_start_anchor}" "${_start_replacement}")

# Device list / setup changes can happen without the UI reopening. Refresh the
# atomics from the actual open device whenever status is refreshed as well.
set(_refresh_anchor [=[
void LiveAudioEngine::refreshDeviceStatus()
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    {
        const juce::ScopedLock lock (statusLock);
        currentInputName = setup.inputDeviceName;
        currentOutputName = setup.outputDeviceName;
    }
    outputCable.store (looksLikeVirtualCable (setup.outputDeviceName));
    deviceOpen.store (deviceManager.getCurrentAudioDevice() != nullptr);
}
]=])
set(_refresh_replacement [=[
void LiveAudioEngine::refreshDeviceStatus()
{
    const auto setup = deviceManager.getAudioDeviceSetup();
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        currentSampleRate.store (device->getCurrentSampleRate());
        currentBufferSize.store (juce::jmax (1, device->getCurrentBufferSizeSamples()));
    }

    {
        const juce::ScopedLock lock (statusLock);
        currentInputName = setup.inputDeviceName;
        currentOutputName = setup.outputDeviceName;
    }
    outputCable.store (looksLikeVirtualCable (setup.outputDeviceName));
    deviceOpen.store (deviceManager.getCurrentAudioDevice() != nullptr);
}
]=])
sc220_clock_replace_once(_engine "refresh actual open-device clock"
    "${_refresh_anchor}" "${_refresh_replacement}")

foreach(_required IN ITEMS
        "currentSampleRate.store (sampleRate)"
        "currentBufferSize.store (blockSize)"
        "device->getCurrentSampleRate()"
        "device->getCurrentBufferSizeSamples()")
    string(FIND "${_engine}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 runtime clock publication missing: ${_required}")
    endif()
endforeach()

file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live runtime clock telemetry now follows the actual open audio device")
