#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace askp {

// MasArray Sonic Core — a spherical glass vessel filled with violet liquid.
// A lightweight OBS-style filter bank drives generic low-frequency impacts;
// no kick classifier, tempo detector or beat prediction participates in the
// surface motion. The liquid silhouette is the hero animation. A tiny fixed
// bubble pool and sparse bass-led splash droplets provide depth without the
// glitter field and alpha-heavy fantasy effects used by earlier revisions.
// Everything runs on the editor timer; the audio thread only writes the
// lock-free visual tap.
class OrbVisualizer : public juce::Component {
public:
    explicit OrbVisualizer(Processor& p);

    // Called from the editor timer (~24 Hz).
    void refresh(bool bypassed);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // The visual filter bank only needs the newest 4096 samples. ScopeTap
    // discards any older backlog before copying, so high sample rates never
    // pay for an 8192-sample copy merely to throw half of it away.
    static constexpr int kAnalysisSamples = 4096;
    static constexpr int kVisualBands = 6;             // sub .. cymbal air
    static constexpr int kFluidNodes = 32;             // compact spring lattice
    static constexpr int kBubbles = 8;                 // fixed pool; typically 3-5 visible
    static constexpr int kDroplets = 12;               // sparse bass-led splash pool

    struct Bubble {
        float x = 0.0f, y = 0.70f, r = 0.014f;
        float rise = 0.004f;
        float swayAmp = 0.002f, swayRate = 0.9f, swayPhase = 0.0f;
        float depth = 0.7f;                            // 0 = rear, 1 = front
        float opacity = 0.20f;
        float life = 0.0f;                             // 0 = inactive
    };

    struct Droplet {
        float x = 0.0f, y = 0.0f;                     // bowl-normalised coordinates
        float vx = 0.0f, vy = 0.0f;
        float life = 0.0f;                            // 0 = inactive, 1 = emitted
        float size = 1.0f;
        float brightness = 0.0f;
    };

    void  rebuildStaticLayer();
    void  seedAmbientBubble(Bubble& b, bool anywhere);
    void  emitBubble(float xNorm, float strength, bool micro);
    void  advanceBubbles(bool active);
    void  emitDroplets(float xNorm, float strength, int count,
                       bool wide, float brightness);
    void  advanceDroplets(bool active);
    void  refreshSilentFrame(bool bypassChanged);
    void  injectBallisticPulse(float xNorm, float radius, float strength, float direction);
    float strikeQuietRegion(float preferredX, float strength, float radius,
                            float stereoPan, bool avoidCentre = false);
    void  advanceFluid(bool active);
    // Liquid surface height (bowl-normalised y, y down) at xNorm in [0..1].
    float surfaceY(float xNorm) const;

    Processor& processor_;
    std::array<float, kAnalysisSamples> timeLeft_{};
    std::array<float, kAnalysisSamples> timeRight_{};
    std::array<Bubble,  (size_t)kBubbles>  bubbles_{};
    std::array<Droplet, (size_t)kDroplets> droplets_{};

    juce::Random rng_{0x4D617341};
    juce::Image staticLayer_;
    juce::Image staticFrontLayer_;
    juce::Image glossImage_;
    juce::Image topGlossLayer_;
    float level_ = 0.0f;
    float activity_ = 0.0f;
    std::array<float, kVisualBands> previousBandTarget_{};
    std::array<float, kVisualBands - 1> analysisPoleALeft_{};
    std::array<float, kVisualBands - 1> analysisPoleBLeft_{};
    std::array<float, kVisualBands - 1> analysisPoleARight_{};
    std::array<float, kVisualBands - 1> analysisPoleBRight_{};
    std::array<float, kVisualBands> analysisFastEnvelope_{};
    std::array<float, kVisualBands> analysisSlowEnvelope_{};
    float analysisSampleRate_ = 0.0f;
    bool  analysisPrimed_ = false;
    int   silentMeterFrames_ = 0;
    int   emptyCaptureFrames_ = 0;
    bool  analysisSleeping_ = false;
    std::array<float, kFluidNodes> fluidHeight_{};
    std::array<float, kFluidNodes> fluidVelocity_{};
    float lowEnergy_ = 0.0f;
    float bodyEnergy_ = 0.0f;
    float airEnergy_ = 0.0f;
    float topPulse_ = 0.0f;
    float bassFlow_ = 0.0f;
    float bassSpectrumFast_ = 0.0f;
    float bassSpectrumSlow_ = 0.0f;
    int   bassReturnCountdown_ = 0;
    float bassReturnX_ = 0.50f;
    float bassReturnRadius_ = 0.18f;
    float bassReturnStrength_ = 0.0f;
    int   bassReboundCountdown_ = 0;
    float bassReboundX_ = 0.50f;
    float bassReboundRadius_ = 0.12f;
    float bassReboundStrength_ = 0.0f;
    int   framesSincePercussion_ = 1000;
    int   impactSequence_ = 0;
    float lastImpactX_ = 0.50f;
    int   lowImpactCooldown_ = 0;
    int   percussionImpactCooldown_ = 0;
    int   bodyImpactCooldown_ = 0;
    int   airImpactCooldown_ = 0;
    float slosh_ = 0.0f;
    float sloshVelocity_ = 0.0f;
    int   bubbleWrite_ = 0;
    int   ambientBubbleCountdown_ = 0;
    int   dropletWrite_ = 0;
    float phase_ = 0.0f;
    bool  bypassed_ = false;
    bool  seeded_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrbVisualizer)
};

} // namespace askp
