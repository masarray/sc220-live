#include "LiveLiquidVisualizer.h"

#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace askp::live {
namespace {
constexpr float twoPi = juce::MathConstants<float>::twoPi;
constexpr float restLevel = 0.0f;

float onePoleCoefficient (float frequency, float sampleRate) noexcept
{
    return 1.0f - std::exp (-twoPi * frequency / juce::jmax (1.0f, sampleRate));
}

float compressEnergy (float value, float floor, float scale) noexcept
{
    return juce::jlimit (0.0f, 1.0f,
                         std::log1p (juce::jmax (0.0f, value - floor) * scale)
                            / std::log1p (scale));
}
}

LiveLiquidVisualizer::LiveLiquidVisualizer (LiveAudioEngine& engineToUse)
    : engine (engineToUse)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    glossAsset = juce::ImageCache::getFromMemory (BinaryData::askp_sphere_gloss_png,
                                                   BinaryData::askp_sphere_gloss_pngSize);
}

LiveLiquidVisualizer::~LiveLiquidVisualizer()
{
    setVisualEnabled (false);
}

void LiveLiquidVisualizer::setVisualEnabled (bool enabled)
{
    if (visualEnabled == enabled)
        return;

    visualEnabled = enabled;
    engine.setVisualTapEnabled (enabled);
    silentFrames = 0;
    emptyFrames = 0;

    if (enabled)
    {
        resetAnalysis();
        repaint();
    }
}

void LiveLiquidVisualizer::resized()
{
    rebuildStaticLayers();
}

void LiveLiquidVisualizer::resetAnalysis() noexcept
{
    bandFast.fill (0.0f);
    bandSlow.fill (0.0f);
    bandDrive.fill (0.0f);
    poleALeft.fill (0.0f);
    poleBLeft.fill (0.0f);
    poleARight.fill (0.0f);
    poleBRight.fill (0.0f);
    analysisRate = 0.0;
}

