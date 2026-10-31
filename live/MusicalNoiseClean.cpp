#include "MusicalNoiseClean.h"

#include <algorithm>
#include <cmath>

namespace askp::live {
namespace {
constexpr float kFloor = 1.0e-12f;

float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

float gainToDb (float gain) noexcept
{
    return juce::Decibels::gainToDecibels (juce::jmax (gain, 1.0e-9f), -120.0f);
}

float smoothStep (float x) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}
} // namespace

void MusicalNoiseClean::FixedFft::configure (int newSize) noexcept
{
    size = newSize == 512 ? 512 : 256;
    stages = size == 512 ? 9 : 8;

    for (int index = 0; index < size; ++index)
    {
        unsigned int value = static_cast<unsigned int> (index);
        unsigned int result = 0;
        for (int bit = 0; bit < stages; ++bit)
        {
            result = (result << 1u) | (value & 1u);
            value >>= 1u;
        }
        reversed[static_cast<size_t> (index)] = static_cast<int> (result);
    }

    for (int index = 0; index < size / 2; ++index)
    {
        const float angle = -juce::MathConstants<float>::twoPi
                          * static_cast<float> (index) / static_cast<float> (size);
        cosine[static_cast<size_t> (index)] = std::cos (angle);
        sine[static_cast<size_t> (index)] = std::sin (angle);
    }
}

void MusicalNoiseClean::FixedFft::forward (float* work) const noexcept
{
    // Expand the real input to interleaved complex storage in-place. Copying
    // backwards prevents the destination from overwriting unread real samples.
    for (int index = size - 1; index >= 0; --index)
    {
        work[index * 2] = work[index];
        work[index * 2 + 1] = 0.0f;
    }

    for (int index = 0; index < size; ++index)
    {
        const int other = reversed[static_cast<size_t> (index)];
        if (other <= index)
            continue;
        std::swap (work[index * 2], work[other * 2]);
        std::swap (work[index * 2 + 1], work[other * 2 + 1]);
    }

    for (int length = 2; length <= size; length <<= 1)
    {
        const int half = length >> 1;
        const int tableStep = size / length;
        for (int block = 0; block < size; block += length)
        {
            int table = 0;
            for (int offset = 0; offset < half; ++offset, table += tableStep)
            {
                const int even = block + offset;
                const int odd = even + half;
                const float wr = cosine[static_cast<size_t> (table)];
                const float wi = sine[static_cast<size_t> (table)];
                const float oddReal = work[odd * 2];
                const float oddImag = work[odd * 2 + 1];
                const float tr = wr * oddReal - wi * oddImag;
                const float ti = wr * oddImag + wi * oddReal;
                const float er = work[even * 2];
                const float ei = work[even * 2 + 1];
                work[even * 2] = er + tr;
                work[even * 2 + 1] = ei + ti;
                work[odd * 2] = er - tr;
                work[odd * 2 + 1] = ei - ti;
            }
        }
    }
}

void MusicalNoiseClean::FixedFft::inverse (float* work) const noexcept
{
    // Spectral processing edits only the positive half. Restore Hermitian
    // symmetry before the inverse complex transform.
    work[1] = 0.0f;
    work[size + 1] = 0.0f;
    for (int bin = 1; bin < size / 2; ++bin)
    {
        work[(size - bin) * 2] = work[bin * 2];
        work[(size - bin) * 2 + 1] = -work[bin * 2 + 1];
    }

    for (int index = 0; index < size; ++index)
        work[index * 2 + 1] = -work[index * 2 + 1];

    for (int index = 0; index < size; ++index)
    {
        const int other = reversed[static_cast<size_t> (index)];
        if (other <= index)
            continue;
        std::swap (work[index * 2], work[other * 2]);
        std::swap (work[index * 2 + 1], work[other * 2 + 1]);
    }

    for (int length = 2; length <= size; length <<= 1)
    {
        const int half = length >> 1;
        const int tableStep = size / length;
        for (int block = 0; block < size; block += length)
        {
            int table = 0;
            for (int offset = 0; offset < half; ++offset, table += tableStep)
            {
                const int even = block + offset;
                const int odd = even + half;
                const float wr = cosine[static_cast<size_t> (table)];
                const float wi = sine[static_cast<size_t> (table)];
                const float oddReal = work[odd * 2];
                const float oddImag = work[odd * 2 + 1];
                const float tr = wr * oddReal - wi * oddImag;
                const float ti = wr * oddImag + wi * oddReal;
                const float er = work[even * 2];
                const float ei = work[even * 2 + 1];
                work[even * 2] = er + tr;
                work[even * 2 + 1] = ei + ti;
                work[odd * 2] = er - tr;
                work[odd * 2 + 1] = ei - ti;
            }
        }
    }

    const float scale = 1.0f / static_cast<float> (size);
    for (int index = 0; index < size; ++index)
        work[index] = work[index * 2] * scale;
}

