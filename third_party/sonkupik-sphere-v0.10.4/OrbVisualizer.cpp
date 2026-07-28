#include "OrbVisualizer.h"
#include "AskpLookAndFeel.h"
#include "BinaryData.h"

#include <array>
#include <cmath>
#include <limits>

namespace askp {

namespace {
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

// Liquid geometry (bowl-normalised units, radius = 1, y grows downward).
// A horizontal plane through a sphere's centre divides its volume exactly
// in half. All simulated waves are mean-balanced around this level.
constexpr float kRestLevel   = 0.0f;
constexpr int   kSurfacePts  = 40;
} // namespace

OrbVisualizer::OrbVisualizer(Processor& p) : processor_(p)
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false);
    glossImage_ = juce::ImageCache::getFromMemory(BinaryData::askp_sphere_gloss_png,
                                                   BinaryData::askp_sphere_gloss_pngSize);
}

void OrbVisualizer::resized()
{
    rebuildStaticLayer();
}

void OrbVisualizer::rebuildStaticLayer()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty()) {
        staticLayer_ = juce::Image();
        staticFrontLayer_ = juce::Image();
        topGlossLayer_ = juce::Image();
        return;
    }

    staticLayer_ = juce::Image(juce::Image::ARGB, bounds.getWidth(),
                               bounds.getHeight(), true);
    juce::Graphics g(staticLayer_);
    const auto floatBounds = bounds.toFloat();
    const auto centre = floatBounds.getCentre();
    const float R = juce::jmin(floatBounds.getWidth(), floatBounds.getHeight()) * 0.5f - 3.0f;
    const float r = R - 6.5f;
    const auto hot = bypassed_ ? juce::Colour(0xFF8A8593) : palette::violetHot;
    const auto deep = bypassed_ ? juce::Colour(0xFF17151C) : palette::violetDeep;
    const float masterAlpha = bypassed_ ? 0.45f : 1.0f;

    // Cache the alpha-heavy but completely static bezel and vessel body. The
    // live frame only has to draw the liquid, a handful of bubbles and splash.
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillEllipse(centre.x - R, centre.y - R + 5.0f, R * 2.0f, R * 2.0f);

    // Sonkupik Live approved single bezel: metallic navy-black matching the rack UX.
    juce::ColourGradient rim(juce::Colour(0xFF2B3948), centre.x - R * 0.30f, centre.y - R,
                             juce::Colour(0xFF070D15), centre.x + R * 0.36f, centre.y + R, false);
    rim.addColour(0.20, juce::Colour(0xFF223142));
    rim.addColour(0.46, juce::Colour(0xFF111B27));
    rim.addColour(0.78, juce::Colour(0xFF0A1018));
    g.setGradientFill(rim);
    g.fillEllipse(centre.x - R, centre.y - R, R * 2.0f, R * 2.0f);

    juce::ColourGradient bowl(juce::Colour(0xFF16121F), centre.x, centre.y - r,
                              juce::Colour(0xFF030307), centre.x, centre.y + r, false);
    bowl.addColour(0.55, deep.interpolatedWith(juce::Colour(0xFF0A0812), 0.55f)
                              .withAlpha(0.85f * masterAlpha));
    g.setGradientFill(bowl);
    g.fillEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    // Front glass vignette and rim strokes are also static. Keeping them in a
    // separate layer preserves their correct z-order above liquid and gloss.
    staticFrontLayer_ = juce::Image(juce::Image::ARGB, bounds.getWidth(),
                                    bounds.getHeight(), true);
    juce::Graphics frontG(staticFrontLayer_);
    juce::Path frontGlassClip;
    frontGlassClip.addEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    frontG.saveState();
    frontG.reduceClipRegion(frontGlassClip);

    // Edge-only glass shading keeps the sphere round without creating a
    // secondary semi-transparent orb in the centre. The liquid area must stay
    // readable; therefore all darkening originates from the edges, not from a
    // radial centre-based vignette.
    juce::ColourGradient leftShade(juce::Colours::black.withAlpha(0.18f * masterAlpha),
                                   centre.x - r, centre.y,
                                   juce::Colours::transparentBlack,
                                   centre.x - r * 0.42f, centre.y, false);
    frontG.setGradientFill(leftShade);
    frontG.fillRect(juce::Rectangle<float>(centre.x - r, centre.y - r,
                                           r * 0.82f, r * 2.0f));

    juce::ColourGradient rightShade(juce::Colours::transparentBlack,
                                    centre.x + r * 0.42f, centre.y,
                                    juce::Colours::black.withAlpha(0.14f * masterAlpha),
                                    centre.x + r, centre.y, false);
    frontG.setGradientFill(rightShade);
    frontG.fillRect(juce::Rectangle<float>(centre.x + r * 0.18f, centre.y - r,
                                           r * 0.82f, r * 2.0f));

    juce::ColourGradient bottomShade(juce::Colours::transparentBlack,
                                     centre.x, centre.y + r * 0.10f,
                                     juce::Colours::black.withAlpha(0.14f * masterAlpha),
                                     centre.x, centre.y + r, false);
    frontG.setGradientFill(bottomShade);
    frontG.fillRect(juce::Rectangle<float>(centre.x - r, centre.y + r * 0.02f,
                                           r * 2.0f, r * 0.98f));

    frontG.restoreState();
    frontG.setColour(juce::Colours::black.withAlpha(0.50f));
    frontG.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.1f);
    frontG.setColour(hot.withAlpha(0.20f * masterAlpha));
    frontG.drawEllipse(centre.x - r + 3.0f, centre.y - r + 3.0f,
                       r * 2.0f - 6.0f, r * 2.0f - 6.0f, 1.0f);

    // Preserve only the attractive upper reflection from the original gloss
    // artwork. The lower half is deliberately cropped out so it cannot create
    // a circular/elliptical blob in the centre of the vessel.
    topGlossLayer_ = juce::Image(juce::Image::ARGB, bounds.getWidth(),
                                 bounds.getHeight(), true);
    if (glossImage_.isValid()) {
        juce::Graphics glossG(topGlossLayer_);
        const int sourceHeight = juce::jlimit(1, glossImage_.getHeight(),
            juce::roundToInt((float)glossImage_.getHeight() * 0.56f));
        glossG.drawImage(glossImage_,
                         juce::roundToInt(centre.x - r),
                         juce::roundToInt(centre.y - r),
                         juce::roundToInt(r * 2.0f),
                         juce::roundToInt(r * 1.08f),
                         0, 0, glossImage_.getWidth(), sourceHeight, false);
    }

    frontG.setColour(juce::Colours::white.withAlpha(0.12f * masterAlpha));
    frontG.drawEllipse(centre.x - r + 1.8f, centre.y - r + 1.8f,
                       r * 2.0f - 3.6f, r * 2.0f - 3.6f, 0.9f);
}


void OrbVisualizer::seedAmbientBubble(Bubble& b, bool anywhere)
{
    b.x = (rng_.nextFloat() * 2.0f - 1.0f) * 0.60f;
    b.y = anywhere ? (0.62f + rng_.nextFloat() * 0.24f)
                   : (0.70f + rng_.nextFloat() * 0.18f);
    b.r = 0.010f + rng_.nextFloat() * 0.015f;
    b.rise = 0.0026f + rng_.nextFloat() * 0.0034f;
    b.swayAmp = 0.0011f + rng_.nextFloat() * 0.0022f;
    b.swayRate = 0.45f + rng_.nextFloat() * 0.75f;
    b.swayPhase = rng_.nextFloat() * kTwoPi;
    b.depth = 0.30f + rng_.nextFloat() * 0.70f;
    b.opacity = 0.17f + rng_.nextFloat() * 0.15f;
    b.life = 0.78f + rng_.nextFloat() * 0.22f;

    const float maxY = std::sqrt(juce::jmax(0.0f, 1.0f - b.x * b.x)) * 0.94f;
    b.y = juce::jmin(b.y, maxY);
}

