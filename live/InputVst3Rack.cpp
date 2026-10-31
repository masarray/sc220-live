#include "InputVst3Rack.h"
#include <cmath>

namespace askp::live {

juce::String InputVst3Rack::createPrepared (const juce::File& file, double sampleRate,
                                           int blockSize, PreparedInstance& result)
{
    result.reset();
    if (! file.exists() || ! file.hasFileExtension (".vst3"))
        return "Select an installed .vst3 module or bundle.";
    if (sampleRate <= 0.0 || blockSize <= 0)
        return "Open an audio device before loading a VST3 effect.";

    // Deliberately scan only the user-selected bundle; no startup plugin scan.
    juce::VST3PluginFormat scanner;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    scanner.findAllTypesForFile (descriptions, file.getFullPathName());
    if (descriptions.isEmpty())
        return "No compatible VST3 audio effect found in the selected path.";

    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());
    juce::String error;
    PreparedInstance candidate;
    for (auto* desc : descriptions)
    {
        if (desc == nullptr || desc->isInstrument)
            continue;
        candidate = formats.createPluginInstance (*desc, sampleRate, blockSize, error);
        if (candidate != nullptr)
            break;
    }
    if (candidate == nullptr)
        return error.isNotEmpty() ? error : "No compatible audio effect could be created.";

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    if (! candidate->setBusesLayout (layout)
        || candidate->getTotalNumInputChannels() != 2
        || candidate->getTotalNumOutputChannels() != 2)
        return "The VST3 must support exactly one stereo input/output bus.";

    // Initialisation is strictly outside the realtime callback, following
    // Steinberg's VST3 main-thread lifecycle contract.
    candidate->setRateAndBufferSizeDetails (sampleRate, blockSize);
    candidate->prepareToPlay (sampleRate, blockSize);
    candidate->reset();
    if (candidate->getLatencySamples() > maximumAcceptedPluginLatency)
    {
        candidate->releaseResources();
        return "Plugin latency exceeds the live voice rack's supported alignment (8192 samples).";
    }
    result = std::move (candidate);
    return {};
}

void InputVst3Rack::allocateDryAlignment (double sampleRate, int maximumBlockSize)
{
    const int capacity = juce::jmax (1, maximumBlockSize);
    dryDelaySamples = juce::jlimit (0, maximumAcceptedPluginLatency,
                                   instance != nullptr ? instance->getLatencySamples() : 0);
    originalDry.setSize (2, capacity, false, true, false);
    delayAlignedDry.setSize (2, juce::jmax (1, dryDelaySamples + 1), false, true, false);
    delayAlignedDry.clear();
    dryDelayPosition = 0;
    bypassMix.reset (sampleRate, 0.012);
    bypassMix.setCurrentAndTargetValue (isBypassed() ? 1.0f : 0.0f);
}

void InputVst3Rack::install (PreparedInstance replacement,
                             double sampleRate, int maximumBlockSize)
{
    release();
    instance = std::move (replacement);
    name = instance != nullptr ? instance->getName() : juce::String();
    bypassed.store (false, std::memory_order_release);
    prepared = instance != nullptr;
    if (prepared)
        allocateDryAlignment (sampleRate, maximumBlockSize);
}

void InputVst3Rack::clear()
{
    release();
    instance.reset();
    name.clear();
    originalDry.setSize (0, 0);
    delayAlignedDry.setSize (0, 0);
    dryDelayPosition = dryDelaySamples = 0;
}

void InputVst3Rack::prepare (double sampleRate, int maximumBlockSize)
{
    if (instance == nullptr) return;
    release();
    instance->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
    instance->prepareToPlay (sampleRate, maximumBlockSize);
    instance->reset();
    allocateDryAlignment (sampleRate, maximumBlockSize);
    prepared = true;
}

void InputVst3Rack::release()
{
    if (instance != nullptr && prepared)
        instance->releaseResources();
    prepared = false;
}

void InputVst3Rack::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept
{
    if (! prepared || instance == nullptr)
        return;
    const int samples = buffer.getNumSamples();
    if (samples <= 0 || samples > originalDry.getNumSamples() || buffer.getNumChannels() != 2)
        return; // Never allocate in the callback, even after a driver block-size change.

    for (int channel = 0; channel < 2; ++channel)
        originalDry.copyFrom (channel, 0, buffer, channel, 0, samples);

    midi.clear();
    instance->processBlock (buffer, midi);
    bypassMix.setTargetValue (isBypassed() ? 1.0f : 0.0f);

    // The VST3 remains warm through bypass, so its reverb tail/state survives.
    // Dry samples are delayed by the plugin-reported latency for aligned A/B.
    const int ringSize = delayAlignedDry.getNumSamples();
    for (int i = 0; i < samples; ++i)
    {
        const float dryWeight = bypassMix.getNextValue();
        const int readPosition = (dryDelayPosition + ringSize - dryDelaySamples) % ringSize;
        for (int channel = 0; channel < 2; ++channel)
        {
            const float input = originalDry.getSample (channel, i);
            delayAlignedDry.setSample (channel, dryDelayPosition, input);
            const float alignedDry = delayAlignedDry.getSample (channel, readPosition);
            const float wet = buffer.getSample (channel, i);
            const float safeWet = std::isfinite (wet) ? wet : 0.0f;
            buffer.setSample (channel, i, safeWet * (1.0f - dryWeight) + alignedDry * dryWeight);
        }
        if (++dryDelayPosition == ringSize)
            dryDelayPosition = 0;
    }
}
} // namespace askp::live