void MusicalNoiseClean::SubsonicFilter::prepare (double newSampleRate, float cutoffHz) noexcept
{
    const auto safeRate = static_cast<float> (newSampleRate > 0.0 ? newSampleRate : 48000.0);
    const auto safeCutoff = juce::jlimit (5.0f, 45.0f, cutoffHz);
    coefficient = std::exp (-juce::MathConstants<float>::twoPi * safeCutoff / safeRate);
    reset();
}

void MusicalNoiseClean::SubsonicFilter::reset() noexcept
{
    previousInput1 = previousOutput1 = 0.0f;
    previousInput2 = previousOutput2 = 0.0f;
}

float MusicalNoiseClean::SubsonicFilter::process (float input) noexcept
{
    // Two cascaded one-pole high-pass stages. This removes DC/subsonic rumble
    // with a gentle 12 dB/oct slope before the short-FFT hiss reducer.
    const float stage1 = coefficient * (previousOutput1 + input - previousInput1);
    previousInput1 = input;
    previousOutput1 = stage1;

    const float stage2 = coefficient * (previousOutput2 + stage1 - previousInput2);
    previousInput2 = stage1;
    previousOutput2 = stage2;
    return std::isfinite (stage2) ? stage2 : 0.0f;
}

MusicalNoiseClean::MusicalNoiseClean()
{
    fixedFft.configure (256);
    for (auto& value : linkedGainState)
        value = 1.0f;

}

void MusicalNoiseClean::prepare (double newSampleRate, int, int channels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    channelCount = juce::jlimit (1, maxChannels, channels);
    qualityDirty.store (true);
    reconfigureIfNeeded();
    for (auto& filter : subsonicFilters)
        filter.prepare (sampleRate, 24.0f);
    reset();
}

void MusicalNoiseClean::reset()
{
    for (auto& channel : inputRing) channel.fill (0.0f);
    for (auto& channel : outputRing) channel.fill (0.0f);
    for (auto& channel : dryDelayRing) channel.fill (0.0f);
    for (auto& channel : fftWork) channel.fill (0.0f);
    for (auto& channel : currentPower) channel.fill (0.0f);
    for (auto& channel : previousMagnitude) channel.fill (0.0f);
    linkedGainState.fill (1.0f);
    for (auto& filter : subsonicFilters) filter.reset();

    inputWritePos = 0;
    outputReadPos = 0;
    hopCounter = 0;
    samplesSeen = 0;
    learningFramesCaptured = 0;
    learningTimeDomainEnergy = 0.0;
    learningTimeDomainSamples = 0;
}

void MusicalNoiseClean::setQuality (Quality quality)
{
    requestedQuality.store (quality);
    qualityDirty.store (true);
}

void MusicalNoiseClean::setReductionDb (float value) noexcept
{
    reductionDb.store (juce::jlimit (0.0f, 12.0f, value));
}

void MusicalNoiseClean::setPreserve (float value01) noexcept
{
    preserve.store (juce::jlimit (0.0f, 1.0f, value01));
}

void MusicalNoiseClean::setThresholdOffsetDb (float value) noexcept
{
    thresholdOffsetDb.store (juce::jlimit (-6.0f, 12.0f, value));
}

void MusicalNoiseClean::startLearning (double seconds) noexcept
{
    learnSecondsRequested.store (juce::jlimit (0.5, 6.0, seconds));
    learnRequested.store (true);
}