void LiveLiquidVisualizer::rebuildStaticLayers()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty())
    {
        staticBack = {};
        staticFront = {};
        topGloss = {};
        return;
    }

    const auto centre = bounds.toFloat().getCentre();
    const float outerRadius = juce::jmin ((float) bounds.getWidth(), (float) bounds.getHeight()) * 0.5f - 3.0f;
    const float innerRadius = outerRadius - 7.0f;
    const float alpha = bypassState ? 0.46f : 1.0f;
    const auto violet = bypassState ? juce::Colour (0xff85808e) : colours::violetHot;
    const auto deep = bypassState ? juce::Colour (0xff17151c) : juce::Colour (0xff261238);

    staticBack = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g (staticBack);

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillEllipse (centre.x - outerRadius, centre.y - outerRadius + 6.0f,
                   outerRadius * 2.0f, outerRadius * 2.0f);

    juce::ColourGradient rim (juce::Colour (0xfff2f4f8), centre.x - outerRadius * 0.25f,
                              centre.y - outerRadius, juce::Colour (0xff5d6571),
                              centre.x + outerRadius * 0.4f, centre.y + outerRadius, false);
    rim.addColour (0.30, juce::Colour (0xffd6dce4));
    rim.addColour (0.72, juce::Colour (0xff69727e));
    g.setGradientFill (rim);
    g.fillEllipse (centre.x - outerRadius, centre.y - outerRadius,
                   outerRadius * 2.0f, outerRadius * 2.0f);

    juce::ColourGradient bowl (juce::Colour (0xff171020), centre.x, centre.y - innerRadius,
                               juce::Colour (0xff020306), centre.x, centre.y + innerRadius, false);
    bowl.addColour (0.54, deep.withAlpha (0.88f * alpha));
    g.setGradientFill (bowl);
    g.fillEllipse (centre.x - innerRadius, centre.y - innerRadius,
                   innerRadius * 2.0f, innerRadius * 2.0f);

    staticFront = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics front (staticFront);
    juce::Path circle;
    circle.addEllipse (centre.x - innerRadius, centre.y - innerRadius,
                       innerRadius * 2.0f, innerRadius * 2.0f);
    front.saveState();
    front.reduceClipRegion (circle);

    juce::ColourGradient leftShade (juce::Colours::black.withAlpha (0.20f * alpha),
                                    centre.x - innerRadius, centre.y,
                                    juce::Colours::transparentBlack,
                                    centre.x - innerRadius * 0.38f, centre.y, false);
    front.setGradientFill (leftShade);
    front.fillRect (juce::Rectangle<float> (centre.x - innerRadius, centre.y - innerRadius,
                                            innerRadius * 0.82f, innerRadius * 2.0f));

    juce::ColourGradient rightShade (juce::Colours::transparentBlack,
                                     centre.x + innerRadius * 0.38f, centre.y,
                                     juce::Colours::black.withAlpha (0.15f * alpha),
                                     centre.x + innerRadius, centre.y, false);
    front.setGradientFill (rightShade);
    front.fillRect (juce::Rectangle<float> (centre.x + innerRadius * 0.18f,
                                            centre.y - innerRadius,
                                            innerRadius * 0.82f, innerRadius * 2.0f));

    juce::ColourGradient bottomShade (juce::Colours::transparentBlack,
                                      centre.x, centre.y + innerRadius * 0.05f,
                                      juce::Colours::black.withAlpha (0.18f * alpha),
                                      centre.x, centre.y + innerRadius, false);
    front.setGradientFill (bottomShade);
    front.fillRect (juce::Rectangle<float> (centre.x - innerRadius,
                                            centre.y + innerRadius * 0.02f,
                                            innerRadius * 2.0f, innerRadius));
    front.restoreState();

    front.setColour (juce::Colours::black.withAlpha (0.55f));
    front.drawEllipse (centre.x - innerRadius, centre.y - innerRadius,
                       innerRadius * 2.0f, innerRadius * 2.0f, 1.1f);
    front.setColour (violet.withAlpha (0.23f * alpha));
    front.drawEllipse (centre.x - innerRadius + 3.0f, centre.y - innerRadius + 3.0f,
                       innerRadius * 2.0f - 6.0f, innerRadius * 2.0f - 6.0f, 1.0f);
    front.setColour (juce::Colours::white.withAlpha (0.11f * alpha));
    front.drawEllipse (centre.x - innerRadius + 1.8f, centre.y - innerRadius + 1.8f,
                       innerRadius * 2.0f - 3.6f, innerRadius * 2.0f - 3.6f, 0.9f);

    topGloss = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    if (glossAsset.isValid())
    {
        juce::Graphics gloss (topGloss);
        const int sourceHeight = juce::jlimit (1, glossAsset.getHeight(),
            juce::roundToInt ((float) glossAsset.getHeight() * 0.56f));
        gloss.drawImage (glossAsset,
                         juce::roundToInt (centre.x - innerRadius),
                         juce::roundToInt (centre.y - innerRadius),
                         juce::roundToInt (innerRadius * 2.0f),
                         juce::roundToInt (innerRadius * 1.08f),
                         0, 0, glossAsset.getWidth(), sourceHeight, false);
    }
}