void OrbVisualizer::emitBubble(float xNorm, float strength, bool micro)
{
    strength = juce::jlimit(0.0f, 1.0f, strength);
    Bubble* slot = nullptr;
    for (auto& b : bubbles_) {
        if (b.life <= 0.01f) {
            slot = &b;
            break;
        }
    }
    if (slot == nullptr)
        slot = &bubbles_[(size_t)(bubbleWrite_++ % kBubbles)];

    auto& b = *slot;
    const float originX = juce::jlimit(-0.82f, 0.82f, xNorm * 2.0f - 1.0f);
    const float jitter = (rng_.nextFloat() * 2.0f - 1.0f) * (micro ? 0.055f : 0.085f);
    b.x = juce::jlimit(-0.88f, 0.88f, originX + jitter);
    b.y = micro ? (0.60f + rng_.nextFloat() * 0.18f)
                : (0.72f + rng_.nextFloat() * 0.14f);
    b.r = micro ? (0.0062f + strength * 0.0036f)
                : (0.0110f + strength * 0.0085f);
    b.rise = micro ? (0.0050f + strength * 0.0030f)
                   : (0.0031f + strength * 0.0035f);
    b.swayAmp = micro ? 0.0013f : 0.0020f;
    b.swayRate = micro ? 1.20f : 0.72f;
    b.swayPhase = rng_.nextFloat() * kTwoPi;
    b.depth = micro ? 0.42f : (0.52f + rng_.nextFloat() * 0.38f);
    b.opacity = micro ? (0.19f + strength * 0.12f)
                      : (0.25f + strength * 0.16f);
    b.life = 0.78f + strength * 0.22f;
}

void OrbVisualizer::advanceBubbles(bool active)
{
    for (auto& b : bubbles_) {
        if (b.life <= 0.01f) {
            b.life = 0.0f;
            continue;
        }

        if (!active) {
            b.life *= 0.76f;
            continue;
        }

        const float drive = 0.30f + bassFlow_ * 0.44f
                          + lowEnergy_ * 0.18f + bodyEnergy_ * 0.10f;
        b.y -= b.rise * drive;
        b.x += b.swayAmp * std::sin(phase_ * b.swayRate + b.swayPhase) * drive;
        b.life *= 0.9972f;

        const float lim = std::sqrt(juce::jmax(0.0f, 1.0f - b.y * b.y)) * 0.94f;
        b.x = juce::jlimit(-lim, lim, b.x);
        const float localSurface = surfaceY(b.x * 0.5f + 0.5f);
        const float blendLine = localSurface + 0.22f;
        const float vanishLine = localSurface + 0.10f;
        if (b.y < blendLine)
            b.life *= 0.80f;
        if (b.y < vanishLine)
            b.life = 0.0f;
    }
}

void OrbVisualizer::emitDroplets(float xNorm, float strength, int count,
                                 bool wide, float brightness)
{
    // A tiny asymmetric fan reads more like liquid than a symmetrical particle
    // burst. The fixed patterns avoid random spray jitter while still keeping
    // consecutive splashes visually varied.
    constexpr std::array<float, 8> spread{{
        -0.88f, 0.62f, -0.36f, 0.92f, 0.20f, -0.66f, 0.48f, -0.14f
    }};
    constexpr std::array<float, 8> lift{{
        1.00f, 0.82f, 0.72f, 0.90f, 0.64f, 0.78f, 0.68f, 0.86f
    }};

    strength = juce::jlimit(0.0f, 1.0f, strength);
    count = juce::jlimit(0, 3, count);
    const float originX = juce::jlimit(-0.86f, 0.86f, xNorm * 2.0f - 1.0f);
    const float spreadScale = wide ? 0.072f : 0.040f;

    for (int i = 0; i < count; ++i) {
        auto& d = droplets_[(size_t)(dropletWrite_++ % kDroplets)];
        const size_t pattern = (size_t)(dropletWrite_ + i * 3) % spread.size();
        d.x = juce::jlimit(-0.92f, 0.92f,
            originX + spread[pattern] * spreadScale * (0.65f + strength * 0.55f));
        const float dropletXNorm = d.x * 0.5f + 0.5f;
        d.y = surfaceY(dropletXNorm) - 0.008f;
        d.vx = spread[pattern] * (0.0025f + strength * (wide ? 0.012f : 0.007f));
        d.vy = -(0.032f + strength * 0.062f) * lift[pattern];
        d.life = 0.76f + strength * 0.24f;
        d.size = 0.72f + strength * 1.35f * (0.76f + 0.24f * lift[pattern]);
        d.brightness = juce::jlimit(0.0f, 1.0f, brightness);
    }
}

void OrbVisualizer::advanceDroplets(bool active)
{
    for (auto& d : droplets_) {
        if (d.life <= 0.001f) {
            d.life = 0.0f;
            continue;
        }

        if (!active) {
            d.life *= 0.56f;
            continue;
        }

        d.vy += 0.0088f;
        d.vx *= 0.988f;
        d.x += d.vx;
        d.y += d.vy;
        d.life *= 0.885f;

        const float radialSq = d.x * d.x + d.y * d.y;
        const float xNorm = juce::jlimit(0.02f, 0.98f, d.x * 0.5f + 0.5f);
        const bool reentered = d.vy > 0.0f
            && d.y >= surfaceY(xNorm) - 0.002f;
        if (reentered) {
            // A returning bead makes a tiny downward dimple, then the spring
            // lattice naturally rebounds. Only bass-led droplets reach here,
            // so this detail cannot make vocals or cymbals look like bass.
            const float reentry = 0.00055f + d.life * 0.00105f;
            injectBallisticPulse(xNorm, 0.036f + d.size * 0.003f,
                                 -reentry, 0.0f);
            if (d.life > 0.38f)
                emitBubble(xNorm, d.life * 0.34f, true);
            d.life = 0.0f;
        } else if (radialSq > 0.98f) {
            d.life = 0.0f;
        }
    }
}

float OrbVisualizer::surfaceY(float xNorm) const
{
    xNorm = juce::jlimit(0.0f, 1.0f, xNorm);

    // True visual noise/bypass gate. No hidden oscillator is allowed to keep
    // moving a silent or bypassed liquid surface.
    const float motion = bypassed_ ? 0.0f : activity_;
    if (motion < 0.015f)
        return kRestLevel;

    const float activity = motion * motion * (3.0f - 2.0f * motion);
    const float centred = xNorm * 2.0f - 1.0f;
    // Interpolate the simulated water lattice. There is deliberately no
    // oscillator or pre-shaped frequency trace here: every crest must have
    // arrived from a real musical onset and then travelled through the fluid.
    const float nodePosition = xNorm * (float)(kFluidNodes - 1);
    const int node0 = juce::jlimit(0, kFluidNodes - 1, (int)nodePosition);
    const int node1 = juce::jmin(kFluidNodes - 1, node0 + 1);
    const float nodeMix = nodePosition - (float)node0;
    const float wave = fluidHeight_[(size_t)node0]
        + (fluidHeight_[(size_t)node1] - fluidHeight_[(size_t)node0]) * nodeMix;

    // The visible surface comes only from the propagated spring lattice. A
    // tiny antisymmetric slosh is the vessel's inertia; there are no synthetic
    // crowns, EQ-shaped hills or ballistic level offsets layered on top.
    const float vesselSlosh = slosh_ * centred * 0.42f;
    return juce::jlimit(-0.52f, 0.52f,
        kRestLevel + activity * (wave * 1.34f + vesselSlosh));
}