void MusicalNoiseClean::reconfigureIfNeeded() noexcept
{
    if (! qualityDirty.exchange (false))
        return;

    const auto quality = requestedQuality.load();
    fftSize = quality == Quality::clean512 ? 512 : 256;
    hopSize = fftSize / 2;
    fixedFft.configure (fftSize);
    activeFftSize.store (fftSize);

    // A profile is tied to the exact FFT grid and sample rate. Reusing it after
    // a quality/device change would apply the learned energy to different bins.
    profileValid.store (false);
    learningActive.store (false);
    learningProgress.store (0.0f);

    for (int i = 0; i < fftSize; ++i)
    {
        const auto hann = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                  * static_cast<float> (i)
                                                  / static_cast<float> (fftSize));
        window[static_cast<size_t> (i)] = std::sqrt (juce::jmax (0.0f, hann));
    }

    reset();
}

void MusicalNoiseClean::beginLearningOnAudioThread() noexcept
{
    learningFramesCaptured = 0;
    learningFramesTarget = juce::jmax (1, static_cast<int> (std::ceil (
        learnSecondsRequested.load() * sampleRate / static_cast<double> (hopSize))));
    learningTimeDomainEnergy = 0.0;
    learningTimeDomainSamples = 0;

    for (auto& channel : profileAccumulator) channel.fill (0.0);

    learningProgress.store (0.0f);
    learningActive.store (true);
    profileValid.store (false);
}

float MusicalNoiseClean::frequencyWeight (int bin) const noexcept
{
    const auto frequency = static_cast<float> (bin) * static_cast<float> (sampleRate)
                         / static_cast<float> (fftSize);

    if (frequency < 18.0f)   return 0.0f;
    if (frequency < 80.0f)   return 0.82f;
    if (frequency < 300.0f)  return 0.34f;
    if (frequency < 4000.0f) return 0.48f;
    if (frequency < 12000.0f)return 1.00f;
    if (frequency < 17000.0f)return 0.72f;
    return 0.42f;
}

float MusicalNoiseClean::calculateLinkedGain (int bin, float linkedPower,
                                               float localTonality,
                                               float transientProtect) noexcept
{
    if (! profileValid.load())
        return 1.0f;

    const float profile = 0.5f * (profilePower[0][static_cast<size_t> (bin)]
                                + profilePower[1][static_cast<size_t> (bin)]);
    const float threshold = profile * std::pow (10.0f, thresholdOffsetDb.load() / 10.0f);
    const float snr = juce::jmax (0.0f, linkedPower / (threshold + kFloor) - 1.0f);
    const float wiener = snr / (snr + 1.0f);

    const float requestedReduction = reductionDb.load();
    const float minGain = dbToGain (-requestedReduction);
    const float strength = juce::jmap (requestedReduction, 0.0f, 12.0f, 0.45f, 1.55f);
    float target = juce::jmax (minGain, std::pow (juce::jlimit (0.0f, 1.0f, wiener), strength));

    const float weight = frequencyWeight (bin);
    target = 1.0f - weight * (1.0f - target);

    const float protectAmount = preserve.load();
    const float protection = juce::jlimit (0.0f, 0.92f,
                                           localTonality * (0.22f + 0.58f * protectAmount)
                                         + transientProtect * (0.28f + 0.62f * protectAmount));
    target += (1.0f - target) * protection;

    const float engageAlpha = 1.0f - std::exp (-static_cast<float> (hopSize)
                                               / static_cast<float> (sampleRate * 0.135));
    const float releaseAlpha = 1.0f - std::exp (-static_cast<float> (hopSize)
                                                / static_cast<float> (sampleRate * 0.012));
    auto& state = linkedGainState[static_cast<size_t> (bin)];
    const float alpha = target < state ? engageAlpha : releaseAlpha;
    state += alpha * (target - state);
    return juce::jlimit (minGain, 1.0f, state);
}

