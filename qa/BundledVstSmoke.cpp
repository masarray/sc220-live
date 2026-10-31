#include "InputVst3Rack.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (argc != 2) { std::cerr << "Usage: smoke <bundled .vst3 path>\n"; return 2; }
    const auto bundle = juce::File (juce::String::fromUTF8 (argv[1]));
    if (! bundle.exists() || ! bundle.hasFileExtension (".vst3"))
        { std::cerr << "Bundled VST3 not found\n"; return 2; }

    constexpr int blockSize = 256;
    constexpr double rate = 48000.0;
    askp::live::InputVst3Rack voices[2];
    const char* required[] = { "Factory Preset", "Enhance", "Smart Bass",
                               "Smart Treble", "Vocal Body", "Stereo Magic", "Bypass" };
    for (int lane = 0; lane < 2; ++lane)
    {
        askp::live::InputVst3Rack::PreparedInstance prepared;
        auto error = askp::live::InputVst3Rack::createPrepared (bundle, rate, blockSize, prepared);
        if (error.isNotEmpty()) { std::cerr << "VST instantiate failed: " << error << "\n"; return 3; }
        for (const auto* requiredName : required)
        {
            bool found = false;
            for (auto* param : prepared->getParameters())
                if (param && param->getName (128).equalsIgnoreCase (requiredName))
                    found = true;
            if (! found) { std::cerr << "Missing VST parameter: " << requiredName << "\n"; return 4; }
        }
        voices[lane].install (std::move (prepared), rate, blockSize);
    }

    juce::AudioBuffer<float> samples (2, blockSize);
    juce::MidiBuffer midi;
    for (int block = 0; block < 32; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            samples.clear (channel, 0, blockSize);
            if (block == 0) samples.setSample (channel, 0, 0.125f);
        }
        midi.clear();
        voices[block % 2].setBypassed (block >= 16);
        voices[block % 2].process (samples, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
                if (! std::isfinite (samples.getSample (channel, sample)))
                    { std::cerr << "Nonfinite output in VST processing\n"; return 5; }
    }
    std::cout << "PASS: two official VST3 instances loaded, all required parameters present, output finite\n";
    return 0;
}