void OrbVisualizer::injectBallisticPulse(float xNorm, float radius,
                                         float strength, float direction)
{
    xNorm = juce::jlimit(0.04f, 0.96f, xNorm);
    radius = juce::jmax(0.035f, radius);
    // Positive strength launches a crest; negative strength is the
    // delayed counter-impulse that pulls the same crest down quickly.
    strength = juce::jlimit(-0.46f, 0.46f, strength);
    direction = juce::jlimit(-1.0f, 1.0f, direction);
    const float impactX = juce::jlimit(0.04f, 0.96f,
        xNorm + direction * radius * 0.16f);

    for (int i = 0; i < kFluidNodes; ++i) {
        const float x = (float)i / (float)(kFluidNodes - 1);
        const float distance = std::abs(x - impactX) / radius;
        if (distance >= 1.0f)
            continue;

        // A Gaussian pressure footprint has a broad base but one naturally
        // rounded peak. Unlike the old high-gain cosine cap, it does not hold
        // many neighbouring nodes at the limiter and form a square plateau.
        const float core = std::exp(-4.5f * distance * distance);
        fluidVelocity_[(size_t)i] -= strength * core;
    }
}

float OrbVisualizer::strikeQuietRegion(float preferredX, float strength,
                                       float radius, float stereoPan,
                                       bool avoidCentre)
{
    // Full-width deterministic lanes. The live lattice decides which lane is
    // currently least occupied; ordering is only a stable tie-break, never an
    // animation oscillator or random impact generator.
    // No lane sits exactly at 0.50. Mono bass therefore remains varied, while
    // propagated waves are still free to cross and meet at the centre.
    constexpr std::array<float, 12> lanes{{
        0.20f, 0.72f, 0.34f, 0.84f, 0.58f, 0.14f,
        0.66f, 0.28f, 0.78f, 0.42f, 0.86f, 0.24f
    }};
    constexpr std::array<float, 8> strengthScale{{
        1.00f, 0.86f, 1.08f, 0.92f, 1.04f, 0.82f, 1.10f, 0.95f
    }};
    constexpr std::array<float, 8> radiusScale{{
        0.92f, 1.12f, 0.84f, 1.04f, 0.96f, 1.18f, 0.88f, 1.08f
    }};

    preferredX = juce::jlimit(0.10f, 0.90f, preferredX);
    stereoPan = juce::jlimit(0.0f, 1.0f, stereoPan);
    const float stereoConfidence = std::abs(stereoPan - 0.5f) * 2.0f;
    const int laneStart = impactSequence_ % (int)lanes.size();
    float bestX = lanes[(size_t)laneStart];
    float bestScore = std::numeric_limits<float>::max();

    for (int rank = 0; rank < (int)lanes.size(); ++rank) {
        const float candidate = lanes[(size_t)((laneStart + rank) % (int)lanes.size())];
        const int centreNode = juce::jlimit(0, kFluidNodes - 1,
            (int)std::round(candidate * (float)(kFluidNodes - 1)));
        float occupancy = 0.0f;
        float weightSum = 0.0f;
        for (int offset = -2; offset <= 2; ++offset) {
            const int node = juce::jlimit(0, kFluidNodes - 1, centreNode + offset);
            const float weight = 3.0f - (float)std::abs(offset) * 0.55f;
            const float height = fluidHeight_[(size_t)node];
            occupancy += weight * (std::abs(height) * 0.80f
                + juce::jmax(0.0f, -height) * 1.40f
                + std::abs(fluidVelocity_[(size_t)node]) * 0.35f);
            weightSum += weight;
        }
        occupancy /= weightSum;

        const float reuseDistance = std::abs(candidate - lastImpactX_);
        const float reuseWindow = avoidCentre ? 0.28f : 0.24f;
        const float reusePenalty = juce::jmax(0.0f, reuseWindow - reuseDistance)
            * (avoidCentre ? 0.62f : 0.18f);
        // For bass, stereo is deliberately only a bias. Mono and near-mono
        // material must not drag every crest back to the middle.
        const float stereoPenalty = std::abs(candidate - preferredX)
            * (avoidCentre ? (0.012f + stereoConfidence * 0.095f)
                           : (0.028f + stereoConfidence * 0.18f));
        const bool oppositeSide = (stereoPan < 0.38f && candidate > 0.55f)
                               || (stereoPan > 0.62f && candidate < 0.45f);
        const float sidePenalty = oppositeSide
            ? stereoConfidence * (avoidCentre ? 0.030f : 0.055f) : 0.0f;
        const float centreDistance = std::abs(candidate - 0.5f);
        const float centrePenalty = avoidCentre
            ? juce::jmax(0.0f, 0.17f - centreDistance) * 0.48f : 0.0f;
        const float score = occupancy * (avoidCentre ? 2.45f : 2.80f)
                          + reusePenalty + stereoPenalty + sidePenalty
                          + centrePenalty + (float)rank * 0.00045f;
        if (score < bestScore) {
            bestScore = score;
            bestX = candidate;
        }
    }

    float finalX = bestX;
    if (avoidCentre) {
        const float stereoWeight = 0.08f + stereoConfidence * 0.32f;
        finalX += (stereoPan - finalX) * stereoWeight;

        // Preserve a small dead zone around the exact centre for mono bass.
        // This prevents a visually repetitive centre spike without forcing
        // hard left/right motion on genuinely stereo low-frequency content.
        if (stereoConfidence < 0.22f && std::abs(finalX - 0.5f) < 0.075f)
            finalX = 0.5f + (bestX < 0.5f ? -0.075f : 0.075f);
    }

    const size_t shape = (size_t)(impactSequence_ & 7);
    ++impactSequence_;
    injectBallisticPulse(finalX, radius * radiusScale[shape],
                         strength * strengthScale[shape],
                         (stereoPan - 0.5f) * 2.0f);
    lastImpactX_ = finalX;
    return finalX;
}

void OrbVisualizer::advanceFluid(bool active)
{
    if (!active) {
        float remaining = 0.0f;
        for (int i = 0; i < kFluidNodes; ++i) {
            fluidHeight_[(size_t)i] *= 0.72f;
            fluidVelocity_[(size_t)i] *= 0.58f;
            remaining = juce::jmax(remaining, std::abs(fluidHeight_[(size_t)i]));
        }
        sloshVelocity_ *= 0.54f;
        slosh_ *= 0.72f;
        if (remaining < 0.0003f) {
            fluidHeight_.fill(0.0f);
            fluidVelocity_.fill(0.0f);
            slosh_ = sloshVelocity_ = 0.0f;
        }
        return;
    }

    // Three tiny fixed substeps at the ~24 Hz editor rate. Fresh low/percussion
    // motion gets enough persistence to travel, then damping rises sharply so
    // repeated impacts cannot pump a permanent standing-wave mountain.
    const float propagationDamping = framesSincePercussion_ <= 2 ? 0.965f
        : (framesSincePercussion_ <= 5 ? 0.900f : 0.820f);
    const float baselineReturn = framesSincePercussion_ <= 2 ? 0.008f
        : (framesSincePercussion_ <= 5 ? 0.022f : 0.040f);
    for (int substep = 0; substep < 3; ++substep) {
        std::array<float, kFluidNodes> nextVelocity{};
        std::array<float, kFluidNodes> nextHeight{};
        float meanHeight = 0.0f;
        for (int i = 0; i < kFluidNodes; ++i) {
            const float left = fluidHeight_[(size_t)(i > 0 ? i - 1 : 1)];
            const float right = fluidHeight_[(size_t)(i + 1 < kFluidNodes ? i + 1
                                                                          : kFluidNodes - 2)];
            const float velocityLeft = fluidVelocity_[(size_t)(i > 0 ? i - 1 : 1)];
            const float velocityRight = fluidVelocity_[(size_t)(i + 1 < kFluidNodes ? i + 1
                                                                                     : kFluidNodes - 2)];
            const float height = fluidHeight_[(size_t)i];
            const float laplacian = left + right - 2.0f * height;
            const float velocityLaplacian = velocityLeft + velocityRight
                                          - 2.0f * fluidVelocity_[(size_t)i];
            const float acceleration = laplacian * 0.205f
                                     + velocityLaplacian * 0.085f
                                     - height * baselineReturn;
            nextVelocity[(size_t)i] = juce::jlimit(-0.460f, 0.460f,
                (fluidVelocity_[(size_t)i] + acceleration) * propagationDamping);
            meanHeight += height + nextVelocity[(size_t)i] * 0.65f;
        }

        meanHeight /= (float)kFluidNodes;
        float clippedMean = 0.0f;
        for (int i = 0; i < kFluidNodes; ++i) {
            fluidVelocity_[(size_t)i] = nextVelocity[(size_t)i];
            const float predicted = fluidHeight_[(size_t)i]
                                  + nextVelocity[(size_t)i] * 0.65f - meanHeight;
            // Soft saturation rounds strong low/percussion crests instead of
            // producing the flat plateaus caused by a hard per-node clamp.
            nextHeight[(size_t)i] = 0.400f * std::tanh(predicted / 0.400f);
            clippedMean += nextHeight[(size_t)i];
        }
        clippedMean /= (float)kFluidNodes;
        for (int i = 0; i < kFluidNodes; ++i) {
            // Soft clipping is nonlinear, so remove its tiny residual bias as
            // a second pass and preserve exactly half-filled visual volume.
            fluidHeight_[(size_t)i] = nextHeight[(size_t)i] - clippedMean;
        }
    }

    sloshVelocity_ = (sloshVelocity_ - slosh_ * 0.060f) * 0.915f;
    slosh_ = juce::jlimit(-0.055f, 0.055f, slosh_ + sloshVelocity_);
}

