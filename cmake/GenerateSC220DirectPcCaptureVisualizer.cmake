# Generate a direct WASAPI visual sample path and attach it as an independent
# spectralizer authority. This path is intentionally upstream of the DSP FIFO,
# ASK-P bridges and programme callback. It proves whether Windows capture itself
# is producing frames and cannot be starved by downstream lifecycle changes.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 direct capture visualizer requires final UI source")
endif()

get_filename_component(_sc220_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SC220_PC_CAPTURE_SOURCE
    "${_sc220_root}/live/PcAudioLoopbackCapture.cpp")
set(SC220_PC_CAPTURE_GENERATED_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sc220-monitor/PcAudioLoopbackCapture_Visual.cpp")

file(READ "${SC220_PC_CAPTURE_SOURCE}" _capture)
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_direct_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 direct capture anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_include_anchor [=[
#include <algorithm>
#include <cmath>
#include <vector>
]=])
set(_include_replacement [=[
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
]=])
sc220_direct_replace_once(_capture "visual queue includes"
    "${_include_anchor}" "${_include_replacement}")

set(_tap_anchor [=[
float peakToDb (float value) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (value, 1.0e-9f), -120.0f);
}
]=])
set(_tap_replacement [=[
float peakToDb (float value) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (value, 1.0e-9f), -120.0f);
}

class DirectPcCaptureVisualTap final
{
public:
    void reset() noexcept
    {
        producerWrite = 0;
        consumerRead = 0;
        writePublished.store (0, std::memory_order_release);
        readPublished.store (0, std::memory_order_release);
        publishedSamples.store (0, std::memory_order_release);
    }

    void push (const float* left, const float* right, int samples) noexcept
    {
        if (left == nullptr || right == nullptr || samples <= 0)
            return;

        int write = producerWrite;
        const int read = readPublished.load (std::memory_order_acquire);
        const int writable = (read - write - 1) & queueMask;
        const int count = juce::jmin (samples, writable);
        if (count <= 0)
            return; // visual data drops; the capture thread never waits

        // Keep the newest tail when one WASAPI packet is larger than the space
        // currently available to the visual queue.
        const int sourceOffset = samples - count;
        left += sourceOffset;
        right += sourceOffset;

        const int first = juce::jmin (count, queueSize - write);
        std::memcpy (leftQueue.data() + write, left,
                     static_cast<size_t> (first) * sizeof (float));
        std::memcpy (rightQueue.data() + write, right,
                     static_cast<size_t> (first) * sizeof (float));
        const int second = count - first;
        if (second > 0)
        {
            std::memcpy (leftQueue.data(), left + first,
                         static_cast<size_t> (second) * sizeof (float));
            std::memcpy (rightQueue.data(), right + first,
                         static_cast<size_t> (second) * sizeof (float));
        }

        write = (write + count) & queueMask;
        producerWrite = write;
        publishedSamples.fetch_add (static_cast<std::uint64_t> (count),
                                    std::memory_order_relaxed);
        writePublished.store (write, std::memory_order_release);
    }

    int consumeLatest (float* leftDestination, float* rightDestination,
                       int maximumSamples) noexcept
    {
        maximumSamples = juce::jlimit (0, 8192, maximumSamples);
        if (leftDestination == nullptr || rightDestination == nullptr
            || maximumSamples <= 0)
            return 0;

        int read = consumerRead;
        const int published = writePublished.load (std::memory_order_acquire);
        int available = (published - read) & queueMask;
        if (available > maximumSamples)
        {
            read = (published - maximumSamples) & queueMask;
            available = maximumSamples;
        }

        const int first = juce::jmin (available, queueSize - read);
        if (first > 0)
        {
            std::memcpy (leftDestination, leftQueue.data() + read,
                         static_cast<size_t> (first) * sizeof (float));
            std::memcpy (rightDestination, rightQueue.data() + read,
                         static_cast<size_t> (first) * sizeof (float));
        }
        const int second = available - first;
        if (second > 0)
        {
            std::memcpy (leftDestination + first, leftQueue.data(),
                         static_cast<size_t> (second) * sizeof (float));
            std::memcpy (rightDestination + first, rightQueue.data(),
                         static_cast<size_t> (second) * sizeof (float));
        }

        consumerRead = published;
        readPublished.store (published, std::memory_order_release);
        return available;
    }

    std::uint64_t totalPublished() const noexcept
    {
        return publishedSamples.load (std::memory_order_acquire);
    }

private:
    static constexpr int queueSize = 16384;
    static constexpr int queueMask = queueSize - 1;
    std::array<float, queueSize> leftQueue {};
    std::array<float, queueSize> rightQueue {};
    int producerWrite = 0; // WASAPI capture thread only
    int consumerRead = 0;  // JUCE message thread only
    std::atomic<int> writePublished { 0 };
    std::atomic<int> readPublished { 0 };
    std::atomic<std::uint64_t> publishedSamples { 0 };
};

DirectPcCaptureVisualTap directPcCaptureVisualTap;
]=])
sc220_direct_replace_once(_capture "direct PC capture SPSC tap"
    "${_tap_anchor}" "${_tap_replacement}")

