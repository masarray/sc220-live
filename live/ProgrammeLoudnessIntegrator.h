#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace askp::live {

// Allocation-free ITU-R BS.1770 / EBU R128 loudness measurement.
// K-weighting is performed by LiveAudioEngine before frames reach this class.
// Four adjacent 100 ms segments form each 400 ms momentary block with 75%
// overlap. Integrated loudness retains the absolute and relative R128 gates.
class ProgrammeLoudnessIntegrator final
{
public:
    static constexpr float absoluteGateLufs = -70.0f;
    static constexpr float relativeGateLu = -10.0f;
    static constexpr double warmupSeconds = 3.0;

    void prepare (double nextSampleRate) noexcept
    {
        sampleRate = std::max (1.0, nextSampleRate);
        hopSamples = std::max (1, static_cast<int> (std::lround (sampleRate * 0.100)));
        reset();
    }

    void reset() noexcept
    {
        segmentEnergy.fill (0.0);
        segmentLength.fill (0);
        histogramEnergy.fill (0.0);
        histogramCount.fill (0);
        currentSegmentEnergy = 0.0;
        currentSegmentLength = 0;
        segmentWrite = 0;
        segmentsReady = 0;
        measuredFrames = 0;
        acceptedBlocks = 0;
        integrated = -120.0f;
        momentary = -120.0f;
    }

    // weightedChannelEnergy is the sum of squared K-weighted channel samples
    // for one frame. Stereo channels therefore retain their BS.1770 weights.
    void processFrame (double weightedChannelEnergy) noexcept
    {
        currentSegmentEnergy += std::max (0.0, weightedChannelEnergy);
        ++currentSegmentLength;
        ++measuredFrames;

        if (currentSegmentLength >= hopSamples)
            finishSegment();
    }

    float integratedLufs() const noexcept { return integrated; }
    float momentaryLufs() const noexcept { return momentary; }
    std::uint64_t blockCount() const noexcept { return acceptedBlocks; }
    double elapsedSeconds() const noexcept
    {
        return static_cast<double> (measuredFrames) / sampleRate;
    }
    bool momentaryReady() const noexcept
    {
        return segmentsReady >= static_cast<int> (segmentEnergy.size());
    }
    bool ready() const noexcept
    {
        return acceptedBlocks > 0 && elapsedSeconds() >= warmupSeconds;
    }

    static double energyFromLufs (double lufs) noexcept
    {
        return std::pow (10.0, (lufs + 0.691) / 10.0);
    }

    static float lufsFromEnergy (double energy) noexcept
    {
        return energy > 1.0e-15
            ? static_cast<float> (-0.691 + 10.0 * std::log10 (energy))
            : -120.0f;
    }

private:
    static constexpr float histogramMinimumLufs = -70.0f;
    static constexpr float histogramMaximumLufs = 10.0f;
    static constexpr float histogramStepLu = 0.1f;
    static constexpr int histogramBins = static_cast<int> (
        (histogramMaximumLufs - histogramMinimumLufs) / histogramStepLu);

    void finishSegment() noexcept
    {
        segmentEnergy[static_cast<std::size_t> (segmentWrite)] = currentSegmentEnergy;
        segmentLength[static_cast<std::size_t> (segmentWrite)] = currentSegmentLength;
        segmentWrite = (segmentWrite + 1) % static_cast<int> (segmentEnergy.size());
        segmentsReady = std::min (segmentsReady + 1, static_cast<int> (segmentEnergy.size()));
        currentSegmentEnergy = 0.0;
        currentSegmentLength = 0;

        if (segmentsReady < static_cast<int> (segmentEnergy.size()))
            return;

        double blockEnergySum = 0.0;
        int blockFrames = 0;
        for (std::size_t i = 0; i < segmentEnergy.size(); ++i)
        {
            blockEnergySum += segmentEnergy[i];
            blockFrames += segmentLength[i];
        }

        const double blockMeanEnergy = blockEnergySum / std::max (1, blockFrames);
        momentary = lufsFromEnergy (blockMeanEnergy);
        if (momentary < absoluteGateLufs)
            return;

        const int bin = std::clamp (
            static_cast<int> ((momentary - histogramMinimumLufs) / histogramStepLu),
            0, histogramBins - 1);
        histogramEnergy[static_cast<std::size_t> (bin)] += blockMeanEnergy;
        ++histogramCount[static_cast<std::size_t> (bin)];
        ++acceptedBlocks;
        recalculateIntegrated();
    }

    void recalculateIntegrated() noexcept
    {
        double absoluteEnergy = 0.0;
        std::uint64_t absoluteCount = 0;
        for (std::size_t i = 0; i < histogramEnergy.size(); ++i)
        {
            absoluteEnergy += histogramEnergy[i];
            absoluteCount += histogramCount[i];
        }
        if (absoluteCount == 0)
            return;

        const float absoluteLufs = lufsFromEnergy (
            absoluteEnergy / static_cast<double> (absoluteCount));
        const float relativeThreshold = std::max (
            absoluteGateLufs, absoluteLufs + relativeGateLu);

        double gatedEnergy = 0.0;
        std::uint64_t gatedCount = 0;
        for (int bin = 0; bin < histogramBins; ++bin)
        {
            const float binCentre = histogramMinimumLufs
                                  + (static_cast<float> (bin) + 0.5f) * histogramStepLu;
            if (binCentre + histogramStepLu * 0.5f < relativeThreshold)
                continue;
            gatedEnergy += histogramEnergy[static_cast<std::size_t> (bin)];
            gatedCount += histogramCount[static_cast<std::size_t> (bin)];
        }

        integrated = gatedCount > 0
            ? lufsFromEnergy (gatedEnergy / static_cast<double> (gatedCount))
            : -120.0f;
    }

    double sampleRate = 48000.0;
    int hopSamples = 4800;
    std::array<double, 4> segmentEnergy {};
    std::array<int, 4> segmentLength {};
    std::array<double, histogramBins> histogramEnergy {};
    std::array<std::uint64_t, histogramBins> histogramCount {};
    double currentSegmentEnergy = 0.0;
    int currentSegmentLength = 0;
    int segmentWrite = 0;
    int segmentsReady = 0;
    std::uint64_t measuredFrames = 0;
    std::uint64_t acceptedBlocks = 0;
    float integrated = -120.0f;
    float momentary = -120.0f;
};

} // namespace askp::live
