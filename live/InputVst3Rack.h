#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

namespace askp::live {

// A single user-selected, zero-added-host-latency stereo VST3 voice insert.
// Creation/preparation and retirement run on the message thread; the caller
// must quiesce audio callbacks before install()/clear() or device preparation.
// process() uses only preallocated memory and never acquires a host lock.
class InputVst3Rack final
{
public:
    InputVst3Rack() = default;
    using PreparedInstance = std::unique_ptr<juce::AudioPluginInstance>;
    static juce::String createPrepared (const juce::File& file, double sampleRate,
                                        int blockSize, PreparedInstance& result);
    void install (PreparedInstance prepared, double sampleRate, int maximumBlockSize);
    void clear();
    void prepare (double sampleRate, int maximumBlockSize);
    void release();
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&) noexcept;
    void setBypassed (bool value) noexcept { bypassed.store (value, std::memory_order_release); }
    bool isBypassed() const noexcept { return bypassed.load (std::memory_order_acquire); }
    bool isLoaded() const noexcept { return instance != nullptr; }
    juce::String getName() const { return name; }
    int getLatencySamples() const noexcept
    {
        return prepared && instance != nullptr ? juce::jmax (0, instance->getLatencySamples()) : 0;
    }
    juce::AudioPluginInstance* getProcessor() const noexcept { return instance.get(); }
private:
    void allocateDryAlignment (double sampleRate, int maximumBlockSize);
    PreparedInstance instance;
    juce::String name;
    std::atomic<bool> bypassed { false };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix;
    juce::AudioBuffer<float> originalDry;
    juce::AudioBuffer<float> delayAlignedDry;
    int dryDelayPosition = 0;
    int dryDelaySamples = 0;
    bool prepared = false;

    static constexpr int maximumAcceptedPluginLatency = 8192;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputVst3Rack)
};
} // namespace askp::live