set(_prepare_anchor [=[
    underflowCount.store (0);
    overflowCount.store (0);
}
]=])
set(_prepare_replacement [=[
    underflowCount.store (0);
    overflowCount.store (0);
    directPcCaptureVisualTap.reset();
}
]=])
sc220_direct_replace_once(_capture "reset direct visual queue"
    "${_prepare_anchor}" "${_prepare_replacement}")

set(_consume_api_anchor [=[
int PcAudioLoopbackCapture::read (juce::AudioBuffer<float>& destination,
                                  int numSamples) noexcept
]=])
set(_consume_api_replacement [=[
int PcAudioLoopbackCapture::consumeLatestVisual (float* leftDestination,
                                                 float* rightDestination,
                                                 int maximumSamples) noexcept
{
    return directPcCaptureVisualTap.consumeLatest (
        leftDestination, rightDestination, maximumSamples);
}

std::uint64_t PcAudioLoopbackCapture::capturedVisualSamples() noexcept
{
    return directPcCaptureVisualTap.totalPublished();
}

int PcAudioLoopbackCapture::read (juce::AudioBuffer<float>& destination,
                                  int numSamples) noexcept
]=])
sc220_direct_replace_once(_capture "public direct capture consumer"
    "${_consume_api_anchor}" "${_consume_api_replacement}")

set(_push_anchor [=[
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    const int writable = juce::jmin (numSamples, fifo.getFreeSpace());
]=])
set(_push_replacement [=[
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    // Publish before touching the DSP FIFO. Therefore a full/blocked DSP FIFO
    // cannot suppress the visual proof that WASAPI delivered this packet.
    directPcCaptureVisualTap.push (left, right, numSamples);

    const int writable = juce::jmin (numSamples, fifo.getFreeSpace());
]=])
sc220_direct_replace_once(_capture "publish every WASAPI packet upstream of DSP FIFO"
    "${_push_anchor}" "${_push_replacement}")

# Add direct PC capture between Music DSP recovery and Voice recovery. It is a
# real frequency source, not an animation derived from peak meter telemetry.
set(_source_block_anchor [=[
        if (count <= 0)
        {
            count = voiceTap.consumeLatest (captureLeft.data(),
                                            captureRight.data(), fftSize);
            if (count > 0)
                source = CaptureSource::voiceRecovery;
        }
]=])
set(_source_block_replacement [=[
        if (count <= 0)
        {
            count = PcAudioLoopbackCapture::consumeLatestVisual (
                captureLeft.data(), captureRight.data(), fftSize);
            if (count > 0)
                source = CaptureSource::pcCapture;
        }

        if (count <= 0)
        {
            count = voiceTap.consumeLatest (captureLeft.data(),
                                            captureRight.data(), fftSize);
            if (count > 0)
                source = CaptureSource::voiceRecovery;
        }
]=])
sc220_direct_replace_once(_ui "direct PC capture fallback"
    "${_source_block_anchor}" "${_source_block_replacement}")