void MusicalNoiseClean::processFrame() noexcept
{
    const int bins = fftSize / 2 + 1;


    for (int channel = 0; channel < channelCount; ++channel)
    {
        auto& work = fftWork[static_cast<size_t> (channel)];
        std::fill (work.begin(), work.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            int source = inputWritePos + i;
            if (source >= fftSize) source -= fftSize;
            work[static_cast<size_t> (i)] = inputRing[static_cast<size_t> (channel)][static_cast<size_t> (source)]
                                                * window[static_cast<size_t> (i)];
        }

        fixedFft.forward (work.data());

        for (int bin = 0; bin < bins; ++bin)
        {
            const float real = work[static_cast<size_t> (bin * 2)];
            const float imaginary = work[static_cast<size_t> (bin * 2 + 1)];
            const float power = real * real + imaginary * imaginary;
            const float magnitude = std::sqrt (power + kFloor);
            currentPower[static_cast<size_t> (channel)][static_cast<size_t> (bin)] = power;

            const float previous = previousMagnitude[static_cast<size_t> (channel)][static_cast<size_t> (bin)];
            previousMagnitude[static_cast<size_t> (channel)][static_cast<size_t> (bin)] = magnitude;

            if (learningActive.load())
                profileAccumulator[static_cast<size_t> (channel)][static_cast<size_t> (bin)] += power;
        }
    }

    if (learningActive.load())
    {
        ++learningFramesCaptured;
        learningProgress.store (juce::jlimit (0.0f, 1.0f,
            static_cast<float> (learningFramesCaptured) / static_cast<float> (learningFramesTarget)));

        if (learningFramesCaptured >= learningFramesTarget)
        {
            for (int channel = 0; channel < channelCount; ++channel)
                for (int bin = 0; bin < bins; ++bin)
                    profilePower[static_cast<size_t> (channel)][static_cast<size_t> (bin)]
                        = static_cast<float> (profileAccumulator[static_cast<size_t> (channel)][static_cast<size_t> (bin)]
                                              / static_cast<double> (learningFramesCaptured));

            if (channelCount == 1)
                profilePower[1] = profilePower[0];

            const double meanSquare = learningTimeDomainEnergy
                                    / static_cast<double> (juce::jmax<int64_t> (1, learningTimeDomainSamples));
            estimatedNoiseFloorDb.store (gainToDb (static_cast<float> (std::sqrt (meanSquare + 1.0e-16))));
            profileValid.store (true);
            learningActive.store (false);
            learningProgress.store (1.0f);
        }
    }

}

void MusicalNoiseClean::process (juce::AudioBuffer<float>& buffer) noexcept
{
    // Static-noise LEARN is analysis-only. Never put a delayed FFT wet/dry
    // signal into the live microphone path, including during the two-second
    // calibration. No allocation or blocking; FFT state is preallocated.
    if (learnRequested.exchange (false, std::memory_order_acq_rel))
        beginLearningOnAudioThread();
    if (! learningActive.load (std::memory_order_acquire))
        return;

    const int channels = juce::jmin (channelCount, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    for (int sample = 0; sample < samples; ++sample)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            const float input = buffer.getSample (channel, sample);
            const float analysed = subsonicFilters[static_cast<size_t> (channel)].process (input);
            inputRing[static_cast<size_t> (channel)][static_cast<size_t> (inputWritePos)] = analysed;
            learningTimeDomainEnergy += static_cast<double> (analysed) * analysed;
            ++learningTimeDomainSamples;
        }
        if (channels == 1)
            inputRing[1][static_cast<size_t> (inputWritePos)]
                = inputRing[0][static_cast<size_t> (inputWritePos)];
        if (++inputWritePos >= fftSize) inputWritePos = 0;
        ++samplesSeen;
        if (++hopCounter >= hopSize && samplesSeen >= fftSize)
        {
            hopCounter = 0;
            processFrame();
            if (! learningActive.load (std::memory_order_relaxed))
                break;
        }
    }
}

void SmoothDownwardExpander::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

void SmoothDownwardExpander::reset() noexcept
{
    detectorEnvelope = 0.0f;
    gainDbState = 0.0f;
    holdSamplesRemaining = 0;
    openState = true;
    gainReductionDb.store (0.0f);
}

void SmoothDownwardExpander::setAmount (float value01) noexcept
{
    amount.store (juce::jlimit (0.0f, 1.0f, value01));
}