void LiveLiquidVisualizer::analyse (int sampleCount, double sampleRate)
{
    if (sampleCount <= 0)
        return;

    if (analysisRate != sampleRate)
    {
        resetAnalysis();
        analysisRate = sampleRate;
    }

    constexpr std::array<float, visualBands - 1> crossoverHz { 72.0f, 180.0f, 620.0f, 2400.0f, 7600.0f };
    std::array<double, visualBands> energyLeft {};
    std::array<double, visualBands> energyRight {};

    std::array<float, visualBands - 1> coefficient {};
    for (int band = 0; band < visualBands - 1; ++band)
        coefficient[(size_t) band] = onePoleCoefficient (crossoverHz[(size_t) band],
                                                         (float) sampleRate);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const float sourceL = leftSamples[(size_t) sample];
        const float sourceR = rightSamples[(size_t) sample];
        std::array<float, visualBands - 1> lowL {};
        std::array<float, visualBands - 1> lowR {};

        for (int split = 0; split < visualBands - 1; ++split)
        {
            const float c = coefficient[(size_t) split];
            poleALeft[(size_t) split] += c * (sourceL - poleALeft[(size_t) split]);
            poleBLeft[(size_t) split] += c * (poleALeft[(size_t) split] - poleBLeft[(size_t) split]);
            poleARight[(size_t) split] += c * (sourceR - poleARight[(size_t) split]);
            poleBRight[(size_t) split] += c * (poleARight[(size_t) split] - poleBRight[(size_t) split]);
            lowL[(size_t) split] = poleBLeft[(size_t) split];
            lowR[(size_t) split] = poleBRight[(size_t) split];
        }

        const std::array<float, visualBands> bandsL {
            lowL[0], lowL[1] - lowL[0], lowL[2] - lowL[1],
            lowL[3] - lowL[2], lowL[4] - lowL[3], sourceL - lowL[4]
        };
        const std::array<float, visualBands> bandsR {
            lowR[0], lowR[1] - lowR[0], lowR[2] - lowR[1],
            lowR[3] - lowR[2], lowR[4] - lowR[3], sourceR - lowR[4]
        };

        for (int band = 0; band < visualBands; ++band)
        {
            energyLeft[(size_t) band] += (double) bandsL[(size_t) band] * bandsL[(size_t) band];
            energyRight[(size_t) band] += (double) bandsR[(size_t) band] * bandsR[(size_t) band];
        }
    }

    const float inverseCount = 1.0f / (float) sampleCount;
    constexpr std::array<float, visualBands> gain { 6.8f, 6.0f, 4.8f, 4.0f, 3.6f, 4.4f };
    for (int band = 0; band < visualBands; ++band)
    {
        const float rmsL = std::sqrt ((float) energyLeft[(size_t) band] * inverseCount);
        const float rmsR = std::sqrt ((float) energyRight[(size_t) band] * inverseCount);
        const float rms = juce::jmax (rmsL, rmsR);
        const float target = compressEnergy (rms * gain[(size_t) band], 0.0012f, 13.0f);
        bandFast[(size_t) band] += (target - bandFast[(size_t) band])
                                * (target > bandFast[(size_t) band] ? 0.62f : 0.23f);
        bandSlow[(size_t) band] += (target - bandSlow[(size_t) band]) * 0.075f;
        const float onset = juce::jmax (0.0f, bandFast[(size_t) band]
                                               - bandSlow[(size_t) band] * 0.82f);
        bandDrive[(size_t) band] = juce::jlimit (0.0f, 1.0f,
                                                 bandFast[(size_t) band] * 0.48f + onset * 2.25f);
    }

    bassEnergy = bandDrive[0] * 0.68f + bandDrive[1] * 0.52f;
    bodyEnergy = bandDrive[2] * 0.55f + bandDrive[3] * 0.42f;
    airEnergy = bandDrive[4] * 0.38f + bandDrive[5] * 0.32f;
    const float dominantEnergy = std::max (bassEnergy, std::max (bodyEnergy * 0.82f, airEnergy * 0.62f));
    totalLevel += (dominantEnergy - totalLevel) * 0.34f;

    const double leftTotal = std::accumulate (energyLeft.begin(), energyLeft.end(), 0.0);
    const double rightTotal = std::accumulate (energyRight.begin(), energyRight.end(), 0.0);
    const float pan = (float) ((rightTotal - leftTotal) / (rightTotal + leftTotal + 1.0e-12));

    if (lowCooldown > 0) --lowCooldown;
    if (bodyCooldown > 0) --bodyCooldown;
    if (airCooldown > 0) --airCooldown;

    if (bassEnergy > 0.18f && lowCooldown == 0)
    {
        const float x = juce::jlimit (0.18f, 0.82f, 0.50f + pan * 0.22f);
        injectPulse (x, 0.22f, 0.055f + bassEnergy * 0.13f, -1.0f);
        sloshVelocity += pan * bassEnergy * 0.030f;
        if (bassEnergy > 0.45f)
        {
            emitBubble (x, bassEnergy, false);
            emitDroplets (x, bassEnergy, bassEnergy > 0.72f ? 2 : 1);
        }
        lowCooldown = 2;
    }

    if (bodyEnergy > 0.24f && bodyCooldown == 0)
    {
        const float x = juce::jlimit (0.18f, 0.82f, 0.48f - pan * 0.18f);
        injectPulse (x, 0.14f, 0.025f + bodyEnergy * 0.055f, -1.0f);
        bodyCooldown = 3;
    }

    if (airEnergy > 0.34f && airCooldown == 0)
    {
        const float x = juce::jlimit (0.12f, 0.88f, 0.55f + pan * 0.30f);
        injectPulse (x, 0.07f, 0.012f + airEnergy * 0.022f, -1.0f);
        if (airEnergy > 0.58f)
            emitBubble (x, airEnergy, true);
        airCooldown = 4;
    }
}