set(_switch_anchor [=[
            case CaptureSource::postMaster:     sourceText = "POST-MASTER"; break;
            case CaptureSource::musicRecovery: sourceText = "MUSIC RECOVERY"; break;
            case CaptureSource::voiceRecovery: sourceText = "VOICE RECOVERY"; break;
            case CaptureSource::none:          sourceText = "NO FRAMES"; break;
]=])
set(_switch_replacement [=[
            case CaptureSource::postMaster:     sourceText = "POST-MASTER"; break;
            case CaptureSource::musicRecovery: sourceText = "MUSIC RECOVERY"; break;
            case CaptureSource::pcCapture:     sourceText = "PC CAPTURE"; break;
            case CaptureSource::voiceRecovery: sourceText = "VOICE RECOVERY"; break;
            case CaptureSource::none:          sourceText = "NO FRAMES"; break;
]=])
sc220_direct_replace_once(_ui "visible direct capture authority"
    "${_switch_anchor}" "${_switch_replacement}")

set(_enum_anchor [=[
    enum class CaptureSource { none, postMaster, musicRecovery, voiceRecovery };
]=])
set(_enum_replacement [=[
    enum class CaptureSource
    {
        none,
        postMaster,
        musicRecovery,
        pcCapture,
        voiceRecovery
    };
]=])
sc220_direct_replace_once(_ui "direct capture source enum"
    "${_enum_anchor}" "${_enum_replacement}")

set(_pc_state_anchor [=[
    const auto pcStateText = pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : "PLAYBACK CAPTURE STARTING";
    g.drawFittedText (pcStateText,
                      pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63,
                      142, 22, juce::Justification::centredLeft, 1);
]=])
set(_pc_state_replacement [=[
    const auto pcStateText = pcReady ? "PLAYBACK CAPTURE ACTIVE"
                           : lastStatus.pcAudioLoopBlocked ? "ROUTING BLOCKED"
                           : lastStatus.pcAudioError.isNotEmpty() ? "CAPTURE ERROR"
                           : "PLAYBACK CAPTURE STARTING";
    g.drawFittedText (pcStateText,
                      pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63,
                      142, 22, juce::Justification::centredLeft, 1);
    if (lastStatus.pcAudioError.isNotEmpty())
    {
        g.setColour (palette::red.withAlpha (0.82f));
        g.setFont (interFont (7.6f));
        g.drawFittedText (lastStatus.pcAudioError,
                          pcStripArea.getX() + 16, pcStripArea.getY() + 84,
                          pcStripArea.getWidth() - 32, 18,
                          juce::Justification::centred, 1);
    }
]=])
sc220_direct_replace_once(_ui "show actual WASAPI capture failure"
    "${_pc_state_anchor}" "${_pc_state_replacement}")

foreach(_required IN ITEMS
        "directPcCaptureVisualTap.push"
        "consumeLatestVisual"
        "capturedVisualSamples"
        "PC CAPTURE"
        "CaptureSource::pcCapture"
        "CAPTURE ERROR")
    string(FIND "${_capture}${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 direct capture authority missing: ${_required}")
    endif()
endforeach()

get_filename_component(_generated_directory
    "${SC220_PC_CAPTURE_GENERATED_SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${_generated_directory}")
file(WRITE "${SC220_PC_CAPTURE_GENERATED_SOURCE}" "${_capture}")
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties("${SC220_PC_CAPTURE_SOURCE}"
        PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(SC220Live PRIVATE "${SC220_PC_CAPTURE_GENERATED_SOURCE}")
endif()

message(STATUS "SC220 Live direct WASAPI capture spectralizer authority generated")
