# Build a lightweight fixed-frequency spectralizer with a redundant runtime
# sample path. The protected post-master ScopeTap remains primary. If that tap
# is starved at runtime, the UI immediately falls back to the post-Signature
# Music tap and then the Voice tap. All taps are lock-free SPSC queues. The
# 512-point FFT, smoothing and drawing remain on the 30 Hz message thread.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE
   OR NOT DEFINED SC220_ENGINE_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 spectralizer requires final UI and engine sources")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)
file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)

function(sc220_spectrum_replace_range variable_name label start_marker end_marker replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${start_marker}" _start)
    string(FIND "${_value}" "${end_marker}" _end)
    if(_start EQUAL -1 OR _end EQUAL -1 OR _end LESS_EQUAL _start)
        message(FATAL_ERROR "SC220 spectralizer range changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" 0 ${_start} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}\n\n${_suffix}" PARENT_SCOPE)
endfunction()

function(sc220_spectrum_replace_first variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 spectralizer anchor changed: ${label}")
    endif()
    string(LENGTH "${anchor}" _anchor_length)
    math(EXPR _suffix_position "${_position} + ${_anchor_length}")
    string(SUBSTRING "${_value}" 0 ${_position} _prefix)
    string(SUBSTRING "${_value}" ${_suffix_position} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

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

    void updateFromTaps (askp::ScopeTap& programmeTap,
                         askp::ScopeTap& musicTap,
                         askp::ScopeTap& voiceTap,
                         double sampleRate)
    {
        int count = programmeTap.consumeLatest (captureLeft.data(),
                                                captureRight.data(), fftSize);
        source = count > 0 ? CaptureSource::postMaster : CaptureSource::none;

        if (count <= 0)
        {
            count = musicTap.consumeLatest (captureLeft.data(),
                                            captureRight.data(), fftSize);
            if (count > 0)
                source = CaptureSource::musicRecovery;
        }

        if (count <= 0)
        {
            count = voiceTap.consumeLatest (captureLeft.data(),
                                            captureRight.data(), fftSize);
            if (count > 0)
                source = CaptureSource::voiceRecovery;
        }

        if (count <= 0 || sampleRate <= 0.0)
        {
            source = CaptureSource::none;
            ++emptyFrameStreak;
            decayWithoutFrame();
            repaint();
            return;
        }

        emptyFrameStreak = 0;
        capturedFrameCount += static_cast<unsigned long long> (count);
        leftFftData.fill (0.0f);
        rightFftData.fill (0.0f);
        const int copied = juce::jlimit (0, fftSize, count);
        const int destinationStart = fftSize - copied;
        float capturedPeak = 0.0f;
        for (int i = 0; i < copied; ++i)
        {
            const float leftSample = captureLeft[static_cast<size_t> (i)];
            const float rightSample = captureRight[static_cast<size_t> (i)];
            leftFftData[static_cast<size_t> (destinationStart + i)] = leftSample;
            rightFftData[static_cast<size_t> (destinationStart + i)] = rightSample;
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
                peakHoldFrames[band] = 15; // 500 ms at 30 Hz.
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

        juce::String sourceText;
        switch (source)
        {
            case CaptureSource::postMaster:     sourceText = "POST-MASTER"; break;
            case CaptureSource::musicRecovery: sourceText = "MUSIC RECOVERY"; break;
            case CaptureSource::voiceRecovery: sourceText = "VOICE RECOVERY"; break;
            case CaptureSource::none:          sourceText = "NO FRAMES"; break;
        }
        g.setColour (source == CaptureSource::none
                         ? palette::red.withAlpha (0.78f)
                         : palette::green.withAlpha (0.78f));
        g.setFont (interFont (8.3f, true));
        g.drawText (sourceText + "  /  24 BAND  /  30 FPS",
                    juce::roundToInt (area.getRight() - 245.0f),
                    juce::roundToInt (area.getY() + 6.0f), 211, 16,
                    juce::Justification::centredRight);

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
            g.drawText (source == CaptureSource::none
                            ? "WAITING FOR ANALYZER FRAMES"
                            : "PROGRAM SILENCE",
                        graph.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    enum class CaptureSource { none, postMaster, musicRecovery, voiceRecovery };

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
    CaptureSource source = CaptureSource::none;
    unsigned long long capturedFrameCount = 0;
    int emptyFrameStreak = 0;
    bool hasSignal = false;
};
]=])
sc220_spectrum_replace_range(_ui "fixed-frequency spectralizer with redundant taps"
    "${_display_start}" "${_display_end}" "${_display_replacement}")

# Keep all three lock-free taps armed for the lifetime of the visible console.
set(_constructor_anchor [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.setProgrammeAnalyzerEnabled (true);

    configureControls();
]=])
set(_constructor_replacement [=[
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.setProgrammeAnalyzerEnabled (true);

    configureControls();
]=])
sc220_spectrum_replace_first(_ui "arm analyzer taps in constructor"
    "${_constructor_anchor}" "${_constructor_replacement}")

set(_ui_active_anchor [=[
void SC220MainComponent::setUiActive (bool active)
{
    uiActive = active;
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    if (active)
        startTimerHz (30);
    else
        stopTimer();
}
]=])
set(_ui_active_replacement [=[
void SC220MainComponent::setUiActive (bool active)
{
    uiActive = active;
    // Keep publication armed while hidden. ScopeTap drops visual data instead
    // of ever blocking audio, and the reader receives the newest tail on resume.
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.setProgrammeAnalyzerEnabled (true);
    if (active)
        startTimerHz (30);
    else
        stopTimer();
}
]=])
sc220_spectrum_replace_first(_ui "keep analyzer publication armed"
    "${_ui_active_anchor}" "${_ui_active_replacement}")

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
    // Runtime recovery is deliberate: a device reopen or stale UI lifecycle
    // cannot leave all visual taps disabled while the audio meters are moving.
    audioEngine.setProgrammeAnalyzerEnabled (true);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);
    spectrumDisplay->updateFromTaps (audioEngine.programmeAnalyzerTap(),
                                     audioEngine.getMusicEnhancerBridge().analysisTap(),
                                     audioEngine.getVoiceEnhancerBridge().analysisTap(),
                                     lastStatus.sampleRate);
]=])
sc220_spectrum_replace_first(_ui "drive spectralizer with redundant live taps"
    "${_timer_anchor}" "${_timer_replacement}")