void OrbVisualizer::refreshSilentFrame(bool bypassChanged)
{
    // Preserve the same release character as the full analyzer path, but skip
    // all crossover/filter/envelope work while the output is below the visual
    // noise floor. The liquid and particles still receive a few cheap decay
    // frames so they settle gracefully instead of freezing mid-motion.
    level_ += (0.0f - level_) * 0.12f;
    activity_ += (0.0f - activity_) * 0.18f;
    lowEnergy_ += (0.0f - lowEnergy_) * 0.13f;
    bodyEnergy_ += (0.0f - bodyEnergy_) * 0.10f;
    airEnergy_ += (0.0f - airEnergy_) * 0.18f;
    topPulse_ += (0.0f - topPulse_) * 0.18f;
    bassFlow_ += (0.0f - bassFlow_) * 0.075f;
    bassSpectrumFast_ *= 0.36f;
    bassSpectrumSlow_ *= 0.90f;

    bassReturnCountdown_ = 0;
    bassReturnStrength_ = 0.0f;
    bassReboundCountdown_ = 0;
    bassReboundStrength_ = 0.0f;
    lowImpactCooldown_ = juce::jmax(0, lowImpactCooldown_ - 1);
    percussionImpactCooldown_ = juce::jmax(0, percussionImpactCooldown_ - 1);
    bodyImpactCooldown_ = juce::jmax(0, bodyImpactCooldown_ - 1);
    airImpactCooldown_ = juce::jmax(0, airImpactCooldown_ - 1);
    framesSincePercussion_ = juce::jmin(1000, framesSincePercussion_ + 1);

    advanceFluid(false);
    ambientBubbleCountdown_ = 0;
    advanceBubbles(false);
    advanceDroplets(false);

    bool fluidMoving = activity_ > 0.006f;
    for (int i = 0; i < kFluidNodes && !fluidMoving; ++i) {
        fluidMoving = std::abs(fluidHeight_[(size_t)i]) > 0.00025f
            || std::abs(fluidVelocity_[(size_t)i]) > 0.00025f;
    }
    bool particlesMoving = false;
    for (const auto& b : bubbles_)
        particlesMoving = particlesMoving || b.life > 0.01f;
    for (const auto& d : droplets_)
        particlesMoving = particlesMoving || d.life > 0.01f;

    if (bypassChanged || fluidMoving || particlesMoving)
        repaint();
}

