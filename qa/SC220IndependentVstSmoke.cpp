#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 3)
    {
        std::cerr << "Usage: SC220PluginSmoke <ArSonKuPik.vst3> <ArSonVerb.vst3>\n";
        return 2;
    }
    juce::AudioPluginFormatManager formats;
    formats.addFormat(new juce::VST3PluginFormat());
    constexpr int block = 256;
    constexpr double rate = 48000.0;
    const char* expected[] = { "Enhance","Smart Bass","Smart Treble",
                                "Vocal Body","Stereo Magic" };
    for (int product = 0; product < 2; ++product)
    {
        const auto bundle = juce::File(juce::String::fromUTF8(argv[product + 1]));
        if (!bundle.exists() || !bundle.hasFileExtension(".vst3"))
        {
            std::cerr << "Missing .vst3: " << bundle.getFullPathName() << "\n";
            return 3;
        }
        juce::VST3PluginFormat vst;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        vst.findAllTypesForFile(descriptions, bundle.getFullPathName());
        std::cout << "Found " << descriptions.size() << " plugin types in "
                  << bundle.getFileName() << "\n";
        if (descriptions.isEmpty()) return 4;
        for (int instanceIndex = 0; instanceIndex < (product == 0 ? 2 : 1); ++instanceIndex)
        {
            juce::String failure;
            std::unique_ptr<juce::AudioPluginInstance> instance;
            for (auto* d : descriptions)
            {
                if (d == nullptr || d->isInstrument) continue;
                instance = formats.createPluginInstance(*d, rate, block, failure);
                if (instance) break;
            }
            if (!instance)
            {
                std::cerr << "VST3 instantiation failed: " << failure << "\n";
                return 5;
            }
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::stereo());
            layout.outputBuses.add(juce::AudioChannelSet::stereo());
            if (!instance->setBusesLayout(layout)
                || instance->getTotalNumInputChannels() != 2
                || instance->getTotalNumOutputChannels() != 2)
            {
                std::cerr << "Stereo bus unsupported: " << instance->getName() << "\n";
                return 6;
            }
            if (product == 0)
            {
                for (const auto* required : expected)
                {
                    bool available = false;
                    for (auto* p : instance->getParameters())
                        if (p && p->getName(128).removeCharacters(" _-")
                                      .equalsIgnoreCase(juce::String(required).removeCharacters(" _-")))
                            available = true;
                    if (!available)
                    {
                        std::cerr << "Missing signature macro: " << required << "\n";
                        return 7;
                    }
                }
            }
            instance->prepareToPlay(rate, block);
            juce::AudioBuffer<float> audio(2, block);
            juce::MidiBuffer midi;
            for (int frame = 0; frame < 12; ++frame)
            {
                audio.clear();
                if (frame == 0)
                    for (int channel = 0; channel < 2; ++channel)
                        audio.setSample(channel, 0, 0.125f);
                midi.clear();
                instance->processBlock(audio, midi);
                for (int channel = 0; channel < 2; ++channel)
                    for (int sample = 0; sample < block; ++sample)
                        if (!std::isfinite(audio.getSample(channel, sample)))
                        {
                            std::cerr << "Non-finite DSP output\n";
                            return 8;
                        }
            }
            instance->releaseResources();
            std::cout << "PASS: " << instance->getName()
                      << " loaded, stereo processed, no NaN/Inf\n";
        }
    }
    return 0;
}
