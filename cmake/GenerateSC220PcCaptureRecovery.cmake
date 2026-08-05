# Generate the production PC Audio capture source with OBS-style endpoint
# recovery. The reviewed source remains readable; this configure-time stage
# adds compatibility fallback, reconnect and honest meter decay without
# maintaining a second handwritten capture implementation.

if(NOT DEFINED SC220_PC_CAPTURE_SOURCE OR NOT DEFINED SC220_PC_CAPTURE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 PC capture recovery requires source and destination")
endif()

file(READ "${SC220_PC_CAPTURE_SOURCE}" _capture)

function(sc220_capture_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 PC capture recovery anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_capture_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 PC capture recovery range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

set(_stop_anchor [=[
void PcAudioLoopbackCapture::stop()
{
    signalThreadShouldExit();
    stopThread (2500);
    running.store (false);
    captureSampleRate.store (0.0);
}
]=])
set(_stop_replacement [=[
void PcAudioLoopbackCapture::stop()
{
    signalThreadShouldExit();
    stopThread (2500);
    running.store (false);
    captureSampleRate.store (0.0);
    peakL.store (-120.0f);
    peakR.store (-120.0f);
    fifo.reset();
    ringBuffer.clear();
}
]=])
sc220_capture_replace_once(_capture "clear stale transport and meters on stop"
    "${_stop_anchor}" "${_stop_replacement}")

set(_empty_read_anchor [=[
    if (available <= 0)
    {
        ++underflowCount;
        return 0;
    }
]=])
set(_empty_read_replacement [=[
    if (available <= 0)
    {
        ++underflowCount;
        const float releaseDb = static_cast<float> (
            36.0 * static_cast<double> (numSamples)
            / juce::jmax (1.0, targetRate.load()));
        peakL.store (juce::jmax (-120.0f, peakL.load() - releaseDb));
        peakR.store (juce::jmax (-120.0f, peakR.load() - releaseDb));
        return 0;
    }
]=])
sc220_capture_replace_once(_capture "decay PC meters when capture supplies no frame"
    "${_empty_read_anchor}" "${_empty_read_replacement}")

set(_run_replacement [=[
void PcAudioLoopbackCapture::run()
{
#if JUCE_WINDOWS
    ScopedComInitialiser com;
    if (! com.usable())
    {
        setError ("Unable to initialise Windows audio COM services.");
        return;
    }

    while (! threadShouldExit())
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> endpoint;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX* format = nullptr;
        HANDLE eventHandle = nullptr;
        bool eventDriven = true;

        auto closeSession = [&]() noexcept
        {
            if (client != nullptr)
                client->Stop();
            capture.Reset();
            client.Reset();
            if (eventHandle != nullptr)
            {
                CloseHandle (eventHandle);
                eventHandle = nullptr;
            }
            if (format != nullptr)
            {
                CoTaskMemFree (format);
                format = nullptr;
            }
        };

        auto hresult = [] (HRESULT value)
        {
            return juce::String::toHexString (static_cast<int> (value))
                .paddedLeft ('0', 8).toUpperCase();
        };

        auto fail = [this, &hresult] (const juce::String& action, HRESULT result)
        {
            setError (action + " (HRESULT 0x" + hresult (result)
                      + "); retrying automatically.");
        };

        {
            const juce::ScopedLock lock (endpointLock);
            activeEndpointId.clear();
            activeEndpointFriendlyName.clear();
        }

        HRESULT hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr,
                                       CLSCTX_ALL,
                                       IID_PPV_ARGS (enumerator.GetAddressOf()));

        juce::String requestedId;
        {
            const juce::ScopedLock lock (endpointLock);
            requestedId = requestedEndpointId;
        }

        if (SUCCEEDED (hr))
        {
            if (requestedId.isNotEmpty())
                hr = enumerator->GetDevice (requestedId.toWideCharPointer(),
                                            endpoint.GetAddressOf());
            else
                hr = enumerator->GetDefaultAudioEndpoint (
                    eRender, eConsole, endpoint.GetAddressOf());
        }

        if (SUCCEEDED (hr) && endpoint != nullptr)
        {
            const auto endpointId = getDeviceId (*endpoint.Get());
            auto endpointName = getDeviceFriendlyName (*endpoint.Get());
            if (endpointName.isEmpty())
                endpointName = endpointId;
            const juce::ScopedLock lock (endpointLock);
            activeEndpointId = endpointId;
            activeEndpointFriendlyName = endpointName;
        }

        if (FAILED (hr) || endpoint == nullptr)
        {
            fail ("Unable to resolve the selected Windows playback endpoint", hr);
            running.store (false);
            captureSampleRate.store (0.0);
            peakL.store (-120.0f);
            peakR.store (-120.0f);
            if (! threadShouldExit())
                wait (500);
            continue;
        }

        auto initialiseCapture = [&] (bool useEventCallback) -> HRESULT
        {
            closeSession();

            HRESULT result = endpoint->Activate (
                __uuidof (IAudioClient), CLSCTX_ALL, nullptr,
                reinterpret_cast<void**> (client.GetAddressOf()));
            if (FAILED (result))
                return result;

            result = client->GetMixFormat (&format);
            if (FAILED (result) || format == nullptr)
                return FAILED (result) ? result : E_FAIL;

            DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK
                        | AUDCLNT_STREAMFLAGS_NOPERSIST;
            if (useEventCallback)
                flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

            // Some physical USB/headset endpoints reject event-driven loopback
            // after a runtime device switch. Try the low-latency event path
            // first, then recreate the client in polling mode as compatibility
            // fallback instead of remaining in STARTING forever.
            const REFERENCE_TIME duration = useEventCallback ? 0 : 1000000;
            result = client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags,
                                         duration, 0, format, nullptr);
            if (FAILED (result))
                return result;

            if (useEventCallback)
            {
                eventHandle = CreateEventW (nullptr, FALSE, FALSE, nullptr);
                if (eventHandle == nullptr)
                    return HRESULT_FROM_WIN32 (GetLastError());
                result = client->SetEventHandle (eventHandle);
                if (FAILED (result))
                    return result;
            }

            result = client->GetService (
                IID_PPV_ARGS (capture.GetAddressOf()));
            if (FAILED (result))
                return result;

            return client->Start();
        };

        const HRESULT eventResult = initialiseCapture (true);
        hr = eventResult;
        if (FAILED (hr) && ! threadShouldExit())
        {
            eventDriven = false;
            hr = initialiseCapture (false);
        }

        if (FAILED (hr) || format == nullptr || capture == nullptr)
        {
            if (FAILED (eventResult) && hr != eventResult)
            {
                setError ("Unable to start WASAPI loopback capture. Event mode: 0x"
                          + hresult (eventResult) + "; polling fallback: 0x"
                          + hresult (hr) + "; retrying automatically.");
            }
            else
            {
                fail ("Unable to start WASAPI loopback capture", hr);
            }
            closeSession();
            running.store (false);
            captureSampleRate.store (0.0);
            peakL.store (-120.0f);
            peakR.store (-120.0f);
            if (! threadShouldExit())
                wait (500);
            continue;
        }

        const int channels = juce::jmax (1, static_cast<int> (format->nChannels));
        const int frameBytes = juce::jmax (1, static_cast<int> (format->nBlockAlign));
        const double sourceRate = static_cast<double> (format->nSamplesPerSec);
        const double destinationRate = targetRate.load();
        captureSampleRate.store (sourceRate);
        running.store (true);
        peakL.store (-120.0f);
        peakR.store (-120.0f);
        setError ({});

        juce::LagrangeInterpolator leftResampler;
        juce::LagrangeInterpolator rightResampler;
        std::vector<float> inputLeft;
        std::vector<float> inputRight;
        std::vector<float> outputLeft;
        std::vector<float> outputRight;

        bool sessionFailed = false;
        juce::String failedOperation;
        HRESULT sessionResult = S_OK;

        while (! threadShouldExit() && ! sessionFailed)
        {
            if (eventDriven)
            {
                const DWORD waitResult = WaitForSingleObject (eventHandle, 100);
                if (waitResult == WAIT_FAILED)
                {
                    failedOperation = "WASAPI loopback wait";
                    sessionResult = HRESULT_FROM_WIN32 (GetLastError());
                    sessionFailed = true;
                    break;
                }
            }
            else
            {
                wait (5);
            }

            while (! threadShouldExit())
            {
                UINT32 packetFrames = 0;
                sessionResult = capture->GetNextPacketSize (&packetFrames);
                if (FAILED (sessionResult))
                {
                    failedOperation = "WASAPI GetNextPacketSize";
                    sessionFailed = true;
                    break;
                }
                if (packetFrames == 0)
                    break;

                BYTE* data = nullptr;
                DWORD packetFlags = 0;
                UINT32 frames = 0;
                sessionResult = capture->GetBuffer (
                    &data, &frames, &packetFlags, nullptr, nullptr);
                if (FAILED (sessionResult))
                {
                    failedOperation = "WASAPI IAudioCaptureClient::GetBuffer";
                    sessionFailed = true;
                    break;
                }

                inputLeft.resize (frames);
                inputRight.resize (frames);
                const bool silent =
                    (packetFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0
                    || data == nullptr;
                for (UINT32 frame = 0; frame < frames; ++frame)
                {
                    if (silent)
                    {
                        inputLeft[frame] = 0.0f;
                        inputRight[frame] = 0.0f;
                        continue;
                    }

                    const BYTE* frameData = data
                        + static_cast<size_t> (frame) * frameBytes;
                    inputLeft[frame] = readPcmSample (frameData, 0, *format);
                    inputRight[frame] = readPcmSample (
                        frameData, channels > 1 ? 1 : 0, *format);
                }

                if (std::abs (sourceRate - destinationRate) < 0.5)
                {
                    push (inputLeft.data(), inputRight.data(),
                          static_cast<int> (frames));
                }
                else
                {
                    const double speedRatio = sourceRate / destinationRate;
                    const int outputFrames = juce::jmax (1,
                        static_cast<int> (std::floor (
                            static_cast<double> (frames) / speedRatio)));
                    outputLeft.resize (static_cast<size_t> (outputFrames));
                    outputRight.resize (static_cast<size_t> (outputFrames));
                    leftResampler.process (speedRatio, inputLeft.data(),
                                           outputLeft.data(), outputFrames);
                    rightResampler.process (speedRatio, inputRight.data(),
                                            outputRight.data(), outputFrames);
                    push (outputLeft.data(), outputRight.data(), outputFrames);
                }

                sessionResult = capture->ReleaseBuffer (frames);
                if (FAILED (sessionResult))
                {
                    failedOperation = "WASAPI IAudioCaptureClient::ReleaseBuffer";
                    sessionFailed = true;
                    break;
                }
            }
        }

        closeSession();
        running.store (false);
        captureSampleRate.store (0.0);
        peakL.store (-120.0f);
        peakR.store (-120.0f);

        if (! threadShouldExit())
        {
            if (sessionFailed)
                fail (failedOperation + " failed; endpoint will be recreated",
                      sessionResult);
            else
                setError ("PC Audio capture stopped unexpectedly; retrying automatically.");
            wait (350);
        }
    }
#endif
}
]=])
sc220_capture_replace_range(_capture "reconnectable event/polling WASAPI capture"
    "void PcAudioLoopbackCapture::run()"
    "} // namespace askp::live"
    "${_run_replacement}")

foreach(_required IN ITEMS
        "polling fallback"
        "retrying automatically"
        "WASAPI GetNextPacketSize"
        "peakL.store (-120.0f)"
        "36.0 * static_cast<double> (numSamples)")
    string(FIND "${_capture}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 PC capture recovery authority missing: ${_required}")
    endif()
endforeach()

get_filename_component(_generated_directory "${SC220_PC_CAPTURE_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_PC_CAPTURE_GENERATED_SOURCE}" "${_capture}")
message(STATUS "SC220 Live resilient PC capture source: ${SC220_PC_CAPTURE_GENERATED_SOURCE}")

if(TARGET SC220Live)
    set_source_files_properties("${SC220_PC_CAPTURE_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE "${SC220_PC_CAPTURE_GENERATED_SOURCE}")
endif()