void OrbVisualizer::refresh(bool bypassed)
{
    const bool bypassChanged = bypassed_ != bypassed;
    bypassed_ = bypassed;
    if (bypassChanged)
        rebuildStaticLayer();
    phase_ += 0.020f + bodyEnergy_ * 0.014f + airEnergy_ * 0.008f;
    if (phase_ > 512.0f * kTwoPi)
        phase_ -= 512.0f * kTwoPi;

    if (!seeded_) {
        seeded_ = true;
        for (int i = 0; i < 3; ++i)
            seedAmbientBubble(bubbles_[(size_t)i], false);
        ambientBubbleCountdown_ = 10;
    }

    // ScopeTap retains only the newest requested tail and advances its read
    // cursor over the complete backlog. Requesting 4096 here avoids the old
    // 8192-sample copy followed by discarding half of the copied data.
    const int capturedSamples = processor_.scopeTap.consumeLatest(
        timeLeft_.data(), timeRight_.data(), kAnalysisSamples);
    const int analysisSamples = capturedSamples;
    constexpr int analysisStart = 0;

    const float sr = (float)(processor_.getSampleRate() > 1000.0
        ? processor_.getSampleRate() : 48000.0);
    if (analysisSampleRate_ != sr) {
        analysisPoleALeft_.fill(0.0f);
        analysisPoleBLeft_.fill(0.0f);
        analysisPoleARight_.fill(0.0f);
        analysisPoleBRight_.fill(0.0f);
        analysisFastEnvelope_.fill(0.0f);
        analysisSlowEnvelope_.fill(0.0f);
        analysisSampleRate_ = sr;
        analysisPrimed_ = false;
    }

    // The processor's per-block peak is an atomic, allocation-free early gate.
    // Require several consecutive quiet UI frames so a short gap between notes
    // cannot put the analyzer to sleep. A separate no-capture timeout covers a
    // stopped/paused host whose last meter value may remain stale.
    const float outputPeakDb = processor_.meters.outputPeakDb.load(
        std::memory_order_relaxed);
    // At most 128 stereo probes guard the wake-up edge. This closes the tiny
    // race where ScopeTap may publish a new block just before its meter atomics
    // are updated, without paying for a full RMS or crossover pass.
    float capturedProbePeak = 0.0f;
    if (capturedSamples > 0) {
        const int probeStride = juce::jmax(1, capturedSamples / 128);
        for (int i = 0; i < capturedSamples; i += probeStride) {
            capturedProbePeak = juce::jmax(capturedProbePeak,
                juce::jmax(std::abs(timeLeft_[(size_t)i]),
                           std::abs(timeRight_[(size_t)i])));
        }
    }
    const bool capturedSignalHint = capturedProbePeak >= 0.00030f;
    const bool belowVisualFloor = outputPeakDb <= -78.0f
        && !capturedSignalHint;
    silentMeterFrames_ = belowVisualFloor
        ? juce::jmin(1000, silentMeterFrames_ + 1) : 0;
    emptyCaptureFrames_ = capturedSamples == 0
        ? juce::jmin(1000, emptyCaptureFrames_ + 1) : 0;
    const bool useSilentFastPath = silentMeterFrames_ >= 3
        || emptyCaptureFrames_ >= 8;

    if (useSilentFastPath) {
        if (!analysisSleeping_) {
            // Clear filter history once on entry, but leave the analyzer primed.
            // The first real sample after silence can therefore create a valid
            // transient instead of being swallowed by a warm-up frame.
            analysisPoleALeft_.fill(0.0f);
            analysisPoleBLeft_.fill(0.0f);
            analysisPoleARight_.fill(0.0f);
            analysisPoleBRight_.fill(0.0f);
            analysisFastEnvelope_.fill(0.0f);
            analysisSlowEnvelope_.fill(0.0f);
            previousBandTarget_.fill(0.0f);
            bassSpectrumFast_ = 0.0f;
            bassSpectrumSlow_ = 0.0f;
            analysisPrimed_ = true;
        }
        analysisSleeping_ = true;
        refreshSilentFrame(bypassChanged);
        return;
    }
    analysisSleeping_ = false;

    // Five lightweight two-stage one-pole crossovers form six broad OBS-style
    // visual bands. They reveal sub, bass, body, presence, attack and cymbal air
    // without kick classification, tempo prediction or measurement-grade claims.
    const auto coefficient = [sr](float hz) {
        return 1.0f - std::exp(-kTwoPi * hz / sr);
    };
    constexpr std::array<float, kVisualBands - 1> cutoffs{{
        90.0f, 240.0f, 850.0f, 2400.0f, 6000.0f
    }};
    std::array<float, kVisualBands - 1> coefficients{};
    for (size_t i = 0; i < coefficients.size(); ++i)
        coefficients[i] = coefficient(cutoffs[i]);

    std::array<double, kVisualBands> bandPowerLeft{};
    std::array<double, kVisualBands> bandPowerRight{};
    std::array<float, kVisualBands> bandSampleLeft{};
    std::array<float, kVisualBands> bandSampleRight{};
    std::array<float, kVisualBands> transientPeak{};
    std::array<float, kVisualBands> transientPan{};
    transientPan.fill(0.5f);
    double fullPower = 0.0;
    const bool captureTransients = analysisPrimed_ && analysisSamples > 0;
    const int warmupSamples = analysisPrimed_ ? 0
        : juce::jmin(256, juce::jmax(0, analysisSamples - 1));
    const int measuredSamples = juce::jmax(1, analysisSamples - warmupSamples);
    const float fastAttack = 1.0f - std::exp(-1.0f / (0.0022f * sr));
    const float fastRelease = 1.0f - std::exp(-1.0f / (0.022f * sr));
    const float slowFollow = 1.0f - std::exp(-1.0f / (0.055f * sr));

    for (int sample = 0; sample < analysisSamples; ++sample) {
        const int i = analysisStart + sample;
        const float left = timeLeft_[(size_t)i];
        const float right = timeRight_[(size_t)i];
        for (size_t pole = 0; pole < analysisPoleALeft_.size(); ++pole) {
            analysisPoleALeft_[pole] += coefficients[pole]
                * (left - analysisPoleALeft_[pole]);
            analysisPoleBLeft_[pole] += coefficients[pole]
                * (analysisPoleALeft_[pole] - analysisPoleBLeft_[pole]);
            analysisPoleARight_[pole] += coefficients[pole]
                * (right - analysisPoleARight_[pole]);
            analysisPoleBRight_[pole] += coefficients[pole]
                * (analysisPoleARight_[pole] - analysisPoleBRight_[pole]);
        }

        bandSampleLeft[0] = analysisPoleBLeft_[0];
        bandSampleRight[0] = analysisPoleBRight_[0];
        for (int band = 1; band < kVisualBands - 1; ++band) {
            bandSampleLeft[(size_t)band] = analysisPoleBLeft_[(size_t)band]
                - analysisPoleBLeft_[(size_t)band - 1];
            bandSampleRight[(size_t)band] = analysisPoleBRight_[(size_t)band]
                - analysisPoleBRight_[(size_t)band - 1];
        }
        bandSampleLeft[kVisualBands - 1] = left - analysisPoleBLeft_.back();
        bandSampleRight[kVisualBands - 1] = right - analysisPoleBRight_.back();

        for (int band = 0; band < kVisualBands; ++band) {
            const float leftMagnitude = std::abs(bandSampleLeft[(size_t)band]);
            const float rightMagnitude = std::abs(bandSampleRight[(size_t)band]);
            const float magnitude = 0.5f * (leftMagnitude + rightMagnitude);
            if (!analysisPrimed_ && sample == 0) {
                analysisFastEnvelope_[(size_t)band] = magnitude;
                analysisSlowEnvelope_[(size_t)band] = magnitude;
            }
            const float fastRate = magnitude > analysisFastEnvelope_[(size_t)band]
                ? fastAttack : fastRelease;
            analysisFastEnvelope_[(size_t)band] += fastRate
                * (magnitude - analysisFastEnvelope_[(size_t)band]);
            analysisSlowEnvelope_[(size_t)band] += slowFollow
                * (magnitude - analysisSlowEnvelope_[(size_t)band]);

            if (sample >= warmupSamples) {
                bandPowerLeft[(size_t)band] += (double)bandSampleLeft[(size_t)band]
                    * bandSampleLeft[(size_t)band];
                bandPowerRight[(size_t)band] += (double)bandSampleRight[(size_t)band]
                    * bandSampleRight[(size_t)band];
                const float onset = captureTransients ? juce::jmax(0.0f,
                    analysisFastEnvelope_[(size_t)band]
                        - analysisSlowEnvelope_[(size_t)band] * 1.04f) : 0.0f;
                if (onset > transientPeak[(size_t)band]) {
                    transientPeak[(size_t)band] = onset;
                    transientPan[(size_t)band] = rightMagnitude
                        / juce::jmax(0.000001f, leftMagnitude + rightMagnitude);
                }
            }
        }

        if (sample < warmupSamples)
            continue;
        fullPower += 0.5 * ((double)left * left + (double)right * right);
    }
    if (analysisSamples > warmupSamples)
        analysisPrimed_ = true;

    const float rms = std::sqrt((float)(fullPower / measuredSamples));
    // Ignore the numerical/noise-floor residue visible around -75 dBFS in hosts.
    const bool signalPresent = rms >= 0.00022f;
    const float targetLevel = signalPresent
        ? juce::jlimit(0.0f, 1.0f, rms * 2.8f)
        : 0.0f;

    const auto compress = [signalPresent](float value, float drive) {
        return signalPresent ? 1.0f - std::exp(-value * drive) : 0.0f;
    };
    constexpr std::array<float, kVisualBands> bandDrive{{
        18.0f, 16.0f, 12.0f, 10.0f, 15.0f, 24.0f
    }};
    std::array<float, kVisualBands> bandTarget{};
    std::array<float, kVisualBands> bandPan{};
    for (int band = 0; band < kVisualBands; ++band) {
        const float leftRms = std::sqrt((float)(bandPowerLeft[(size_t)band]
                                                / measuredSamples));
        const float rightRms = std::sqrt((float)(bandPowerRight[(size_t)band]
                                                 / measuredSamples));
        const float bandRms = std::sqrt(0.5f * (leftRms * leftRms
                                                + rightRms * rightRms));
        bandTarget[(size_t)band] = compress(bandRms, bandDrive[(size_t)band]);
        bandPan[(size_t)band] = rightRms
            / juce::jmax(0.000001f, leftRms + rightRms);
    }
    const float lowTarget = juce::jmax(bandTarget[0], bandTarget[1]);
    const float bodyTarget = juce::jmax(bandTarget[2], bandTarget[3]);
    const float airTarget = juce::jmax(bandTarget[4], bandTarget[5]);

    // These inexpensive time-domain rises only drive submerged decorative
    // detail and stereo placement. They never classify a surface impact.
    std::array<float, kVisualBands> bandRise{};
    std::array<float, kVisualBands> bandTransient{};
    constexpr std::array<float, kVisualBands> transientDrive{{
        46.0f, 42.0f, 34.0f, 36.0f, 52.0f, 68.0f
    }};
    for (int band = 0; band < kVisualBands; ++band) {
        bandRise[(size_t)band] = juce::jmax(0.0f,
            bandTarget[(size_t)band] - previousBandTarget_[(size_t)band]);
        const float intraFrame = compress(transientPeak[(size_t)band],
                                          transientDrive[(size_t)band]);
        const float frameRise = captureTransients
            ? juce::jlimit(0.0f, 1.0f, bandRise[(size_t)band] * 1.45f)
            : 0.0f;
        bandTransient[(size_t)band] = juce::jmax(intraFrame, frameRise);
    }
    const float subTransient = bandTransient[0];
    const float bassTransient = bandTransient[1];
    const float bodyTransient = juce::jmax(bandTransient[2], bandTransient[3]);
    const float stickTransient = bandTransient[4];

    // A transient's x coordinate comes from actual channel energy, never from
    // sample time. Intra-frame panning wins for a fresh onset; steady band pan
    // is the fallback for frame-rise detections and sustained material.
    std::array<float, kVisualBands> eventPan{};
    for (int band = 0; band < kVisualBands; ++band) {
        eventPan[(size_t)band] = transientPeak[(size_t)band] > 0.000002f
            ? transientPan[(size_t)band] : bandPan[(size_t)band];
    }
    const auto blendPan = [](float amountA, float panA, float amountB,
                             float panB, float fallback) {
        const float amount = amountA + amountB;
        return amount > 0.000001f
            ? juce::jlimit(0.0f, 1.0f, (amountA * panA + amountB * panB) / amount)
            : fallback;
    };
    const float steadyLowPan = blendPan(bandTarget[0], bandPan[0],
                                        bandTarget[1], bandPan[1], 0.5f);
    const float lowImpactPan = blendPan(subTransient, eventPan[0],
                                        bassTransient, eventPan[1], steadyLowPan);
    // Generic low-frequency magnitude, equivalent in intent to the low bars of
    // a common OBS visualizer. The 0-90 Hz band dominates; a small 90-240 Hz
    // contribution keeps bass guitar and kendang body visible without turning
    // this into a drum classifier.
    const float rawBassSpectrum = !bypassed_ && signalPresent
        ? juce::jlimit(0.0f, 1.0f,
            bandTarget[0] * 0.84f + bandTarget[1] * 0.16f)
        : 0.0f;

    level_ += (targetLevel - level_) * (targetLevel > level_ ? 0.30f : 0.12f);
    const float targetActivity = signalPresent
        ? juce::jlimit(0.0f, 1.0f, 0.07f + targetLevel * 4.2f)
        : 0.0f;
    activity_ += (targetActivity - activity_) * (targetActivity > activity_ ? 0.34f : 0.18f);

    auto follow = [](float current, float target, float attack, float release) {
        return current + (target - current) * (target > current ? attack : release);
    };
    lowEnergy_ = follow(lowEnergy_, lowTarget, 0.46f, 0.13f);
    bodyEnergy_ = follow(bodyEnergy_, bodyTarget, 0.34f, 0.10f);
    airEnergy_ = follow(airEnergy_, airTarget, 0.52f, 0.18f);
    bassFlow_ = follow(bassFlow_,
        juce::jlimit(0.0f, 1.0f, bandTarget[0] * 0.62f + bandTarget[1] * 0.78f),
        0.28f, 0.075f);
    const float previousBassFast = bassSpectrumFast_;
    bassSpectrumFast_ = follow(bassSpectrumFast_, rawBassSpectrum, 0.92f, 0.64f);
    bassSpectrumSlow_ = follow(bassSpectrumSlow_, rawBassSpectrum, 0.060f, 0.030f);
    const float bassRise = juce::jmax(0.0f,
        bassSpectrumFast_ - previousBassFast);
    const float bassContrast = juce::jmax(0.0f,
        bassSpectrumFast_ - bassSpectrumSlow_ * 1.06f);
    // The rise term dominates so sustained bass does not repeatedly pump the
    // surface. Contrast only supports a fresh low-frequency swell.
    const float bassPunch = juce::jlimit(0.0f, 1.0f,
        bassRise * 2.65f + bassContrast * 1.05f);
    // Keep the magnitude history continuous through bypass so un-bypassing a
    // sustained note cannot manufacture a fresh low-frequency rise.
    previousBandTarget_ = bandTarget;

    // Bass is the dominant visual contract. Other families remain smaller and
    // less frequent so speech does not resemble a low-frequency impact.
    lowImpactCooldown_ = juce::jmax(0, lowImpactCooldown_ - 1);
    percussionImpactCooldown_ = juce::jmax(0, percussionImpactCooldown_ - 1);
    bodyImpactCooldown_ = juce::jmax(0, bodyImpactCooldown_ - 1);
    airImpactCooldown_ = juce::jmax(0, airImpactCooldown_ - 1);

    // Three-stage surface-tension ballistic: a fast rounded crest, a short
    // counter-impulse, then one restrained rebound. This produces the satisfying
    // liquid snap without holding a mountain on sustained bass.
    if (bypassed_ || !signalPresent) {
        bassReturnCountdown_ = 0;
        bassReturnStrength_ = 0.0f;
        bassReboundCountdown_ = 0;
        bassReboundStrength_ = 0.0f;
    } else {
        if (bassReturnCountdown_ > 0) {
            --bassReturnCountdown_;
            if (bassReturnCountdown_ == 0 && bassReturnStrength_ > 0.0f) {
                injectBallisticPulse(bassReturnX_, bassReturnRadius_,
                                     -bassReturnStrength_, 0.0f);
                bassReturnStrength_ = 0.0f;
            }
        }
        if (bassReboundCountdown_ > 0) {
            --bassReboundCountdown_;
            if (bassReboundCountdown_ == 0 && bassReboundStrength_ > 0.0f) {
                injectBallisticPulse(bassReboundX_, bassReboundRadius_,
                                     bassReboundStrength_, 0.0f);
                bassReboundStrength_ = 0.0f;
            }
        }
    }

    const float lowAccent = juce::jlimit(0.0f, 1.0f,
        bassPunch * 0.88f + rawBassSpectrum * 0.12f);
    const float upperAttack = juce::jmax(stickTransient, bandTransient[5]);
    const float percussionAccent = juce::jlimit(0.0f, 1.0f,
        std::sqrt(juce::jmax(0.0f, bodyTransient * upperAttack)) * 0.92f
            + upperAttack * 0.20f + bodyTarget * 0.10f);
    const float bodyAccent = juce::jlimit(0.0f, 1.0f,
        bodyTransient * 0.78f + bodyTarget * 0.22f);
    const float topPulseTarget = signalPresent
        ? juce::jlimit(0.0f, 1.0f, bassPunch * 1.35f + lowAccent * 0.18f)
        : 0.0f;
    topPulse_ = follow(topPulse_, topPulseTarget, 0.56f, 0.14f);

    // Preserve an intentionally extreme hierarchy: low-frequency pressure owns
    // the liquid silhouette. Upper percussion and body material may only create
    // micro-ripples; their particles can remain readable without competing with
    // a bass crest. Steeper power curves suppress ordinary speech and dense
    // programme material before any surface impulse is considered.
    const float percussionVisual = std::pow(percussionAccent, 1.72f);
    const float bodyVisual = std::pow(bodyAccent, 2.35f);

    const float bodyImpactPan = blendPan(bodyTransient, eventPan[2],
        bandTransient[3], eventPan[3], 0.5f);
    const float upperImpactPan = blendPan(stickTransient, eventPan[4],
        bandTransient[5], eventPan[5], 0.5f);
    const float percussionImpactPan = blendPan(bodyTransient, bodyImpactPan,
        upperAttack, upperImpactPan, 0.5f);


    framesSincePercussion_ = juce::jmin(1000, framesSincePercussion_ + 1);
    if (!bypassed_ && signalPresent) {
        const float lowTransientEvidence = juce::jmax(subTransient, bassTransient);
        const bool lowEvent = lowImpactCooldown_ == 0
            && rawBassSpectrum >= 0.030f
            && bassPunch >= 0.050f
            && (bassRise >= 0.007f || lowTransientEvidence >= 0.075f);
        const bool percussionEvent = !lowEvent && percussionImpactCooldown_ == 0
            && percussionAccent >= 0.39f && upperAttack >= 0.25f;
        const bool bodyEvent = !lowEvent && !percussionEvent
            && bodyImpactCooldown_ == 0 && bodyAccent >= 0.44f
            && bodyTransient >= 0.27f
            && bodyTarget >= 0.018f;

        if (lowEvent) {
            // A narrow rounded crest rises quickly at a non-central lane. The
            // lattice, not an oversized footprint, supplies the natural width.
            const float shapedAccent = lowAccent * (1.0f - 0.28f * lowAccent);
            const float bassStrength = 0.130f + shapedAccent * 0.150f;
            const float bassRadius = 0.160f + shapedAccent * 0.055f;
            const float preferredX = 0.50f + (lowImpactPan - 0.5f) * 0.30f;
            const float x = strikeQuietRegion(preferredX, bassStrength,
                bassRadius, lowImpactPan, true);
            lowImpactCooldown_ = 2;
            percussionImpactCooldown_ = 1;
            framesSincePercussion_ = 0;

            // Counter-force creates the quick fall. A much smaller positive
            // pulse one frame later is the surface-tension rebound.
            bassReturnCountdown_ = 1;
            bassReturnX_ = x;
            bassReturnRadius_ = bassRadius * 0.82f;
            bassReturnStrength_ = bassStrength
                * (0.30f + shapedAccent * 0.08f);
            bassReboundCountdown_ = 2;
            bassReboundX_ = x;
            bassReboundRadius_ = bassRadius * 0.64f;
            bassReboundStrength_ = bassStrength
                * (0.065f + shapedAccent * 0.030f);

            sloshVelocity_ += (x - 0.5f)
                * (0.0015f + shapedAccent * 0.0035f);

            // Splash is deliberately sparse and bass-led. Weak low movement
            // gets no spray; strong impacts emit only one to three beads.
            const float splashAccent = std::pow(lowAccent, 1.35f);
            if (splashAccent >= 0.22f) {
                const int splashCount = splashAccent > 0.78f ? 3
                    : (splashAccent > 0.44f ? 2 : 1);
                emitDroplets(x, splashAccent, splashCount, true, 0.24f);
            }
            if (lowAccent >= 0.18f)
                emitBubble(x, 0.28f + lowAccent * 0.64f, false);
        } else if (percussionEvent) {
            const float preferredX = 0.50f
                + (percussionImpactPan - 0.5f) * 1.30f;
            // Upper percussion remains a very small local ripple only.
            strikeQuietRegion(preferredX,
                0.0045f + percussionVisual * 0.0125f,
                0.035f + percussionVisual * 0.006f,
                percussionImpactPan);
            percussionImpactCooldown_ = 4;
            framesSincePercussion_ = 0;
        } else if (bodyEvent) {
            const float preferredX = 0.50f + (bodyImpactPan - 0.5f) * 1.25f;
            // Vocal and instrument body is almost imperceptible as a wave.
            strikeQuietRegion(preferredX,
                0.0008f + bodyVisual * 0.0024f,
                0.035f + bodyVisual * 0.004f, bodyImpactPan);
            bodyImpactCooldown_ = 6;
        }

    }

    const bool visualActive = !bypassed_ && signalPresent;
    advanceFluid(visualActive && activity_ >= 0.012f);

    // Maintain only a quiet 2-3 bubble bed. Event bubbles temporarily raise the
    // visible count, but the fixed pool can never become a particle cloud.
    int activeBubbles = 0;
    for (const auto& b : bubbles_)
        activeBubbles += b.life > 0.01f ? 1 : 0;
    if (visualActive) {
        ambientBubbleCountdown_ = juce::jmax(0, ambientBubbleCountdown_ - 1);
        if (activeBubbles < 3 && ambientBubbleCountdown_ == 0) {
            for (auto& b : bubbles_) {
                if (b.life <= 0.01f) {
                    seedAmbientBubble(b, false);
                    break;
                }
            }
            ambientBubbleCountdown_ = 12 + (impactSequence_ % 10);
        }
    } else {
        ambientBubbleCountdown_ = 0;
    }

    advanceBubbles(visualActive);
    advanceDroplets(visualActive);

    bool fluidMoving = activity_ > 0.006f;
    for (int i = 0; i < kFluidNodes && !fluidMoving; ++i) {
        fluidMoving = std::abs(fluidHeight_[(size_t)i]) > 0.00025f
            || std::abs(fluidVelocity_[(size_t)i]) > 0.00025f;
    }
    bool particlesMoving = false;
    for (const auto& b : bubbles_)
        particlesMoving = particlesMoving || b.life > 0.01f;
    for (const auto& d : droplets_)
        particlesMoving = particlesMoving || d.life > 0.01f;

    // Silence eventually reaches zero paint cost even though the editor timer
    // continues serving meters and controls.
    if (bypassChanged || visualActive || fluidMoving || particlesMoving)
        repaint();
}

