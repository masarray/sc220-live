#include "AquariumVisualizer.h"
#include "LiveTheme.h"

#include <cmath>

namespace askp::live {
namespace {
float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }
} // namespace

AquariumVisualizer::AquariumVisualizer (askp::Processor& processorToUse)
    : processor (processorToUse)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
}

void AquariumVisualizer::resized()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty())
    {
        staticLayer = {};
        return;
    }

    staticLayer = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g (staticLayer);
    auto b = bounds.toFloat().reduced (0.5f);

    juce::ColourGradient bg (colours::panelHi.brighter (0.03f), b.getX(), b.getY(),
                             colours::panelLo, b.getRight(), b.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (b, 14.0f);

    // Glass body.
    juce::ColourGradient glass (juce::Colours::white.withAlpha (0.05f), b.getCentreX(), b.getY(),
                                juce::Colours::transparentBlack, b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (b.reduced (8.0f), 12.0f);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (juce::Rectangle<float> (b.getX() + 16.0f, b.getY() + 14.0f,
                                                    b.getWidth() - 32.0f, 18.0f), 8.0f);

    g.setColour (colours::border);
    g.drawRoundedRectangle (b, 14.0f, 1.0f);
    g.setColour (colours::borderSoft.withAlpha (0.9f));
    g.drawRoundedRectangle (b.reduced (8.0f), 12.0f, 1.0f);
}

float AquariumVisualizer::bandAt (int index) const noexcept
{
    return bands[(size_t) juce::jlimit (0, kBands - 1, index)];
}

void AquariumVisualizer::seedBubble (Bubble& bubble, bool anywhere)
{
    const float areaX = 0.08f + random.nextFloat() * 0.84f;
    bubble.x = areaX;
    bubble.y = anywhere ? (0.25f + random.nextFloat() * 0.60f) : (0.68f + random.nextFloat() * 0.18f);
    bubble.radius = 1.8f + random.nextFloat() * 3.2f;
    bubble.speed = 0.0045f + random.nextFloat() * 0.0055f;
    bubble.life = 0.5f + random.nextFloat() * 0.5f;
    bubble.opacity = 0.12f + random.nextFloat() * 0.14f;
}

void AquariumVisualizer::updateBandsFromScope()
{
    const int captured = processor.scopeTap.consumeLatest (left.data(), right.data(), kCaptureSamples);
    if (captured <= 0)
    {
        for (auto& target : bandTargets)
            target *= 0.90f;
        return;
    }

    std::array<float, kBands> accum{};
    std::array<int, kBands> counts{};

    for (int i = 0; i < captured; ++i)
    {
        const float mid = 0.5f * (left[(size_t) i] + right[(size_t) i]);
        const float absMid = std::abs (mid);
        const float pos = (float) i / (float) juce::jmax (1, captured - 1);
        const float skewed = std::pow (pos, 0.62f);
        const int band = juce::jlimit (0, kBands - 1, (int) std::floor (skewed * (float) (kBands - 1)));
        accum[(size_t) band] += absMid;
        counts[(size_t) band] += 1;
    }

    for (int i = 0; i < kBands; ++i)
    {
        const float mean = counts[(size_t) i] > 0 ? accum[(size_t) i] / (float) counts[(size_t) i] : 0.0f;
        const float weighted = mean * (1.15f - 0.60f * ((float) i / (float) (kBands - 1)));
        bandTargets[(size_t) i] = juce::jmax (bandTargets[(size_t) i] * 0.76f, clamp01 (weighted * 16.0f));
        const float attack = bandTargets[(size_t) i] > bands[(size_t) i] ? 0.46f : 0.12f;
        bands[(size_t) i] += (bandTargets[(size_t) i] - bands[(size_t) i]) * attack;
    }

    lowEnergy = 0.0f;
    globalEnergy = 0.0f;
    for (int i = 0; i < kBands; ++i)
    {
        globalEnergy += bands[(size_t) i];
        if (i < 10)
            lowEnergy += bands[(size_t) i];
    }
    globalEnergy = clamp01 (globalEnergy / (float) kBands * 1.2f);
    lowEnergy = clamp01 (lowEnergy / 10.0f * 1.55f);
}

void AquariumVisualizer::advanceBubbles (float energy, bool active)
{
    if (! seeded)
    {
        for (auto& bubble : bubbles)
            seedBubble (bubble, true);
        seeded = true;
    }

    if (ambientCountdown <= 0 && active && energy > 0.10f)
    {
        seedBubble (bubbles[(size_t) random.nextInt (kBubbles)], false);
        ambientCountdown = 6 + random.nextInt (10);
    }
    --ambientCountdown;

    for (auto& bubble : bubbles)
    {
        if (bubble.life <= 0.001f)
        {
            seedBubble (bubble, false);
            bubble.life *= 0.4f;
        }

        if (! active)
        {
            bubble.life *= 0.96f;
            continue;
        }

        bubble.y -= bubble.speed * (0.8f + energy * 1.8f);
        bubble.x += std::sin (phase * 0.7f + bubble.y * 8.0f) * 0.0009f;
        bubble.life *= 0.997f;

        const int index = juce::jlimit (0, kNodes - 1, (int) std::round (bubble.x * (float) (kNodes - 1)));
        const float surface = 0.56f + nodes[(size_t) index] * 0.18f;
        if (bubble.y <= surface + 0.01f)
            bubble.life *= 0.86f;
        if (bubble.y <= surface - 0.02f)
            bubble.life = 0.0f;
    }
}

void AquariumVisualizer::updateFluid (bool active)
{
    const float drive = active ? lowEnergy : 0.0f;
    const float centreBias = 0.5f + (bandAt (6) - bandAt (3)) * 0.06f;
    const int centre = juce::jlimit (2, kNodes - 3, (int) std::round (centreBias * (float) (kNodes - 1)));

    if (active)
    {
        velocities[(size_t) centre] -= drive * 0.010f;
        velocities[(size_t) juce::jmax (1, centre - 1)] -= drive * 0.006f;
        velocities[(size_t) juce::jmin (kNodes - 2, centre + 1)] -= drive * 0.006f;
    }

    std::array<float, kNodes> nextVel = velocities;
    for (int i = 1; i < kNodes - 1; ++i)
    {
        const float neighbourPull = (nodes[(size_t) (i - 1)] + nodes[(size_t) (i + 1)] - 2.0f * nodes[(size_t) i]) * 0.22f;
        nextVel[(size_t) i] += neighbourPull;
        nextVel[(size_t) i] -= nodes[(size_t) i] * 0.030f;
        nextVel[(size_t) i] *= active ? 0.948f : 0.905f;
    }

    velocities = nextVel;
    for (int i = 0; i < kNodes; ++i)
    {
        nodes[(size_t) i] += velocities[(size_t) i];
        nodes[(size_t) i] = juce::jlimit (-0.28f, 0.28f, nodes[(size_t) i]);
    }
}

void AquariumVisualizer::refresh (bool bypassed)
{
    bypassState = bypassed;
    updateBandsFromScope();
    const bool active = ! bypassed && globalEnergy > 0.015f;
    phase += 0.09f + globalEnergy * 0.08f;
    updateFluid (active);
    advanceBubbles (globalEnergy, active);
    repaint();
}

void AquariumVisualizer::paint (juce::Graphics& g)
{
    if (staticLayer.isValid())
        g.drawImageAt (staticLayer, 0, 0);

    auto b = getLocalBounds().toFloat().reduced (8.0f);
    auto inner = b.reduced (10.0f);

    // Liquid clip
    juce::Path liquidPath;
    const float leftX = inner.getX();
    const float rightX = inner.getRight();
    const float bottomY = inner.getBottom();
    const float topBase = inner.getY() + inner.getHeight() * 0.57f;

    liquidPath.startNewSubPath (leftX, bottomY);
    liquidPath.lineTo (leftX, topBase + nodes.front() * inner.getHeight() * 0.18f);
    for (int i = 0; i < kNodes; ++i)
    {
        const float t = (float) i / (float) (kNodes - 1);
        const float x = juce::jmap (t, leftX, rightX);
        const float y = topBase + nodes[(size_t) i] * inner.getHeight() * 0.18f;
        liquidPath.lineTo (x, y);
    }
    liquidPath.lineTo (rightX, bottomY);
    liquidPath.closeSubPath();

    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (inner, 12.0f);
    g.reduceClipRegion (clip);

    juce::Colour liquidTop = bypassState ? juce::Colour (0xff5e5671) : colours::violetHot.brighter (0.10f);
    juce::Colour liquidBottom = bypassState ? juce::Colour (0xff2a2335) : juce::Colour (0xff2c0457);
    juce::ColourGradient fill (liquidTop, inner.getCentreX(), inner.getY() + inner.getHeight() * 0.40f,
                               liquidBottom, inner.getCentreX(), inner.getBottom(), false);
    fill.addColour (0.45, bypassState ? juce::Colour (0xff4d3c6e) : juce::Colour (0xff6f3be3));
    g.setGradientFill (fill);
    g.fillPath (liquidPath);

    // highlight line
    juce::Path rim;
    rim.startNewSubPath (leftX, topBase + nodes.front() * inner.getHeight() * 0.18f);
    for (int i = 0; i < kNodes; ++i)
    {
        const float t = (float) i / (float) (kNodes - 1);
        rim.lineTo (juce::jmap (t, leftX, rightX),
                    topBase + nodes[(size_t) i] * inner.getHeight() * 0.18f);
    }
    g.setColour ((bypassState ? colours::textMuted : colours::violetHot).withAlpha (0.85f));
    g.strokePath (rim, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Bubbles
    for (const auto& bubble : bubbles)
    {
        if (bubble.life <= 0.02f)
            continue;

        const float x = juce::jmap (bubble.x, inner.getX(), inner.getRight());
        const float y = juce::jmap (bubble.y, inner.getY(), inner.getBottom());
        const float alpha = bubble.opacity * bubble.life;
        g.setColour (juce::Colours::white.withAlpha (alpha * 0.65f));
        g.drawEllipse (x - bubble.radius, y - bubble.radius, bubble.radius * 2.0f, bubble.radius * 2.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (alpha * 0.18f));
        g.fillEllipse (x - bubble.radius * 0.45f, y - bubble.radius * 0.45f,
                       bubble.radius * 0.90f, bubble.radius * 0.90f);
    }

    // subtle reflections
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.08f), inner.getX(), inner.getY(),
                                juce::Colours::transparentWhite, inner.getRight(), inner.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (juce::Rectangle<float> (inner.getX() + 10.0f, inner.getY() + 10.0f,
                                                    inner.getWidth() * 0.42f, inner.getHeight() * 0.15f), 10.0f);

    g.restoreState();
}

} // namespace askp::live