void LiveLiquidVisualizer::injectPulse (float xNormalised, float radius,
                                        float strength, float direction)
{
    for (int node = 0; node < fluidNodes; ++node)
    {
        const float x = (float) node / (float) (fluidNodes - 1);
        const float distance = std::abs (x - xNormalised);
        if (distance >= radius)
            continue;
        const float shape = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * distance / radius);
        fluidVelocity[(size_t) node] += direction * strength * shape;
    }
}

void LiveLiquidVisualizer::advanceFluid (bool active)
{
    std::array<float, fluidNodes> acceleration {};
    for (int node = 0; node < fluidNodes; ++node)
    {
        const float here = fluidHeight[(size_t) node];
        const float left = fluidHeight[(size_t) juce::jmax (0, node - 1)];
        const float right = fluidHeight[(size_t) juce::jmin (fluidNodes - 1, node + 1)];
        acceleration[(size_t) node] = (left + right - 2.0f * here) * 0.185f
                                    - here * 0.052f;
    }

    sloshVelocity += (-slosh * 0.030f - sloshVelocity * 0.090f);
    slosh += sloshVelocity;
    slosh = juce::jlimit (-0.10f, 0.10f, slosh);

    float mean = 0.0f;
    for (int node = 0; node < fluidNodes; ++node)
    {
        fluidVelocity[(size_t) node] = (fluidVelocity[(size_t) node]
                                      + acceleration[(size_t) node]) * (active ? 0.905f : 0.82f);
        fluidHeight[(size_t) node] += fluidVelocity[(size_t) node];
        const float x = (float) node / (float) (fluidNodes - 1) - 0.5f;
        fluidHeight[(size_t) node] += slosh * x * 0.034f;
        mean += fluidHeight[(size_t) node];
    }

    mean /= (float) fluidNodes;
    for (auto& height : fluidHeight)
        height = juce::jlimit (-0.29f, 0.24f, height - mean);
}

float LiveLiquidVisualizer::surfaceAt (float xNormalised) const noexcept
{
    xNormalised = juce::jlimit (0.0f, 1.0f, xNormalised);
    const float position = xNormalised * (float) (fluidNodes - 1);
    const int lower = juce::jlimit (0, fluidNodes - 1, (int) position);
    const int upper = juce::jmin (fluidNodes - 1, lower + 1);
    const float fraction = position - (float) lower;
    return restLevel + fluidHeight[(size_t) lower]
         + fraction * (fluidHeight[(size_t) upper] - fluidHeight[(size_t) lower]);
}

void LiveLiquidVisualizer::emitBubble (float xNormalised, float strength, bool micro)
{
    auto& bubble = bubbles[(size_t) (bubbleCursor++ % bubbleCount)];
    bubble.x = juce::jlimit (-0.82f, 0.82f, xNormalised * 2.0f - 1.0f
        + (random.nextFloat() * 2.0f - 1.0f) * (micro ? 0.04f : 0.07f));
    bubble.y = micro ? 0.62f + random.nextFloat() * 0.12f
                     : 0.72f + random.nextFloat() * 0.12f;
    bubble.radius = micro ? 0.006f + strength * 0.004f
                          : 0.010f + strength * 0.008f;
    bubble.rise = micro ? 0.0060f : 0.0041f;
    bubble.sway = micro ? 0.0011f : 0.0019f;
    bubble.phase = random.nextFloat() * twoPi;
    bubble.opacity = micro ? 0.22f : 0.30f;
    bubble.life = 1.0f;
}

