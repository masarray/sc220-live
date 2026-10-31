#include "ScopeTap.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

int main()
{
    constexpr int fftOrder = 10;
    constexpr int fftSize = 1 << fftOrder;
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 1000.0;

    std::array<float, fftSize> sourceLeft {};
    std::array<float, fftSize> sourceRight {};
    for (int sample = 0; sample < fftSize; ++sample)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * frequency
                         * static_cast<double> (sample) / sampleRate;
        sourceLeft[static_cast<std::size_t> (sample)] = static_cast<float> (0.25 * std::sin (phase));
        sourceRight[static_cast<std::size_t> (sample)] = -sourceLeft[static_cast<std::size_t> (sample)];
    }

    askp::ScopeTap tap;
    tap.setEnabled (true);
    tap.push (sourceLeft.data(), sourceRight.data(), fftSize);

    std::array<float, fftSize> capturedLeft {};
    std::array<float, fftSize> capturedRight {};
    const auto captured = tap.consumeLatest (capturedLeft.data(), capturedRight.data(), fftSize);
    if (captured != fftSize)
    {
        std::cerr << "ScopeTap capture failed: " << captured << '\n';
        return 1;
    }

    std::array<float, fftSize * 2> leftFft {};
    std::array<float, fftSize * 2> rightFft {};
    float collapsedPeak = 0.0f;
    for (int sample = 0; sample < fftSize; ++sample)
    {
        leftFft[static_cast<std::size_t> (sample)] = capturedLeft[static_cast<std::size_t> (sample)];
        rightFft[static_cast<std::size_t> (sample)] = capturedRight[static_cast<std::size_t> (sample)];
        collapsedPeak = std::max (collapsedPeak, std::abs (0.5f
            * (capturedLeft[static_cast<std::size_t> (sample)]
             + capturedRight[static_cast<std::size_t> (sample)])));
    }

    juce::dsp::WindowingFunction<float> window (
        fftSize, juce::dsp::WindowingFunction<float>::hann, true);
    window.multiplyWithWindowingTable (leftFft.data(), fftSize);
    window.multiplyWithWindowingTable (rightFft.data(), fftSize);
    juce::dsp::FFT fft (fftOrder);
    fft.performFrequencyOnlyForwardTransform (leftFft.data());
    fft.performFrequencyOnlyForwardTransform (rightFft.data());

    const auto targetBin = juce::roundToInt (frequency * fftSize / sampleRate);
    const auto leftMagnitude = leftFft[static_cast<std::size_t> (targetBin)];
    const auto rightMagnitude = rightFft[static_cast<std::size_t> (targetBin)];
    const auto stereoMagnitude = std::sqrt (0.5f
        * (leftMagnitude * leftMagnitude + rightMagnitude * rightMagnitude));

    if (collapsedPeak > 1.0e-6f || stereoMagnitude < 10.0f)
    {
        std::cerr << "Anti-phase programme was not preserved: mono=" << collapsedPeak
                  << " stereoFFT=" << stereoMagnitude << '\n';
        return 2;
    }

    std::cout << "SC220 programme analyzer anti-phase audit passed\n";
    return 0;
}
