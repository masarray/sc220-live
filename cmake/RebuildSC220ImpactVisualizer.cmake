# Replace every queue/tap-based production visual path with one deterministic
# post-limiter snapshot. The same stereo buffer that feeds Stream output and
# Stream metering fills a lock-free double bank of exactly 512 samples. The UI
# reads only complete banks. FFT and drawing remain on the 30 Hz message thread.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 snapshot spectralizer requires final UI and engine sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_snapshot_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 snapshot spectralizer range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

function(sc220_snapshot_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 snapshot spectralizer anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

set(_ui_include_anchor [=[
#include "SC220MainComponent.h"
]=])
set(_ui_include_replacement [=[
#include "SC220MainComponent.h"
#include "ProgrammeSpectrumSnapshot.h"
]=])
sc220_snapshot_replace_once(_ui "UI snapshot include"
    "${_ui_include_anchor}" "${_ui_include_replacement}")

set(_engine_include_anchor [=[
#include "LiveAudioEngine.h"
]=])
set(_engine_include_replacement [=[
#include "LiveAudioEngine.h"
#include "ProgrammeSpectrumSnapshot.h"
]=])
sc220_snapshot_replace_once(_engine "engine snapshot include"
    "${_engine_include_anchor}" "${_engine_include_replacement}")

set(_display_start "class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component")
set(_display_end "class SC220MainComponent::LoudnessDisplay final : public juce::Component")
set(_display_replacement [=[
class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component
{
public:
    LiveSpectrumDisplay()
        : fft (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann, true)
    {
        levels.fill (0.0f);
        heldPeaks.fill (0.0f);
        peakHoldFrames.fill (0);
    }

    void updateFromSnapshot (double sampleRate)
    {
        const int count = programmeSpectrumSnapshot.consumeLatest (
            captureLeft.data(), captureRight.data(), fftSize);

        if (count != fftSize || sampleRate <= 0.0)
        {
            ++emptyFrameStreak;
            decayWithoutFrame();
            repaint();
            return;
        }

        emptyFrameStreak = 0;
        ++receivedFrames;
        leftFftData.fill (0.0f);
        rightFftData.fill (0.0f);
        float capturedPeak = 0.0f;
        for (int i = 0; i < fftSize; ++i)
        {
            const float leftSample = captureLeft[static_cast<size_t> (i)];
            const float rightSample = captureRight[static_cast<size_t> (i)];
            leftFftData[static_cast<size_t> (i)] = leftSample;
            rightFftData[static_cast<size_t> (i)] = rightSample;
            capturedPeak = juce::jmax (capturedPeak,
                juce::jmax (std::abs (leftSample), std::abs (rightSample)));
        }

        window.multiplyWithWindowingTable (leftFftData.data(), fftSize);
        window.multiplyWithWindowingTable (rightFftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (leftFftData.data());
        fft.performFrequencyOnlyForwardTransform (rightFftData.data());

        const float safeRate = static_cast<float> (sampleRate);
        const float upperFrequency = juce::jmin (16000.0f, safeRate * 0.45f);
        constexpr float lowerFrequency = 35.0f;

        for (size_t band = 0; band < bandCount; ++band)
        {
            const float t0 = static_cast<float> (band) / static_cast<float> (bandCount);
            const float t1 = static_cast<float> (band + 1) / static_cast<float> (bandCount);
            const float frequency0 = lowerFrequency
                * std::pow (upperFrequency / lowerFrequency, t0);
            const float frequency1 = lowerFrequency
                * std::pow (upperFrequency / lowerFrequency, t1);
            const int bin0 = juce::jlimit (1, fftSize / 2 - 1,
                juce::roundToInt (frequency0 * static_cast<float> (fftSize) / safeRate));
            const int bin1 = juce::jlimit (bin0 + 1, fftSize / 2,
                juce::roundToInt (frequency1 * static_cast<float> (fftSize) / safeRate));

            float stereoPower = 0.0f;
            int bins = 0;
            for (int bin = bin0; bin < bin1; ++bin)
            {
                const float leftMagnitude = leftFftData[static_cast<size_t> (bin)];
                const float rightMagnitude = rightFftData[static_cast<size_t> (bin)];
                stereoPower += 0.5f * (leftMagnitude * leftMagnitude
                                     + rightMagnitude * rightMagnitude);
                ++bins;
            }

            const float energy = bins > 0
                ? std::sqrt (stereoPower / static_cast<float> (bins))
                : 0.0f;
            float db = juce::Decibels::gainToDecibels (
                energy / static_cast<float> (fftSize), -96.0f);

            // Display-only contour. It never touches the programme audio.
            if (band < 5)
                db += 4.0f;
            else if (band < 9)
                db += 1.5f;
            else if (band > 19)
                db -= 1.5f;

            const float target = normaliseDb (db, -72.0f);
            if (target > levels[band])
                levels[band] += (target - levels[band]) * 0.82f;
            else
                levels[band] = levels[band] * 0.88f + target * 0.12f;

            if (levels[band] >= heldPeaks[band])
            {
                heldPeaks[band] = levels[band];
                peakHoldFrames[band] = 15;
            }
            else if (peakHoldFrames[band] > 0)
            {
                --peakHoldFrames[band];
            }
            else
            {
                heldPeaks[band] = juce::jmax (levels[band],
                                              heldPeaks[band] - 0.026f);
            }
        }

        hasSignal = capturedPeak > juce::Decibels::decibelsToGain (-78.0f)
                 || std::any_of (levels.begin(), levels.end(),
                     [] (float value) { return value > 0.018f; });
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::ink);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colour (0xff3d4040));
        g.drawRoundedRectangle (area, 7.0f, 1.0f);

        auto graph = area.reduced (34.0f, 27.0f);
        graph.removeFromBottom (14.0f);

        g.setColour (juce::Colours::white.withAlpha (0.065f));
        for (int line = 0; line <= 4; ++line)
        {
            const float y = graph.getBottom()
                - graph.getHeight() * static_cast<float> (line) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        constexpr int segmentCount = 15;
        constexpr float barGap = 4.0f;
        constexpr float segmentGap = 2.0f;
        const float barWidth = (graph.getWidth()
            - barGap * static_cast<float> (bandCount - 1))
            / static_cast<float> (bandCount);
        const float segmentHeight = (graph.getHeight()
            - segmentGap * static_cast<float> (segmentCount - 1))
            / static_cast<float> (segmentCount);

        for (size_t band = 0; band < bandCount; ++band)
        {
            const float x = graph.getX()
                + static_cast<float> (band) * (barWidth + barGap);
            const int litSegments = juce::jlimit (0, segmentCount,
                juce::roundToInt (levels[band] * static_cast<float> (segmentCount)));

            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const float y = graph.getBottom()
                    - static_cast<float> (segment + 1) * segmentHeight
                    - static_cast<float> (segment) * segmentGap;
                const auto cell = juce::Rectangle<float> (x, y, barWidth, segmentHeight);
                const float heightNormal = static_cast<float> (segment)
                    / static_cast<float> (segmentCount - 1);
                const auto activeColour = heightNormal > 0.86f ? palette::red
                                        : heightNormal > 0.68f ? palette::amber
                                                               : palette::green;
                g.setColour (segment < litSegments
                    ? activeColour.withAlpha (0.94f)
                    : activeColour.withAlpha (0.10f));
                g.fillRoundedRectangle (cell, juce::jmin (1.8f, barWidth * 0.30f));
            }

            if (heldPeaks[band] > 0.025f)
            {
                const float peakY = graph.getBottom()
                    - heldPeaks[band] * graph.getHeight();
                g.setColour (palette::amber.withAlpha (0.95f));
                g.fillRoundedRectangle (x, peakY - 1.0f, barWidth, 2.0f,
                                        juce::jmin (1.0f, barWidth * 0.25f));
            }
        }

        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.setFont (interFont (9.5f, true));
        g.drawText ("PROGRAM SPECTRUM",
                    juce::roundToInt (area.getX() + 34.0f),
                    juce::roundToInt (area.getY() + 6.0f), 190, 16,
                    juce::Justification::centredLeft);

        const auto published = programmeSpectrumSnapshot.generation();
        const bool receiving = receivedFrames > 0 && emptyFrameStreak < 6;
        g.setColour (receiving ? palette::green.withAlpha (0.82f)
                               : palette::red.withAlpha (0.82f));
        g.setFont (interFont (8.3f, true));
        const auto sourceText = receiving
            ? "POST-LIMITER SNAPSHOT  #" + juce::String (published)
            : "WAITING FOR POST-LIMITER SNAPSHOT";
        g.drawFittedText (sourceText + "  /  24 BAND  /  30 FPS",
                          juce::roundToInt (area.getRight() - 305.0f),
                          juce::roundToInt (area.getY() + 6.0f), 271, 16,
                          juce::Justification::centredRight, 1);

        const auto labelY = juce::roundToInt (graph.getBottom() + 5.0f);
        const auto frequencyX = [&graph] (float frequency)
        {
            constexpr float lower = 35.0f;
            constexpr float upper = 16000.0f;
            const float normal = std::log (frequency / lower) / std::log (upper / lower);
            return graph.getX() + juce::jlimit (0.0f, 1.0f, normal) * graph.getWidth();
        };
        g.setColour (juce::Colours::white.withAlpha (0.48f));
        g.setFont (interFont (8.2f));
        const std::array<std::pair<float, const char*>, 6> labels {{
            { 40.0f, "40" }, { 100.0f, "100" }, { 250.0f, "250" },
            { 1000.0f, "1k" }, { 4000.0f, "4k" }, { 12000.0f, "12k" }
        }};
        for (const auto& label : labels)
        {
            const int x = juce::roundToInt (frequencyX (label.first));
            g.drawText (label.second, x - 20, labelY, 40, 14,
                        juce::Justification::centred);
        }

        if (! hasSignal)
        {
            g.setColour (juce::Colours::white.withAlpha (0.44f));
            g.setFont (interFont (11.0f, true));
            g.drawText (receivedFrames == 0
                            ? "WAITING FOR POST-LIMITER AUDIO"
                            : "PROGRAM SILENCE",
                        graph.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    void decayWithoutFrame()
    {
        for (size_t band = 0; band < bandCount; ++band)
        {
            levels[band] *= 0.90f;
            if (peakHoldFrames[band] > 0)
                --peakHoldFrames[band];
            else
                heldPeaks[band] = juce::jmax (levels[band],
                                              heldPeaks[band] - 0.026f);
        }
        hasSignal = std::any_of (levels.begin(), levels.end(),
            [] (float value) { return value > 0.018f; });
    }

    static constexpr int fftOrder = 9;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr size_t bandCount = 24;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> captureLeft {};
    std::array<float, fftSize> captureRight {};
    std::array<float, fftSize * 2> leftFftData {};
    std::array<float, fftSize * 2> rightFftData {};
    std::array<float, bandCount> levels {};
    std::array<float, bandCount> heldPeaks {};
    std::array<int, bandCount> peakHoldFrames {};
    unsigned long long receivedFrames = 0;
    int emptyFrameStreak = 0;
    bool hasSignal = false;
};
]=])
sc220_snapshot_replace_range(_ui "snapshot-driven fixed-frequency spectralizer"
    "${_display_start}" "${_display_end}" "${_display_replacement}")

set(_timer_anchor [=[
    static int spectrumDivider = 0;
    if (++spectrumDivider >= 2)
    {
        spectrumDivider = 0;
        spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                        lastStatus.sampleRate);
    }
]=])
set(_timer_replacement [=[
    spectrumDisplay->updateFromSnapshot (lastStatus.sampleRate);
]=])
sc220_snapshot_replace_once(_ui "drive spectrum directly from complete snapshots"
    "${_timer_anchor}" "${_timer_replacement}")

set(_publish_anchor [=[
    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);
]=])
set(_publish_replacement [=[
    programmeSpectrumSnapshot.push (voiceBlock.getReadPointer (0),
                                    voiceBlock.getReadPointer (1), numSamples);
]=])
sc220_snapshot_replace_once(_engine "publish the exact post-limiter programme buffer"
    "${_publish_anchor}" "${_publish_replacement}")

set(_reset_anchor [=[
    programmeAnalyzer.setEnabled (true);

    juce::dsp::ProcessSpec masterSpec;
]=])
set(_reset_replacement [=[
    programmeAnalyzer.setEnabled (false);
    programmeSpectrumSnapshot.reset();

    juce::dsp::ProcessSpec masterSpec;
]=])
sc220_snapshot_replace_once(_engine "reset snapshot on audio device start"
    "${_reset_anchor}" "${_reset_replacement}")

foreach(_required IN ITEMS
        "ProgrammeSpectrumSnapshot.h"
        "updateFromSnapshot"
        "programmeSpectrumSnapshot.consumeLatest"
        "programmeSpectrumSnapshot.push"
        "POST-LIMITER SNAPSHOT"
        "bandCount = 24"
        "fftOrder = 9"
        "segmentCount = 15"
        "peakHoldFrames[band] = 15"
        "performFrequencyOnlyForwardTransform")
    string(FIND "${_ui}${_engine}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 snapshot spectralizer authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "updateFromTaps"
        "programmeTap.consumeLatest"
        "musicTap.consumeLatest"
        "voiceTap.consumeLatest"
        "PcAudioLoopbackCapture::consumeLatestVisual"
        "MUSIC RECOVERY"
        "VOICE RECOVERY"
        "PC CAPTURE"
        "NO FRAMES"
        "PROGRAM IMPACT"
        "PC INPUT IMPACT"
        "OLDER"
        "NOW"
        "historySize = 32")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete visualizer path remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live deterministic post-limiter snapshot spectralizer generated")