void LiveLiquidVisualizer::emitDroplets (float xNormalised, float strength, int count)
{
    static constexpr std::array<float, 3> spread { -0.045f, 0.030f, 0.065f };
    count = juce::jlimit (0, 3, count);
    for (int index = 0; index < count; ++index)
    {
        auto& drop = droplets[(size_t) (dropletCursor++ % dropletCount)];
        drop.x = juce::jlimit (-0.86f, 0.86f, xNormalised * 2.0f - 1.0f);
        drop.y = surfaceAt (xNormalised);
        drop.vx = spread[(size_t) index] * (0.75f + strength * 0.4f);
        drop.vy = -(0.060f + strength * 0.075f) * (1.0f - index * 0.08f);
        drop.size = 1.0f + strength * 1.2f;
        drop.brightness = strength;
        drop.life = 1.0f;
    }
}

void LiveLiquidVisualizer::advanceParticles (bool active)
{
    for (auto& bubble : bubbles)
    {
        if (bubble.life <= 0.001f)
            continue;
        if (! active)
        {
            bubble.life *= 0.76f;
            continue;
        }

        bubble.y -= bubble.rise * (0.42f + bassEnergy * 0.45f + bodyEnergy * 0.15f);
        bubble.x += std::sin (animationPhase * 0.8f + bubble.phase) * bubble.sway;
        bubble.life *= 0.995f;
        const float localSurface = surfaceAt (bubble.x * 0.5f + 0.5f);
        if (bubble.y < localSurface + 0.20f)
            bubble.life *= 0.77f;
        if (bubble.y < localSurface + 0.08f)
            bubble.life = 0.0f;
    }

    for (auto& drop : droplets)
    {
        if (drop.life <= 0.001f)
            continue;
        if (! active)
        {
            drop.life *= 0.70f;
            continue;
        }
        drop.x += drop.vx;
        drop.y += drop.vy;
        drop.vy += 0.012f;
        drop.life *= 0.90f;
        if (drop.y > surfaceAt (drop.x * 0.5f + 0.5f) + 0.03f)
            drop.life = 0.0f;
    }

    if (active && --ambientBubbleCountdown <= 0)
    {
        if (bassEnergy > 0.10f || bodyEnergy > 0.16f)
            emitBubble (0.25f + random.nextFloat() * 0.50f, 0.3f, true);
        ambientBubbleCountdown = 18 + random.nextInt (24);
    }
}

bool LiveLiquidVisualizer::refresh (bool bypassed)
{
    if (! visualEnabled)
        return false;

    const bool bypassChanged = bypassState != bypassed;
    if (bypassChanged)
    {
        bypassState = bypassed;
        rebuildStaticLayers();
    }

    const int captured = engine.copyLatestVisualSamples (leftSamples.data(), rightSamples.data(),
                                                         analysisSamples);
    if (captured <= 0)
    {
        ++emptyFrames;
        totalLevel *= 0.82f;
    }
    else
    {
        emptyFrames = 0;
        analyse (captured, juce::jmax (1.0, engine.getCurrentSampleRate()));
    }

    const bool active = ! bypassed && totalLevel > 0.006f && captured > 0;
    silentFrames = active ? 0 : silentFrames + 1;
    advanceFluid (active);
    advanceParticles (active);
    animationPhase += active ? 0.14f : 0.045f;

    // Once fully settled, stop repainting the dynamic surface until new audio or
    // a bypass transition arrives. The parent timer may continue, but paint work
    // and GPU composition remain asleep.
    float motion = std::abs (slosh) + std::abs (sloshVelocity);
    for (int node = 0; node < fluidNodes; ++node)
        motion += std::abs (fluidHeight[(size_t) node]) + std::abs (fluidVelocity[(size_t) node]);

    const bool asleep = silentFrames > 18 && motion < 0.015f && ! bypassChanged;
    if (asleep)
        return false;

    repaint();
    return true;
}

