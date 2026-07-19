#include "PluginProcessor.h"
#include "arsonkupik_dsp.hpp"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#ifndef SONKUPIK_PARITY_VARIANT
#define SONKUPIK_PARITY_VARIANT "unknown"
#endif

namespace {
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;
constexpr double kDurationSeconds = 5.0;
constexpr double kAnalysisStartSeconds = 3.0;
constexpr double kPi = juce::MathConstants<double>::pi;

void setPlainParameter (askp::Processor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float deterministicNoise (std::uint32_t sample, std::uint32_t channel) noexcept
{
    std::uint32_t x = sample * 747796405u + channel * 2891336453u + 277803737u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return (static_cast<float> (x & 0x00ffffffu) / 8388607.5f) - 1.0f;
}

void generateInputBlock (juce::AudioBuffer<float>& block, std::int64_t startSample)
{
    for (int i = 0; i < block.getNumSamples(); ++i)
    {
        const auto sample = startSample + i;
        const double t = static_cast<double> (sample) / kSampleRate;
        const double beatPhase = std::fmod (t, 0.5);
        const double kickEnv = beatPhase < 0.24 ? std::exp (-beatPhase / 0.075) : 0.0;
        const double kickSweep = 49.0 + 62.0 * std::exp (-beatPhase / 0.032);
        const double kick = kickEnv * std::sin (2.0 * kPi * kickSweep * beatPhase);
        const double phrase = 0.82 + 0.10 * std::sin (2.0 * kPi * 0.31 * t)
                                   + 0.06 * std::sin (2.0 * kPi * 0.113 * t + 0.4);

        const double tonalL = 0.070 * std::sin (2.0 * kPi * 43.0 * t)
                            + 0.052 * std::sin (2.0 * kPi * 86.0 * t + 0.15)
                            + 0.037 * std::sin (2.0 * kPi * 172.0 * t + 0.37)
                            + 0.028 * std::sin (2.0 * kPi * 440.0 * t + 0.62)
                            + 0.021 * std::sin (2.0 * kPi * 1760.0 * t + 0.31)
                            + 0.012 * std::sin (2.0 * kPi * 7040.0 * t + 0.83);
        const double tonalR = 0.067 * std::sin (2.0 * kPi * 43.0 * t + 0.012)
                            + 0.050 * std::sin (2.0 * kPi * 86.0 * t + 0.19)
                            + 0.035 * std::sin (2.0 * kPi * 172.0 * t + 0.43)
                            + 0.030 * std::sin (2.0 * kPi * 440.0 * t + 0.56)
                            + 0.020 * std::sin (2.0 * kPi * 1760.0 * t + 0.39)
                            + 0.011 * std::sin (2.0 * kPi * 7040.0 * t + 0.91);

        const float hissL = deterministicNoise (static_cast<std::uint32_t> (sample), 0u) * 0.0012f;
        const float hissR = deterministicNoise (static_cast<std::uint32_t> (sample), 1u) * 0.0012f;
        block.setSample (0, i, static_cast<float> ((tonalL * phrase) + kick * 0.22) + hissL);
        block.setSample (1, i, static_cast<float> ((tonalR * phrase) + kick * 0.205) + hissR);
    }
}

struct Fingerprint
{
    std::uint64_t hash = 1469598103934665603ull;
    double sumSquares = 0.0;
    float peak = 0.0f;
    std::uint64_t samples = 0;

    void push (float value) noexcept
    {
        std::uint32_t bits = 0;
        static_assert (sizeof (bits) == sizeof (value));
        std::memcpy (&bits, &value, sizeof (bits));
        for (int byte = 0; byte < 4; ++byte)
        {
            hash ^= static_cast<std::uint8_t> ((bits >> (byte * 8)) & 0xffu);
            hash *= 1099511628211ull;
        }
        sumSquares += static_cast<double> (value) * value;
        peak = juce::jmax (peak, std::abs (value));
        ++samples;
    }

    juce::String hashString() const
    {
        std::ostringstream stream;
        stream << std::hex << std::setw (16) << std::setfill ('0') << hash;
        return stream.str();
    }

    double rms() const noexcept
    {
        return samples > 0 ? std::sqrt (sumSquares / static_cast<double> (samples)) : 0.0;
    }
};

juce::var renderPreset (int presetIndex)
{
    askp::Processor processor;
    processor.scopeTap.setEnabled (false);

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    if (processor.checkBusesLayoutSupported (layout))
        processor.setBusesLayout (layout);

    setPlainParameter (processor, "preset", static_cast<float> (presetIndex));
    processor.applyPresetMacroToParameters (presetIndex);
    setPlainParameter (processor, "input_gain_db", 0.0f);
    setPlainParameter (processor, "final_output_gain_db", 0.0f);
    setPlainParameter (processor, "bypass", 0.0f);

    processor.setNonRealtime (true);
    processor.setProcessingPrecision (juce::AudioProcessor::singlePrecision);
    processor.setRateAndBufferSizeDetails (kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    const auto totalSamples = static_cast<std::int64_t> (std::llround (kDurationSeconds * kSampleRate));
    const auto analysisStart = static_cast<std::int64_t> (std::llround (kAnalysisStartSeconds * kSampleRate));
    juce::AudioBuffer<float> block (2, kBlockSize);
    juce::MidiBuffer midi;
    Fingerprint fingerprint;

    for (std::int64_t offset = 0; offset < totalSamples; offset += kBlockSize)
    {
        generateInputBlock (block, offset);
        midi.clear();
        processor.processBlock (block, midi);

        const int first = offset < analysisStart
                        ? static_cast<int> (juce::jlimit<std::int64_t> (0, kBlockSize, analysisStart - offset))
                        : 0;
        for (int i = first; i < kBlockSize && offset + i < totalSamples; ++i)
        {
            fingerprint.push (block.getSample (0, i));
            fingerprint.push (block.getSample (1, i));
        }
    }

    const auto& factory = arsonkupik::factory_presets();
    const auto& preset = factory[static_cast<size_t> (presetIndex)];
    auto* object = new juce::DynamicObject();
    object->setProperty ("index", presetIndex);
    object->setProperty ("id", juce::String (preset.id));
    object->setProperty ("name", juce::String (preset.name));
    object->setProperty ("latencySamples", processor.getLatencySamples());
    object->setProperty ("analysedSamples", static_cast<juce::int64> (fingerprint.samples));
    object->setProperty ("fnv1a64", fingerprint.hashString());
    object->setProperty ("rms", fingerprint.rms());
    object->setProperty ("peak", fingerprint.peak);

    processor.releaseResources();
    return juce::var (object);
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (argc < 2)
    {
        std::fprintf (stderr, "Usage: parity-renderer <output.json>\n");
        return 2;
    }

    juce::Array<juce::var> presets;
    const auto count = static_cast<int> (arsonkupik::factory_presets().size());
    for (int index = 0; index < count; ++index)
    {
        std::printf ("[%s] preset %d/%d\n", SONKUPIK_PARITY_VARIANT, index + 1, count);
        presets.add (renderPreset (index));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("variant", SONKUPIK_PARITY_VARIANT);
    root->setProperty ("sampleRate", kSampleRate);
    root->setProperty ("blockSize", kBlockSize);
    root->setProperty ("durationSeconds", kDurationSeconds);
    root->setProperty ("analysisStartSeconds", kAnalysisStartSeconds);
    root->setProperty ("presetCount", count);
    root->setProperty ("presets", juce::var (presets));

    const juce::File output (juce::String::fromUTF8 (argv[1]));
    output.getParentDirectory().createDirectory();
    if (! output.replaceWithText (juce::JSON::toString (juce::var (root), true)))
    {
        std::fprintf (stderr, "Could not write %s\n", argv[1]);
        return 3;
    }

    return 0;
}
