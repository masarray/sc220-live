#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

namespace askp::live {
// One manually chosen trusted stereo VST3 on the SC220 voice lane.
// Ownership is changed only while LiveAudioEngine's callback is detached.
class InputVst3Rack final
{
public:
    juce::String load (const juce::File& file, double sampleRate, int blockSize);
    void clear();
    void prepare (double sampleRate, int blockSize);
    void release();
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept;
    void setBypassed (bool b) noexcept { bypassed.store (b, std::memory_order_relaxed); }
    bool isBypassed() const noexcept { return bypassed.load (std::memory_order_relaxed); }
    bool isLoaded() const noexcept { return instance != nullptr; }
    juce::String getName() const { return name; }
    int getLatencySamples() const noexcept { return prepared && instance != nullptr ? instance->getLatencySamples() : 0; }
    juce::AudioPluginInstance* getProcessor() const noexcept { return instance.get(); }
private:
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String name;
    std::atomic<bool> bypassed { false };
    bool prepared = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputVst3Rack)
};
} // namespace askp::live
