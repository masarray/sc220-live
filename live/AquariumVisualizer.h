#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace askp::live {

class AquariumVisualizer final : public juce::Component
{
public:
    explicit AquariumVisualizer (askp::Processor& processor);

    void refresh (bool bypassed);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kCaptureSamples = 4096;
    static constexpr int kBands = 48;
    static constexpr int kNodes = 48;
    static constexpr int kBubbles = 10;

    struct Bubble
    {
        float x = 0.0f;
        float y = 0.0f;
        float radius = 2.0f;
        float speed = 0.5f;
        float life = 0.0f;
        float opacity = 0.2f;
    };

    void seedBubble (Bubble& bubble, bool anywhere);
    void advanceBubbles (float energy, bool active);
    void updateBandsFromScope();
    void updateFluid (bool active);
    float bandAt (int index) const noexcept;

    askp::Processor& processor;
    std::array<float, kCaptureSamples> left{};
    std::array<float, kCaptureSamples> right{};
    std::array<float, kBands> bands{};
    std::array<float, kBands> bandTargets{};
    std::array<float, kNodes> nodes{};
    std::array<float, kNodes> velocities{};
    std::array<Bubble, kBubbles> bubbles{};
    juce::Random random { 0x534B4C56 };
    juce::Image staticLayer;
    bool bypassState = false;
    bool seeded = false;
    float globalEnergy = 0.0f;
    float lowEnergy = 0.0f;
    float phase = 0.0f;
    int ambientCountdown = 0;
};

} // namespace askp::live

