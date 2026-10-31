#include "InputVst3Rack.h"
#include <cmath>

namespace askp::live {

juce::String InputVst3Rack::load (const juce::File& file, double sampleRate, int blockSize)
{
    // Explicit user selection only: no recursive plugin search on startup.
    if (! file.exists() || ! file.hasFileExtension (".vst3"))
        return "Select an installed .vst3 module or bundle.";
    if (sampleRate <= 0.0 || blockSize <= 0)
        return "Open an audio device before loading a VST3 effect.";

    juce::VST3PluginFormat scanner;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    scanner.findAllTypesForFile (descriptions, file.getFullPathName());
    if (descriptions.isEmpty())
        return "No compatible VST3 effect found in the selected path.";

    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> candidate;
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
        return "VST3 must support exactly one stereo input and stereo output bus.";

    clear();
    instance = std::move (candidate);
    name = instance->getName();
    bypassed.store (false, std::memory_order_release);
    return {};
}

void InputVst3Rack::clear()
{
    release();
    instance.reset();
    name.clear();
}

void InputVst3Rack::prepare (double sampleRate, int blockSize)
{
    if (instance == nullptr) return;
    release();
    instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);
    instance->reset();
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
    if (! prepared || instance == nullptr || bypassed.load (std::memory_order_relaxed))
        return;
    midi.clear();
    instance->processBlock (buffer, midi);
    // Do not let NaN/Inf contaminate the master protection chain.
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* const samples = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (samples[sample]))
                samples[sample] = 0.0f;
    }
}

} // namespace askp::live