void SmoothDownwardExpander::process (juce::AudioBuffer<float>& target,
                                      const juce::AudioBuffer<float>& detector) noexcept
{
    if (! enabled.load())
    {
        gainReductionDb.store (0.0f);
        return;
    }

    const int samples = juce::jmin (target.getNumSamples(), detector.getNumSamples());
    const int channels = juce::jmin (2, juce::jmin (target.getNumChannels(), detector.getNumChannels()));
    if (channels <= 0 || samples <= 0)
        return;

    const float value = amount.load();
    // Silence 0% is a true unity setting; higher values progressively add
    // gentle downward expansion and a deeper maximum range.
    const float ratio = juce::jmap (value, 1.0f, 1.75f);
    const float rangeDb = juce::jmap (value, 0.0f, 30.0f);
    const float kneeDb = 8.0f;
    const float thresholdDb = juce::jlimit (-86.0f, -36.0f,
                                            learnedNoiseFloorDb.load() + thresholdOffsetDb.load());
    const float openThresholdDb = thresholdDb + 3.0f;
    const int holdLength = static_cast<int> (sampleRate * juce::jmap (value, 0.145f, 0.085f));

    const float detectorAttack = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.008));
    const float detectorRelease = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.085));
    const float gainOpen = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.004));
    const float gainClose = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.45));

    for (int sample = 0; sample < samples; ++sample)
    {
        float power = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
        {
            const float x = detector.getSample (channel, sample);
            power += x * x;
        }
        const float rms = std::sqrt (power / static_cast<float> (channels) + 1.0e-14f);
        const float detectorAlpha = rms > detectorEnvelope ? detectorAttack : detectorRelease;
        detectorEnvelope += detectorAlpha * (rms - detectorEnvelope);
        const float levelDb = gainToDb (detectorEnvelope);

        if (openState)
        {
            if (levelDb < thresholdDb)
            {
                if (holdSamplesRemaining > 0)
                    --holdSamplesRemaining;
                else
                    openState = false;
            }
            else
            {
                holdSamplesRemaining = holdLength;
            }
        }
        else if (levelDb > openThresholdDb)
        {
            openState = true;
            holdSamplesRemaining = holdLength;
        }

        float desiredGainDb = 0.0f;
        if (! openState)
        {
            const float hardGain = juce::jmax (-rangeDb, (ratio - 1.0f) * (levelDb - thresholdDb));
            const float kneeStart = thresholdDb - kneeDb * 0.5f;
            const float kneeEnd = thresholdDb + kneeDb * 0.5f;
            if (levelDb <= kneeStart)
                desiredGainDb = hardGain;
            else if (levelDb < kneeEnd)
            {
                const float mix = 1.0f - smoothStep ((levelDb - kneeStart) / kneeDb);
                desiredGainDb = hardGain * mix;
            }
        }

        const float gainAlpha = desiredGainDb > gainDbState ? gainOpen : gainClose;
        gainDbState += gainAlpha * (desiredGainDb - gainDbState);
        const float gain = dbToGain (gainDbState);

        for (int channel = 0; channel < target.getNumChannels(); ++channel)
            target.setSample (channel, sample, target.getSample (channel, sample) * gain);
    }

    gainReductionDb.store (-gainDbState);
}

void LiveSafetyLimiter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

void LiveSafetyLimiter::reset() noexcept
{
    envelope = 0.0f;
    gainReductionDb.store (0.0f);
}

void LiveSafetyLimiter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels <= 0)
        return;

    const float ceiling = dbToGain (ceilingDb.load());
    const float release = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.095));
    float maxReduction = 0.0f;

    for (int sample = 0; sample < samples; ++sample)
    {
        float peak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, sample)));

        if (peak > envelope)
            envelope = peak;
        else
            envelope += release * (peak - envelope);

        const float gain = envelope > ceiling ? ceiling / (envelope + 1.0e-12f) : 1.0f;
        maxReduction = juce::jmax (maxReduction, -gainToDb (gain));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample, buffer.getSample (channel, sample) * gain);
    }

    gainReductionDb.store (maxReduction);
}

} // namespace askp::live