void OrbVisualizer::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float R = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;

    const auto hot  = bypassed_ ? juce::Colour(0xFF8A8593) : palette::violetHot;
    const auto mid  = bypassed_ ? juce::Colour(0xFF5F5A69) : palette::violet;
    const auto deep = bypassed_ ? juce::Colour(0xFF17151C) : palette::violetDeep;
    const float masterAlpha = bypassed_ ? 0.45f : 1.0f;
    const float motion = bypassed_ ? 0.0f : activity_;
    const float energy = bypassed_ ? 0.08f
                                   : juce::jlimit(0.10f, 1.0f, level_ * 0.85f + 0.12f);

    if (!staticLayer_.isValid()
        || staticLayer_.getWidth() != getWidth()
        || staticLayer_.getHeight() != getHeight())
        rebuildStaticLayer();
    if (staticLayer_.isValid())
        g.drawImageAt(staticLayer_, 0, 0);

    const float r = R - 6.5f;
    const auto toPx = [&](float nx, float ny) {
        return juce::Point<float>(centre.x + nx * r, centre.y + ny * r);
    };

    g.saveState();
    juce::Path clip;
    clip.addEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
    g.reduceClipRegion(clip);

    // No radial haze: it produced the unwanted round white patch in the
    // centre-left of the sphere and reduced liquid readability.

    // -- Liquid surfaces ------------------------------------------------------
    // Sample the surface sparsely, then render it as a quadratic-smoothed
    // curve: rounded ballistic fluid, never a jagged analysis trace. The final
    // trapezoidal mean is removed here as a last volume-conservation guard, so
    // the visible fill remains centred even after wave limiting and interpolation.
    std::array<juce::Point<float>, kSurfacePts + 1> front{}, back{};
    std::array<float, kSurfacePts + 1> sampledSurface{};
    float surfaceIntegral = 0.0f;
    for (int i = 0; i <= kSurfacePts; ++i) {
        const float xNorm = (float)i / (float)kSurfacePts;
        sampledSurface[(size_t)i] = surfaceY(xNorm);
        const float integrationWeight = (i == 0 || i == kSurfacePts) ? 0.5f : 1.0f;
        surfaceIntegral += sampledSurface[(size_t)i] * integrationWeight;
    }
    const float surfaceMean = surfaceIntegral / (float)kSurfacePts;

    for (int i = 0; i <= kSurfacePts; ++i) {
        const float xNorm = (float)i / (float)kSurfacePts;
        const float nx = xNorm * 2.0f - 1.0f;
        const float yF = sampledSurface[(size_t)i] - surfaceMean;
        front[(size_t)i] = toPx(nx * 0.995f, yF);
        back[(size_t)i] = toPx(nx * 0.995f,
            yF - 0.068f - 0.010f * motion
                * std::exp(-nx * nx * 2.8f));
    }

    // Catmull-Rom converted to cubic Beziers: the curve remains liquid-smooth
    // while following every simulated crest and trough without polygon edges.
    auto appendSmooth = [](juce::Path& path,
                           const std::array<juce::Point<float>, kSurfacePts + 1>& pts,
                           bool reversed, bool moveToFirst) {
        const int n = (int)pts.size();
        auto P = [&](int i) { return reversed ? pts[(size_t)(n - 1 - i)] : pts[(size_t)i]; };
        if (moveToFirst) path.startNewSubPath(P(0)); else path.lineTo(P(0));
        for (int i = 0; i < n - 1; ++i) {
            const auto p0 = P(juce::jmax(0, i - 1));
            const auto p1 = P(i);
            const auto p2 = P(i + 1);
            const auto p3 = P(juce::jmin(n - 1, i + 2));
            const auto c1 = p1 + (p2 - p0) * (1.0f / 6.0f);
            const auto c2 = p2 - (p3 - p1) * (1.0f / 6.0f);
            path.cubicTo(c1, c2, p2);
        }
    };

    // Back surface: the far rim of the liquid, slightly higher & calmer —
    // fakes perspective so the fluid reads as a 3-D volume.
    {
        juce::Path backBand;
        appendSmooth(backBand, back, false, true);
        appendSmooth(backBand, front, true, false);
        backBand.closeSubPath();

        juce::ColourGradient bg(hot.withAlpha(0.10f * masterAlpha),
                                centre.x, centre.y - r * 0.4f,
                                mid.withAlpha(0.24f * masterAlpha),
                                centre.x, centre.y + r * 0.4f, false);
        g.setGradientFill(bg);
        g.fillPath(backBand);
    }

    // Main liquid mass under the front surface.
    juce::Path liquid;
    appendSmooth(liquid, front, false, true);
    liquid.lineTo(toPx(0.995f, 1.0f));
    liquid.lineTo(toPx(-0.995f, 1.0f));
    liquid.closeSubPath();

    {
        const float topY = centre.y + r * (kRestLevel - 0.75f);
        juce::ColourGradient fill(hot.withAlpha((0.60f + 0.22f * energy) * masterAlpha),
                                  centre.x, topY,
                                  juce::Colour(0xFF0E0518).withAlpha(0.97f * masterAlpha),
                                  centre.x, centre.y + r, false);
        fill.addColour(0.16, juce::Colour(0xFFE9C4FF).withAlpha((0.38f + 0.16f * energy) * masterAlpha));
        fill.addColour(0.42, mid.withAlpha((0.62f + 0.14f * energy) * masterAlpha));
        fill.addColour(0.78, deep.withAlpha(0.94f * masterAlpha));
        g.setGradientFill(fill);
        g.fillPath(liquid);
    }

    g.saveState();
    g.reduceClipRegion(liquid);

    // A compact lavender caustic follows the vessel curvature. Lower alpha
    // keeps it from reading as a white fog bank under the liquid.
    {
        juce::ColourGradient caustic(hot.withAlpha((0.11f + 0.09f * energy) * masterAlpha),
                                     centre.x - slosh_ * r * 1.6f,
                                     centre.y + r * 0.78f,
                                     juce::Colours::transparentBlack,
                                     centre.x, centre.y + r * 0.28f, true);
        g.setGradientFill(caustic);
        g.fillEllipse(centre.x - r * 0.52f, centre.y + r * 0.52f,
                      r * 1.04f, r * 0.36f);
    }

    // -- Bubble-only depth detail --------------------------------------------
    // No glitter, star crosses or blurred particle halos. Size, outline alpha
    // and rise speed provide depth with a maximum of eight fixed-pool bubbles.
    for (const auto& b : bubbles_) {
        if (b.life <= 0.01f)
            continue;
        const auto p = toPx(b.x, b.y);
        const float br = b.r * r;
        const float fadeIn = juce::jlimit(0.0f, 1.0f, b.life * 2.2f);
        const float localSurface = surfaceY(b.x * 0.5f + 0.5f) - surfaceMean;
        const float surfaceGap = b.y - localSurface;
        const float upperBlend = juce::jlimit(0.0f, 1.0f, (surfaceGap - 0.10f) / 0.18f);
        const float bodyBlend = juce::jlimit(0.0f, 1.0f, (b.y - 0.18f) / 0.34f);
        const float alpha = b.opacity * fadeIn * upperBlend * bodyBlend
            * (0.66f + b.depth * 0.44f) * masterAlpha;
        if (alpha <= 0.012f)
            continue;

        g.setColour(hot.withAlpha(alpha * 0.18f));
        g.fillEllipse(p.x - br * 0.88f, p.y - br * 0.88f,
                      br * 1.76f, br * 1.76f);
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawEllipse(p.x - br, p.y - br, br * 2.0f, br * 2.0f,
                      0.55f + b.depth * 0.28f);
        const float highlight = br * (0.30f + b.depth * 0.08f);
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.fillEllipse(p.x - br * 0.45f, p.y - br * 0.58f,
                      highlight, highlight * 0.72f);
    }

    g.restoreState(); // liquid clip

    // -- Surface tension line ------------------------------------------------
    // Thin halo + crisp core. The satisfying character comes from curvature
    // and rebound timing, not a thick neon stripe or surface glitter.
    juce::Path meniscus;
    appendSmooth(meniscus, front, false, true);

    g.setColour(hot.withAlpha((0.10f + 0.08f * energy) * masterAlpha));
    g.strokePath(meniscus, juce::PathStrokeType(4.4f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.interpolatedWith(hot, 0.58f)
        .withAlpha((0.42f + 0.18f * energy) * masterAlpha));
    g.strokePath(meniscus, juce::PathStrokeType(1.15f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));


    // Sparse bass-led splash beads. Velocity stretches each bead vertically,
    // giving a liquid ballistic read with only two alpha primitives per bead.
    for (const auto& d : droplets_) {
        if (d.life <= 0.015f)
            continue;
        const auto p = toPx(d.x, d.y);
        const float diameter = d.size * (r / 140.0f);
        const float speed = juce::jlimit(0.0f, 1.0f, std::abs(d.vy) * 11.0f);
        const float width = diameter * (0.78f - speed * 0.12f);
        const float height = diameter * (1.05f + speed * 0.95f);
        const float alpha = juce::jlimit(0.0f, 0.82f,
            d.life * (0.55f + d.brightness * 0.18f) * masterAlpha);

        g.setColour(hot.withAlpha(alpha * 0.18f));
        g.fillEllipse(p.x - width * 0.72f, p.y - height * 0.62f,
                      width * 1.44f, height * 1.24f);
        g.setColour(juce::Colours::white.interpolatedWith(hot, 0.50f)
            .withAlpha(alpha));
        g.fillEllipse(p.x - width * 0.50f, p.y - height * 0.50f,
                      width, height);
    }

    // -- Glass finish ---------------------------------------------------------
    // Keep the vessel clean. Only a restrained top glass pulse follows the
    // beat; no inner blobs, shimmer or mid-liquid highlights remain.
    juce::Path airRegion;
    airRegion.startNewSubPath(toPx(-1.05f, -1.05f));
    airRegion.lineTo(toPx(1.05f, -1.05f));
    airRegion.lineTo(front.back());
    appendSmooth(airRegion, front, true, false);
    airRegion.closeSubPath();

    g.saveState();
    g.reduceClipRegion(airRegion);

    // Reuse the original broad upper reflection, but only its cropped top
    // portion. Its opacity pulses quickly with low-frequency impact while the
    // shape remains soft and natural instead of becoming a hard line.
    const float pulse = juce::jlimit(0.0f, 1.0f, topPulse_);
    if (topGlossLayer_.isValid()) {
        const float glossOpacity = (bypassed_ ? 0.10f : 0.16f)
            + pulse * (bypassed_ ? 0.12f : 0.34f);
        g.setOpacity(juce::jlimit(0.0f, 0.58f, glossOpacity * masterAlpha));
        g.drawImageAt(topGlossLayer_, 0, 0);
        g.setOpacity(1.0f);
    }

    g.restoreState();

    g.restoreState(); // circle clip

    if (staticFrontLayer_.isValid())
        g.drawImageAt(staticFrontLayer_, 0, 0);

}

} // namespace askp