# The post-master publisher is also re-armed at the publication point. This is
# one lock-free atomic store per callback and cannot wait, allocate or alter audio.
set(_publish_anchor [=[
    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);
]=])
set(_publish_replacement [=[
    programmeAnalyzer.setEnabled (true);
    programmeAnalyzer.push (voiceBlock.getReadPointer (0),
                            voiceBlock.getReadPointer (1), numSamples);
]=])
sc220_spectrum_replace_first(_engine "force post-master analyzer publication"
    "${_publish_anchor}" "${_publish_replacement}")

foreach(_required IN ITEMS
        "PROGRAM SPECTRUM"
        "bandCount = 24"
        "fftOrder = 9"
        "segmentCount = 15"
        "peakHoldFrames[band] = 15"
        "updateFromTaps"
        "MUSIC RECOVERY"
        "VOICE RECOVERY"
        "WAITING FOR ANALYZER FRAMES"
        "getMusicEnhancerBridge().analysisTap()"
        "getVoiceEnhancerBridge().analysisTap()"
        "performFrequencyOnlyForwardTransform")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 spectralizer authority missing: ${_required}")
    endif()
endforeach()

foreach(_required_engine IN ITEMS
        "programmeAnalyzer.setEnabled (true);"
        "programmeAnalyzer.push")
    string(FIND "${_engine}" "${_required_engine}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 analyzer publisher missing: ${_required_engine}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "PROGRAM IMPACT"
        "PC INPUT IMPACT"
        "OLDER"
        "NOW"
        "historySize = 32"
        "spectrumDisplay->updateFromTap")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete visualizer remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")
message(STATUS "SC220 Live redundant post-master/music/voice spectralizer generated")