void LiveLiquidVisualizer::paint (juce::Graphics& g)
{
    if (staticBack.isValid())
        g.drawImageAt (staticBack, 0, 0);

    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float outerRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    const float radius = outerRadius - 7.0f;
    if (radius <= 4.0f)
        return;

    juce::Path clip;
    clip.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    std::array<juce::Point<float>, fluidNodes> surfacePoints {};
    for (int node = 0; node < fluidNodes; ++node)
    {
        const float xNormalised = (float) node / (float) (fluidNodes - 1);
        const float xBowl = xNormalised * 2.0f - 1.0f;
        const float edge = std::sqrt (juce::jmax (0.0f, 1.0f - xBowl * xBowl));
        const float x = centre.x + xBowl * radius;
        const float y = centre.y + surfaceAt (xNormalised) * radius * 0.62f;
        const float clippedY = juce::jlimit (centre.y - edge * radius * 0.92f,
                                             centre.y + edge * radius * 0.92f, y);
        surfacePoints[(size_t) node] = { x, clippedY };
    }

    juce::Path surface;
    surface.startNewSubPath (surfacePoints.front());
    for (int node = 1; node < fluidNodes - 1; ++node)
    {
        const auto midpoint = (surfacePoints[(size_t) node]
                             + surfacePoints[(size_t) node + 1]) * 0.5f;
        surface.quadraticTo (surfacePoints[(size_t) node], midpoint);
    }
    surface.lineTo (surfacePoints.back());

    juce::Path liquid = surface;
    liquid.lineTo (centre.x + radius, centre.y + radius);
    liquid.lineTo (centre.x - radius, centre.y + radius);
    liquid.closeSubPath();

    const float alpha = bypassState ? 0.42f : 1.0f;
    juce::ColourGradient fill (juce::Colour (0xffa269ff).withAlpha (0.78f * alpha),
                               centre.x, centre.y - radius * 0.18f,
                               juce::Colour (0xff1c0838).withAlpha (0.98f * alpha),
                               centre.x, centre.y + radius, false);
    fill.addColour (0.35, juce::Colour (0xff6c39c8).withAlpha (0.92f * alpha));
    fill.addColour (0.75, juce::Colour (0xff351068).withAlpha (0.98f * alpha));
    g.setGradientFill (fill);
    g.fillPath (liquid);

    g.setColour (juce::Colour (0xffd9b6ff).withAlpha (0.15f * alpha));
    g.strokePath (surface, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffb67aff).withAlpha (0.78f * alpha));
    g.strokePath (surface, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    for (const auto& bubble : bubbles)
    {
        if (bubble.life <= 0.01f)
            continue;
        const float x = centre.x + bubble.x * radius;
        const float y = centre.y + bubble.y * radius;
        const float size = bubble.radius * radius * 2.0f;
        const float opacity = bubble.opacity * bubble.life * alpha;
        g.setColour (juce::Colours::white.withAlpha (opacity * 0.28f));
        g.fillEllipse (x - size * 0.50f, y - size * 0.50f, size, size);
        g.setColour (juce::Colour (0xffd8c3ff).withAlpha (opacity));
        g.drawEllipse (x - size * 0.50f, y - size * 0.50f, size, size, 0.8f);
    }

    for (const auto& drop : droplets)
    {
        if (drop.life <= 0.01f)
            continue;
        const float x = centre.x + drop.x * radius;
        const float y = centre.y + drop.y * radius;
        const float size = drop.size * (0.75f + radius * 0.005f);
        g.setColour (juce::Colour (0xffd2a8ff).withAlpha (
            juce::jlimit (0.0f, 0.75f, drop.life * (0.30f + drop.brightness * 0.36f) * alpha)));
        g.fillEllipse (x - size, y - size, size * 2.0f, size * 2.0f);
    }

    g.restoreState();

    if (topGloss.isValid())
        g.drawImageAt (topGloss, 0, 0);
    if (staticFront.isValid())
        g.drawImageAt (staticFront, 0, 0);
}

} // namespace askp::live
