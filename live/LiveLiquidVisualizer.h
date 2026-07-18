#pragma once

#include "LiveAudioEngine.h"
#include "LiveTheme.h"

#include <array>

namespace askp::live {

// Live adaptation of the current ArSonKuPik liquid visualizer architecture.
// It deliberately uses no FFT: the audio callback only pushes into a lossy
// lock-free tap; six broad musical bands, onset envelopes and fluid physics run
// on the 24 Hz message-thread refresh.
class LiveLiquidVisualizer final : public juce::Component
{
public:
    explicit LiveLiquidVisualizer (LiveAudioEngine& engineToUse);
    ~LiveLiquidVisualizer() override;

    // Returns true when a new visible frame was generated.
    bool refresh (bool bypassed);
    void setVisualEnabled (bool enabled);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int analysisSamples = 4096;
    static constexpr int visualBands = 6;
    static constexpr int fluidNodes = 32;
    static constexpr int bubbleCount = 8;
    static constexpr int dropletCount = 12;

    struct Bubble
    {
        float x = 0.0f, y = 0.72f, radius = 0.014f;
        float rise = 0.004f, sway = 0.002f, phase = 0.0f;
        float opacity = 0.22f, life = 0.0f;
    };

    struct Droplet
    {
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
        float size = 1.0f, brightness = 0.0f, life = 0.0f;
    };

    void rebuildStaticLayers();
    void analyse (int sampleCount, double sampleRate);
    void advanceFluid (bool active);
    void injectPulse (float xNormalised, float radius, float strength, float direction);
    void emitBubble (float xNormalised, float strength, bool micro);
    void emitDroplets (float xNormalised, float strength, int count);
    void advanceParticles (bool active);
    float surfaceAt (float xNormalised) const noexcept;
    void resetAnalysis() noexcept;

    LiveAudioEngine& engine;
    std::array<float, analysisSamples> leftSamples {};
    std::array<float, analysisSamples> rightSamples {};
    std::array<float, visualBands> bandFast {};
    std::array<float, visualBands> bandSlow {};
    std::array<float, visualBands> bandDrive {};
    std::array<float, visualBands - 1> poleALeft {};
    std::array<float, visualBands - 1> poleBLeft {};
    std::array<float, visualBands - 1> poleARight {};
    std::array<float, visualBands - 1> poleBRight {};
    std::array<float, fluidNodes> fluidHeight {};
    std::array<float, fluidNodes> fluidVelocity {};
    std::array<Bubble, bubbleCount> bubbles {};
    std::array<Droplet, dropletCount> droplets {};

    juce::Image staticBack;
    juce::Image staticFront;
    juce::Image topGloss;
    juce::Image glossAsset;
    juce::Random random { 0x4d617341 };

    double analysisRate = 0.0;
    float totalLevel = 0.0f;
    float bassEnergy = 0.0f;
    float bodyEnergy = 0.0f;
    float airEnergy = 0.0f;
    float slosh = 0.0f;
    float sloshVelocity = 0.0f;
    float animationPhase = 0.0f;
    int lowCooldown = 0;
    int bodyCooldown = 0;
    int airCooldown = 0;
    int bubbleCursor = 0;
    int dropletCursor = 0;
    int ambientBubbleCountdown = 0;
    int silentFrames = 0;
    int emptyFrames = 0;
    bool bypassState = false;
    bool visualEnabled = false;
    bool seeded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveLiquidVisualizer)
};

} // namespace askp::live
