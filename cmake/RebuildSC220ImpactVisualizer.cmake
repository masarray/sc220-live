# Replace the incorrect left-to-right history meter with a real OBS-style
# fixed-frequency spectralizer. Audio samples are already published by the
# lock-free post-master ScopeTap. The 512-point stereo FFT, ballistics, drawing,
# and peak hold all run on the 30 Hz message thread, never in the audio callback.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 spectralizer requires the final generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

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

function(sc220_spectrum_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 spectralizer anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
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

    void updateFromTap (askp::ScopeTap& programmeTap, double sampleRate)
    {
        std::array<float, fftSize> left {};
        std::array<float, fftSize> right {};
        const int count = programmeTap.consumeLatest (left.data(), right.data(), fftSize);

        if (count <= 0 || sampleRate <= 0.0)
        {
            decayWithoutFrame();
            repaint();
            return;
        }

        leftFftData.fill (0.0f);
        rightFftData.fill (0.0f);
        const int copied = juce::jlimit (0, fftSize, count);
        const int destinationStart = fftSize - copied;
        float capturedPeak = 0.0f;
        for (int i = 0; i < copied; ++i)
        {
            const float leftSample = left[static_cast<size_t> (i)];
            const float rightSample = right[static_cast<size_t> (i)];
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

            // Gentle display contour: kick/bass is readable, body is restrained,
            // and air remains calm. This affects only the drawing, never audio.
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
                heldPeaks[band] = juce::jmax (levels[band], heldPeaks[band] - 0.026f);
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
        g.setColour (juce::Colours::white.withAlpha (0.42f));
        g.setFont (interFont (8.5f));
        g.drawText ("24 BAND  /  30 FPS  /  500 ms HOLD",
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
            g.drawText ("WAITING FOR PROGRAM AUDIO", graph.toNearestInt(),
                        juce::Justification::centred);
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
                heldPeaks[band] = juce::jmax (levels[band], heldPeaks[band] - 0.026f);
        }
        hasSignal = std::any_of (levels.begin(), levels.end(),
            [] (float value) { return value > 0.018f; });
    }

    static constexpr int fftOrder = 9;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr size_t bandCount = 24;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> leftFftData {};
    std::array<float, fftSize * 2> rightFftData {};
    std::array<float, bandCount> levels {};
    std::array<float, bandCount> heldPeaks {};
    std::array<int, bandCount> peakHoldFrames {};
    bool hasSignal = false;
};
]=])
sc220_spectrum_replace_range(_ui "replace history meter with fixed-frequency spectralizer"
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
    spectrumDisplay->updateFromTap (audioEngine.programmeAnalyzerTap(),
                                    lastStatus.sampleRate);
]=])
sc220_spectrum_replace_once(_ui "run lightweight spectralizer at the 30 Hz UI rate"
    "${_timer_anchor}" "${_timer_replacement}")

foreach(_required IN ITEMS
        "PROGRAM SPECTRUM"
        "bandCount = 24"
        "fftOrder = 9"
        "segmentCount = 15"
        "peakHoldFrames[band] = 15"
        "500 ms HOLD"
        "programmeAnalyzerTap()"
        "performFrequencyOnlyForwardTransform")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 spectralizer authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "PROGRAM IMPACT"
        "PC INPUT IMPACT"
        "OLDER"
        "NOW"
        "historySize = 32")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 obsolete history visualizer remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 Live lightweight OBS-style programme spectralizer generated")
