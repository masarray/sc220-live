#include "SC220MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include "BinaryData.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace askp::live {
namespace palette {
const auto ivory = juce::Colour (0xff10151a);
const auto surface = juce::Colour (0xff151b21);
const auto surfaceAlt = juce::Colour (0xff1d252d);
const auto header = juce::Colour (0xff090d11);
const auto screen = juce::Colour (0xff080d11);
const auto ink = juce::Colour (0xffedf3f6);
const auto graphite = juce::Colour (0xffc1cbd1);
const auto muted = juce::Colour (0xff84919a);
const auto border = juce::Colour (0xff36434d);
const auto borderSoft = juce::Colour (0xff27323a);
const auto cyan = juce::Colour (0xff35c4d8);
const auto cyanDeep = juce::Colour (0xff176d7c);
const auto orange = juce::Colour (0xffe8792f);
const auto green = juce::Colour (0xff3ac985);
const auto amber = juce::Colour (0xffe4b34b);
const auto red = juce::Colour (0xffe05b55);
const auto metalHi = juce::Colour (0xffd5dce0);
const auto metalMid = juce::Colour (0xff77838b);
const auto metalLo = juce::Colour (0xff303a42);
}

namespace {
juce::Image loadImage (const char* resourceName)
{
    int bytes = 0;
    if (const auto* data = BinaryData::getNamedResource (resourceName, bytes))
        return juce::ImageCache::getFromMemory (data, bytes);
    return {};
}

juce::Typeface::Ptr embeddedInterRegular()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (
        BinaryData::inter_regular_ttf, BinaryData::inter_regular_ttfSize);
    return face;
}

juce::Typeface::Ptr embeddedInterMedium()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (
        BinaryData::inter_medium_ttf, BinaryData::inter_medium_ttfSize);
    return face;
}

juce::Typeface::Ptr embeddedInterSemibold()
{
    static const auto face = juce::Typeface::createSystemTypefaceFor (
        BinaryData::inter_semibold_ttf, BinaryData::inter_semibold_ttfSize);
    return face;
}

juce::Font interFont (float size, bool semibold = false)
{
    const auto face = semibold ? embeddedInterSemibold() : embeddedInterRegular();
    if (face != nullptr)
        return juce::Font { juce::FontOptions { face }.withHeight (size) };

    auto fallback = juce::Font { juce::FontOptions { size } };
    return semibold ? fallback.boldened() : fallback;
}

juce::Font interMediumFont (float size)
{
    const auto face = embeddedInterMedium();
    return face != nullptr
        ? juce::Font { juce::FontOptions { face }.withHeight (size) }
        : interFont (size);
}

float normaliseDb (float db, float floor = -84.0f) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db - floor) / -floor);
}

void drawFaderScale (juce::Graphics& g, juce::Rectangle<int> sliderBounds)
{
    static constexpr std::array<int, 8> values { 12, 6, 0, -6, -18, -30, -42, -60 };
    const auto top = static_cast<float> (sliderBounds.getY() + 18);
    const auto height = static_cast<float> (sliderBounds.getHeight() - 36);
    const auto centreX = static_cast<float> (sliderBounds.getCentreX());

    g.setFont (interFont (9.5f));
    for (const auto db : values)
    {
        const auto normal = (12.0f - static_cast<float> (db)) / 72.0f;
        const auto y = top + normal * height;
        const auto major = db == 0 || db == -60;
        g.setColour ((db == 0 ? palette::ink : palette::muted)
                         .withAlpha (major ? 0.88f : 0.66f));
        g.drawLine (centreX + 9.0f, y, centreX + (major ? 17.0f : 14.0f), y,
                    major ? 1.0f : 0.7f);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db),
                    juce::roundToInt (centreX + 20.0f), juce::roundToInt (y - 7.0f),
                    28, 14, juce::Justification::centredLeft);
    }
}
} // namespace

class SC220MainComponent::SurfaceLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SurfaceLookAndFeel()
    {
        if (const auto face = embeddedInterRegular())
            setDefaultSansSerifTypeface (face);

        setColour (juce::ComboBox::backgroundColourId, palette::surface);
        setColour (juce::ComboBox::textColourId, palette::cyan);
        setColour (juce::ComboBox::outlineColourId, palette::border);
        setColour (juce::PopupMenu::backgroundColourId, palette::surface);
        setColour (juce::PopupMenu::textColourId, palette::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::cyanDeep);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, palette::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, palette::ink);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return interFont (juce::jlimit (10.0f, 13.0f, height * 0.34f), true);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return interMediumFont (juce::jlimit (10.0f, 13.0f, box.getHeight() * 0.34f));
    }

    juce::Font getPopupMenuFont() override
    {
        return interFont (13.0f);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const auto requested = label.getFont();
        const auto height = juce::jlimit (9.0f, 28.0f, requested.getHeight());
        return interFont (height, requested.isBold());
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        if (font.isBold())
            if (const auto face = embeddedInterSemibold())
                return face;
        if (const auto face = embeddedInterRegular())
            return face;
        return juce::LookAndFeel_V4::getTypefaceForFont (font);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced (0.7f);
        const bool liveButton = button.getButtonText() == "GO LIVE" || button.getButtonText() == "LIVE";
        auto fill = button.getToggleState() ? palette::cyanDeep : palette::surfaceAlt;
        if (liveButton)
            fill = button.getToggleState() ? palette::green : palette::orange;
        if (highlighted)
            fill = liveButton ? fill.brighter (0.08f) : fill.interpolatedWith (juce::Colours::white, 0.10f);
        if (down)
            fill = fill.darker (0.12f);

        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), 5.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (button.getToggleState() || liveButton ? juce::Colours::transparentBlack
                                                           : palette::border);
        g.drawRoundedRectangle (area, 5.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        const bool liveButton = button.getButtonText() == "GO LIVE" || button.getButtonText() == "LIVE";
        const bool inverse = button.getToggleState() || liveButton;
        g.setColour (inverse ? juce::Colours::white : palette::ink);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (6, 2),
                          juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto area = juce::Rectangle<float> (0.5f, 0.5f,
                                                   static_cast<float> (width - 1),
                                                   static_cast<float> (height - 1));
        g.setColour (palette::surface);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (box.isMouseOverOrDragging() ? palette::cyan.withAlpha (0.78f)
                                                  : palette::border);
        g.drawRoundedRectangle (area, 5.0f, 1.0f);

        juce::Path chevron;
        const float cx = static_cast<float> (width - 18);
        const float cy = static_cast<float> (height) * 0.5f;
        chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (palette::ink);
        g.strokePath (chevron, juce::PathStrokeType (1.4f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (11, 1, box.getWidth() - 38, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const float diameter = static_cast<float> (juce::jmin (width, height)) - 16.0f;
        const float radius = diameter * 0.5f;
        const auto centre = juce::Point<float> (static_cast<float> (x + width) * 0.5f,
                                                static_cast<float> (y + height) * 0.5f);
        const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
        const float alpha = slider.isEnabled() ? 1.0f : 0.38f;

        // The approved console uses instrument-style calibration dots rather
        // than radial strokes. Major dots are only slightly larger so the ring
        // stays quiet and precise.
        for (int i = 0; i <= 20; ++i)
        {
            const auto tickPosition = static_cast<float> (i) / 20.0f;
            const auto tickAngle = startAngle + tickPosition * (endAngle - startAngle);
            const auto dot = centre.getPointOnCircumference (radius + 6.5f, tickAngle);
            const auto dotSize = i % 5 == 0 ? 2.6f : 1.8f;
            g.setColour (palette::graphite.withAlpha ((i % 5 == 0 ? 0.82f : 0.58f) * alpha));
            g.fillEllipse (dot.x - dotSize * 0.5f, dot.y - dotSize * 0.5f, dotSize, dotSize);
        }

        g.setColour (palette::header.withAlpha (0.68f * alpha));
        g.fillEllipse (knob.expanded (2.0f).translated (0.0f, 3.5f));

        juce::ColourGradient rim (palette::metalHi, knob.getCentreX(), knob.getY(),
                                  palette::metalLo, knob.getCentreX(), knob.getBottom(), false);
        rim.addColour (0.30, juce::Colour (0xffd9dad6));
        rim.addColour (0.72, juce::Colour (0xff777b78));
        rim.multiplyOpacity (alpha);
        g.setGradientFill (rim);
        g.fillEllipse (knob);
        g.setColour (juce::Colours::black.withAlpha (0.42f * alpha));
        g.drawEllipse (knob, 1.0f);

        // A thin bezel leaves the black face dominant, matching the physical
        // surface in the approved proposal.
        const auto face = knob.reduced (3.2f);
        juce::ColourGradient faceMetal (juce::Colour (0xff4a4d4c), face.getX(), face.getY(),
                                        juce::Colour (0xff090a0a), face.getRight(), face.getBottom(), false);
        faceMetal.addColour (0.46, juce::Colour (0xff262928));
        faceMetal.multiplyOpacity (alpha);
        g.setGradientFill (faceMetal);
        g.fillEllipse (face);

        // Four extremely subtle satin facets keep the knob face dimensional
        // without turning the clean console into a decorative illustration.
        const float faceRadius = face.getWidth() * 0.5f;
        for (int facet = 0; facet < 4; ++facet)
        {
            const auto facetStart = -juce::MathConstants<float>::halfPi
                                  + static_cast<float> (facet) * juce::MathConstants<float>::halfPi;
            const auto facetEnd = facetStart + juce::MathConstants<float>::halfPi;
            juce::Path wedge;
            wedge.startNewSubPath (centre);
            wedge.lineTo (centre.getPointOnCircumference (faceRadius - 1.6f, facetStart));
            wedge.addCentredArc (centre.x, centre.y, faceRadius - 1.6f, faceRadius - 1.6f,
                                 0.0f, facetStart, facetEnd, false);
            wedge.closeSubPath();
            g.setColour ((facet == 0 || facet == 2 ? juce::Colours::white
                                                   : juce::Colours::black)
                             .withAlpha ((facet == 0 ? 0.055f : 0.040f) * alpha));
            g.fillPath (wedge);
        }
        g.setColour (juce::Colours::white.withAlpha (0.14f * alpha));
        g.drawEllipse (face.reduced (0.8f), 0.8f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius + 3.2f, radius + 3.2f, 0.0f,
                             startAngle, endAngle, true);
        g.setColour (palette::border.withAlpha (alpha));
        g.strokePath (track, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // The pointer carries the value. The approved hardware surface uses a
        // short, fixed orange signature mark on the right-hand side.
        const auto accentStart = endAngle - 0.38f * (endAngle - startAngle);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius + 3.2f, radius + 3.2f, 0.0f,
                           accentStart, endAngle, true);
        g.setColour (palette::cyan.withAlpha (alpha));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const float angle = startAngle + position * (endAngle - startAngle);
        const auto pointerStart = centre.getPointOnCircumference (radius * 0.12f, angle);
        const auto pointerEnd = centre.getPointOnCircumference (radius * 0.73f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.62f * alpha));
        g.drawLine ({ pointerStart.translated (0.7f, 0.9f),
                      pointerEnd.translated (0.7f, 0.9f) }, 2.8f);
        g.setColour (juce::Colours::white.withAlpha (0.94f * alpha));
        g.drawLine ({ pointerStart, pointerEnd }, 2.2f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float alpha = slider.isEnabled() ? 1.0f : 0.36f;
        const float centreX = static_cast<float> (x + width) * 0.5f;
        const auto track = juce::Rectangle<float> (centreX - 5.0f, static_cast<float> (y + 12),
                                                    10.0f, static_cast<float> (height - 24));
        g.setColour (juce::Colours::black.withAlpha (0.16f * alpha));
        g.fillRoundedRectangle (track.expanded (2.0f, 1.0f), 4.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050606), track.getX(), track.getY(),
                                                 juce::Colour (0xff242625), track.getRight(), track.getY(), false));
        g.fillRoundedRectangle (track, 3.6f);
        g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
        g.drawVerticalLine (juce::roundToInt (track.getCentreX()),
                            track.getY() + 3.0f, track.getBottom() - 3.0f);

        const auto cap = juce::Rectangle<float> (centreX - 20.0f, sliderPos - 34.0f, 40.0f, 68.0f);

        // A single metal face avoids any side strip reading as a fake shadow.
        const auto capFace = cap.reduced (0.75f, 2.0f);
        juce::ColourGradient capGradient (palette::metalHi, cap.getCentreX(), cap.getY(),
                                          palette::metalLo, cap.getCentreX(), cap.getBottom(), false);
        capGradient.addColour (0.18, juce::Colour (0xffd8d9d5));
        capGradient.addColour (0.48, palette::metalMid);
        capGradient.addColour (0.72, juce::Colour (0xffeeeeea));
        capGradient.multiplyOpacity (alpha);
        g.setGradientFill (capGradient);
        g.fillRoundedRectangle (capFace, 2.0f);
        g.setColour (juce::Colour (0xff5b5e5c).withAlpha (alpha));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.42f * alpha));
        g.drawHorizontalLine (juce::roundToInt (capFace.getY() + 2.0f),
                              capFace.getX() + 2.0f, capFace.getRight() - 2.0f);
        for (int stripe = -24; stripe <= 24; stripe += 6)
        {
            g.setColour (stripe == 0 ? juce::Colour (0xff343735).withAlpha (alpha)
                                     : juce::Colours::black.withAlpha (0.16f * alpha));
            g.drawHorizontalLine (juce::roundToInt (cap.getCentreY() + static_cast<float> (stripe)),
                                  capFace.getX() + 3.0f, capFace.getRight() - 3.0f);
        }
    }

    int getSliderThumbRadius (juce::Slider& slider) override
    {
        if (slider.getSliderStyle() == juce::Slider::LinearVertical)
            return 34;

        return juce::LookAndFeel_V4::getSliderThumbRadius (slider);
    }
};

class SC220MainComponent::StereoMeter final : public juce::Component
{
public:
    void setLevels (float leftDb, float rightDb)
    {
        left = leftDb;
        right = rightDb;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced (2);
        const int barWidth = juce::jlimit (7, 10, area.getWidth() / 8);
        auto labels = area.removeFromTop (16);
        g.setColour (palette::muted);
        g.setFont (interFont (8.0f, true));
        g.drawText ("L", labels.withWidth (barWidth), juce::Justification::centred);
        g.drawText ("R", labels.removeFromRight (barWidth), juce::Justification::centred);
        drawBar (g, area.withWidth (barWidth), left);
        drawBar (g, area.removeFromRight (barWidth), right);
    }

private:
    static void drawBar (juce::Graphics& g, juce::Rectangle<int> area, float db)
    {
        g.setColour (juce::Colour (0xff232526));
        g.fillRoundedRectangle (area.toFloat(), 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawVerticalLine (area.getX() + 1, static_cast<float> (area.getY() + 2),
                            static_cast<float> (area.getBottom() - 2));
        constexpr int segments = 25;
        const float normal = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        const int active = juce::roundToInt (normal * segments);
        const float segmentHeight = static_cast<float> (area.getHeight()) / segments;
        for (int i = 0; i < segments; ++i)
        {
            const bool lit = i < active;
            const float y = static_cast<float> (area.getBottom()) - (i + 1) * segmentHeight;
            const auto colour = i > 21 ? palette::red : i > 17 ? palette::amber : palette::green;
            g.setColour (lit ? colour : colour.withAlpha (0.13f));
            g.fillRect (static_cast<float> (area.getX() + 1), y + 1.0f,
                        static_cast<float> (area.getWidth() - 2), juce::jmax (1.0f, segmentHeight - 2.0f));
        }
    }

    float left = -120.0f;
    float right = -120.0f;
};

class SC220MainComponent::LiveSpectrumDisplay final : public juce::Component
{
public:
    LiveSpectrumDisplay()
        : fft (fftOrder), window (fftSize, juce::dsp::WindowingFunction<float>::hann, true)
    {
        smoothed.fill (0.0f);
    }

    void updateFromTap (askp::ScopeTap& programmeTap, double sampleRate)
    {
        std::array<float, fftSize> left {}, right {};
        const int count = programmeTap.consumeLatest (left.data(), right.data(), fftSize);

        if (count <= 0 || sampleRate <= 0.0)
        {
            for (auto& value : smoothed)
                value *= 0.92f;
            hasSignal = std::any_of (smoothed.begin(), smoothed.end(), [] (float v) { return v > 0.015f; });
            repaint();
            return;
        }

        leftFftData.fill (0.0f);
        rightFftData.fill (0.0f);
        const int destinationStart = fftSize - count;
        float capturedPeak = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const auto leftSample = left[static_cast<size_t> (i)];
            const auto rightSample = right[static_cast<size_t> (i)];
            leftFftData[static_cast<size_t> (destinationStart + i)] = leftSample;
            rightFftData[static_cast<size_t> (destinationStart + i)] = rightSample;
            capturedPeak = juce::jmax (capturedPeak,
                                       juce::jmax (std::abs (leftSample), std::abs (rightSample)));
        }

        window.multiplyWithWindowingTable (leftFftData.data(), fftSize);
        window.multiplyWithWindowingTable (rightFftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (leftFftData.data());
        fft.performFrequencyOnlyForwardTransform (rightFftData.data());

        constexpr float minFrequency = 20.0f;
        constexpr float maxFrequency = 20000.0f;
        const float safeRate = static_cast<float> (sampleRate);
        for (size_t i = 0; i < smoothed.size(); ++i)
        {
            const float t0 = static_cast<float> (i) / static_cast<float> (smoothed.size());
            const float t1 = static_cast<float> (i + 1) / static_cast<float> (smoothed.size());
            const float frequency0 = minFrequency * std::pow (maxFrequency / minFrequency, t0);
            const float frequency1 = minFrequency * std::pow (maxFrequency / minFrequency, t1);
            const int bin0 = juce::jlimit (1, fftSize / 2 - 1,
                juce::roundToInt (frequency0 * fftSize / safeRate));
            const int bin1 = juce::jlimit (bin0 + 1, fftSize / 2,
                juce::roundToInt (frequency1 * fftSize / safeRate));

            float magnitude = 0.0f;
            for (int bin = bin0; bin < bin1; ++bin)
            {
                const auto leftMagnitude = leftFftData[static_cast<size_t> (bin)];
                const auto rightMagnitude = rightFftData[static_cast<size_t> (bin)];
                const auto stereoMagnitude = std::sqrt (0.5f
                    * (leftMagnitude * leftMagnitude + rightMagnitude * rightMagnitude));
                magnitude = juce::jmax (magnitude, stereoMagnitude);
            }

            const float db = juce::Decibels::gainToDecibels (magnitude / static_cast<float> (fftSize), -96.0f);
            const float next = normaliseDb (db, -84.0f);
            smoothed[i] = next > smoothed[i] ? next : smoothed[i] * 0.86f + next * 0.14f;
        }
        hasSignal = capturedPeak > juce::Decibels::decibelsToGain (-84.0f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::screen);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colour (0xff3d4040));
        g.drawRoundedRectangle (area, 7.0f, 1.0f);

        auto graph = area.reduced (34.0f, 24.0f);
        const auto labels = graph.toNearestInt();
        g.setColour (juce::Colours::white.withAlpha (0.065f));
        for (int i = 0; i <= 8; ++i)
        {
            const float x = graph.getX() + graph.getWidth() * static_cast<float> (i) / 8.0f;
            g.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
        }
        for (int i = 0; i <= 4; ++i)
        {
            const float y = graph.getY() + graph.getHeight() * static_cast<float> (i) / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
        }

        const float gap = 3.0f;
        const float barWidth = (graph.getWidth() - gap * static_cast<float> (smoothed.size() - 1))
                             / static_cast<float> (smoothed.size());
        for (size_t i = 0; i < smoothed.size(); ++i)
        {
            const float x = graph.getX() + static_cast<float> (i) * (barWidth + gap);
            const float activeHeight = juce::jmax (2.0f, smoothed[i] * graph.getHeight() * 0.94f);
            const auto inactive = juce::Rectangle<float> (
                x, graph.getY(), barWidth, graph.getHeight());
            const auto active = juce::Rectangle<float> (
                x, graph.getBottom() - activeHeight, barWidth, activeHeight);
            g.setColour (juce::Colours::white.withAlpha (0.055f));
            g.fillRoundedRectangle (inactive, juce::jmin (2.0f, barWidth * 0.35f));
            const float highBand = static_cast<float> (i) / static_cast<float> (smoothed.size() - 1);
            g.setColour (highBand > 0.78f ? palette::amber : palette::orange);
            g.fillRoundedRectangle (active, juce::jmin (2.0f, barWidth * 0.35f));
        }

        g.setColour (juce::Colours::white.withAlpha (0.66f));
        g.setFont (interFont (9.5f, true));
        g.drawText ("POST-MASTER ANALYZER", labels.getX(), juce::roundToInt (area.getY() + 6.0f),
                    180, 16, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white.withAlpha (0.40f));
        g.setFont (interFont (8.5f));
        g.drawText ("32 BAND  /  15 FPS", labels.getRight() - 150,
                    juce::roundToInt (area.getY() + 6.0f), 150, 16,
                    juce::Justification::centredRight);

        g.setColour (juce::Colours::white.withAlpha (0.56f));
        g.setFont (interFont (9.5f));
        g.drawText ("20", labels.getX(), labels.getBottom() + 4, 32, 16,
                    juce::Justification::centredLeft);
        g.drawText ("100", labels.getX() + labels.getWidth() / 4 - 18, labels.getBottom() + 4, 36, 16,
                    juce::Justification::centred);
        g.drawText ("1k", labels.getCentreX() - 18, labels.getBottom() + 4, 36, 16,
                    juce::Justification::centred);
        g.drawText ("10k", labels.getX() + labels.getWidth() * 3 / 4 - 20, labels.getBottom() + 4, 40, 16,
                    juce::Justification::centred);
        g.drawText ("20k", labels.getRight() - 38, labels.getBottom() + 4, 38, 16,
                    juce::Justification::centredRight);

        if (! hasSignal)
        {
            g.setColour (juce::Colours::white.withAlpha (0.42f));
            g.setFont (interFont (11.0f, true));
            g.drawText ("NO PROGRAM AUDIO", graph.toNearestInt(), juce::Justification::centred);
        }
    }

private:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> leftFftData {};
    std::array<float, fftSize * 2> rightFftData {};
    std::array<float, 32> smoothed {};
    bool hasSignal = false;
};

class SC220MainComponent::LoudnessDisplay final : public juce::Component
{
public:
    void setReading (float nextLufs, float nextTruePeak, float nextElapsedSeconds,
                     bool nextReady)
    {
        lufs = nextLufs;
        truePeak = nextTruePeak;
        elapsedSeconds = nextElapsedSeconds;
        ready = nextReady;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        const auto dial = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                                       juce::jmin (area.getWidth(), area.getHeight()));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (dial.translated (0.0f, 4.0f));
        juce::ColourGradient rim (palette::metalHi, dial.getCentreX(), dial.getY(),
                                  palette::metalLo, dial.getCentreX(), dial.getBottom(), false);
        rim.addColour (0.45, juce::Colour (0xffd1d2ce));
        g.setGradientFill (rim);
        g.fillEllipse (dial);
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.drawEllipse (dial, 1.2f);

        const auto face = dial.reduced (6.0f);
        juce::ColourGradient faceFill (juce::Colour (0xff262929), face.getCentreX(), face.getY(),
                                       juce::Colour (0xff0a0b0b), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (faceFill);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (face.reduced (1.0f), 1.0f);

        constexpr float start = juce::MathConstants<float>::pi * 0.72f;
        constexpr float end = juce::MathConstants<float>::pi * 2.28f;
        const float displayLufs = ready ? lufs : -36.0f;
        const float normal = juce::jlimit (0.0f, 1.0f, (displayLufs + 36.0f) / 30.0f);
        const float radius = face.getWidth() * 0.44f;
        const auto centre = dial.getCentre();
        for (int i = 0; i < 32; ++i)
        {
            const float t = static_cast<float> (i) / 31.0f;
            const float angle = start + t * (end - start);
            const auto colour = t > 0.83f ? palette::red : t > 0.62f ? palette::amber : palette::green;
            g.setColour (i <= juce::roundToInt (normal * 31.0f) ? colour : colour.withAlpha (0.16f));
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.83f, angle),
                          centre.getPointOnCircumference (radius, angle) }, 4.0f);
        }

        const auto needleAngle = start + normal * (end - start);
        const auto needleStart = centre.getPointOnCircumference (radius * 0.53f, needleAngle);
        const auto needleEnd = centre.getPointOnCircumference (radius * 1.02f, needleAngle);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawLine ({ needleStart.translated (1.0f, 1.5f),
                      needleEnd.translated (1.0f, 1.5f) }, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.drawLine ({ needleStart, needleEnd }, 2.0f);

        g.setColour (juce::Colours::white);
        g.setFont (interFont (dial.getWidth() * 0.20f, true));
        g.drawText (! ready || lufs <= -100.0f ? "--" : juce::String (lufs, 1),
                    dial.withTrimmedTop (dial.getHeight() * 0.28f).withHeight (dial.getHeight() * 0.25f),
                    juce::Justification::centred);
        g.setFont (interFont (dial.getWidth() * 0.070f, true));
        g.drawText ("LUFS-I", dial.withTrimmedTop (dial.getHeight() * 0.52f).withHeight (22.0f),
                    juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.74f));
        g.setFont (interFont (dial.getWidth() * 0.058f));
        const auto truePeakText = truePeak <= -100.0f
            ? juce::String ("-- dBTP") : juce::String (truePeak, 1) + " dBTP MAX";
        g.drawText (truePeakText,
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);
        g.setColour (palette::orange.withAlpha (0.92f));
        g.setFont (interFont (dial.getWidth() * 0.050f, true));
        g.drawText ("YT / TIKTOK  -14  |  TP <= -1",
                    dial.withTrimmedTop (dial.getHeight() * 0.14f).withHeight (18.0f),
                    juce::Justification::centred);
        if (! ready)
        {
            g.setColour (juce::Colours::white.withAlpha (0.58f));
            g.setFont (interFont (dial.getWidth() * 0.050f));
            const auto readinessText = elapsedSeconds < 3.0f
                ? "CALIBRATING  " + juce::String (elapsedSeconds, 1) + " / 3.0s"
                : juce::String ("WAITING FOR PROGRAM");
            g.drawText (readinessText,
                        dial.withTrimmedTop (dial.getHeight() * 0.78f).withHeight (18.0f),
                        juce::Justification::centred);
        }
    }

private:
    float lufs = -120.0f;
    float truePeak = -120.0f;
    float elapsedSeconds = 0.0f;
    bool ready = false;
};

SC220MainComponent::SC220MainComponent (LiveAudioEngine& engine, WindowActions actions)
    : audioEngine (engine), windowActions (std::move (actions)),
      lookAndFeel (std::make_unique<SurfaceLookAndFeel>()),
      sc220Meter (std::make_unique<StereoMeter>()),
      pcMeter (std::make_unique<StereoMeter>()),
      streamMeter (std::make_unique<StereoMeter>()),
      spectrumDisplay (std::make_unique<LiveSpectrumDisplay>()),
      loudnessDisplay (std::make_unique<LoudnessDisplay>()),
      licensePanel (std::make_unique<LiveLicensePanel> (engine.getEnhancerBridge()))
{
    setOpaque (true);
    setLookAndFeel (lookAndFeel.get());
    recordingTechLogo = loadImage ("recording_tech_logo_png");
    sc220Image = loadImage ("sc220_mkii_device_alpha_png");

    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);

    configureControls();
    refreshAllControls();
    startTimerHz (30);
}

SC220MainComponent::~SC220MainComponent()
{
    stopTimer();
    voiceFxEditorWindow.reset(); // Editor must die before its VST3 AudioProcessor.
    voiceFxChooser.reset();
    audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (false);
    audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (false);
    setLookAndFeel (nullptr);
}

void SC220MainComponent::configureControls()
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &minimiseButton, &maximiseButton, &closeButton, &settingsButton,
             &enhancerPowerButton, &goLiveButton, &voiceFxButton, &presetBox, &outputBox,
             &stereoButton, &dualMonoButton, &pcSourceBox, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob,
             &sc220Mute, &streamMute })
        addAndMakeVisible (component);

    for (auto& knob : macroKnobs)
        addAndMakeVisible (knob);

    for (auto* component : std::initializer_list<juce::Component*> {
             sc220Meter.get(), pcMeter.get(), streamMeter.get(), spectrumDisplay.get(),
             loudnessDisplay.get() })
        addAndMakeVisible (component);
    sc220Meter->setInterceptsMouseClicks (false, false);
    pcMeter->setInterceptsMouseClicks (false, false);
    streamMeter->setInterceptsMouseClicks (false, false);
    addChildComponent (*licensePanel);

    minimiseButton.setTooltip ("Minimise");
    maximiseButton.setTooltip ("Maximise / Restore");
    closeButton.setTooltip ("Close to tray");
    settingsButton.setTooltip ("Audio Setup (WASAPI / ASIO) and License");
    voiceFxButton.setTooltip ("Voice VST3 insert: choose ArSonVerb or another trusted stereo effect");
    voiceFxButton.onClick = [this] { showVoiceFxMenu(); };
    enhancerPowerButton.setTooltip ("Smart Enhancer On / Bypass");
    enhancerPowerButton.setIconColour (palette::orange);
    enhancerPowerButton.setOffColour (palette::muted);

    minimiseButton.onClick = [this] { if (windowActions.minimise) windowActions.minimise(); };
    maximiseButton.onClick = [this] { if (windowActions.toggleMaximise) windowActions.toggleMaximise(); };
    closeButton.onClick = [this] { if (windowActions.hideToTray) windowActions.hideToTray(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };

    licensePanel->onClose = [this] { licensePanel->setVisible (false); };
    licensePanel->onLicenseChanged = [this] { refreshAllControls(); };

    enhancerPowerButton.setClickingTogglesState (true);
    enhancerPowerButton.onClick = [this]
    {
        audioEngine.setBypassed (! enhancerPowerButton.getToggleState());
        repaint (signatureArea);
    };

    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
        if (goLiveButton.getToggleState())
            audioEngine.resetProgrammeLoudness();
        audioEngine.setOutputMuted (! goLiveButton.getToggleState());
        goLiveButton.setButtonText (goLiveButton.getToggleState() ? "LIVE" : "GO LIVE");
    };

    presetBox.addItemList (audioEngine.getPresetNames(), 1);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedItemIndex() >= 0)
            audioEngine.setPresetIndex (presetBox.getSelectedItemIndex());
    };

    outputBox.addItemList (audioEngine.getOutputDeviceNames(), 1);
    outputBox.setTextWhenNothingSelected ("SELECT VB-CABLE");
    outputBox.onChange = [this]
    {
        if (outputBox.getSelectedItemIndex() < 0)
            return;
        const auto error = audioEngine.selectOutputDevice (outputBox.getText());
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Output unavailable", error);
    };

    stereoButton.setClickingTogglesState (true);
    dualMonoButton.setClickingTogglesState (true);
    stereoButton.setRadioGroupId (701);
    dualMonoButton.setRadioGroupId (701);
    stereoButton.onClick = [this] { audioEngine.setInputChannelMode (2); };
    dualMonoButton.onClick = [this] { audioEngine.setInputChannelMode (1); };

    pcSourceBox.addItem ("SYSTEM MIX", 1);
    pcSourceBox.setSelectedId (1, juce::dontSendNotification);

    auto configureKnob = [] (juce::Slider& slider, double minimum, double maximum,
                             double interval, const juce::String& suffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRange (minimum, maximum, interval);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        slider.setTextValueSuffix (suffix);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.setMouseDragSensitivity (180);
    };
    auto configureFader = [] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setRange (-60.0, 12.0, 0.1);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue (true, 0.0);
    };

    configureKnob (inputTrim, -12.0, 12.0, 0.1, " dB");
    configureKnob (duckAmount, 0.0, 24.0, 0.5, " dB");
    configureKnob (monitorKnob, 0.0, 10.0, 0.1, "");
    configureFader (sc220Fader);
    configureFader (pcFader);
    configureFader (streamFader);
    for (auto& knob : macroKnobs)
    {
        configureKnob (knob, 0.0, 100.0, 0.1, "%");
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }

    inputTrim.onValueChange = [this] { audioEngine.setInputTrimDb (static_cast<float> (inputTrim.getValue())); };
    sc220Fader.onValueChange = [this] { audioEngine.setSc220FaderDb (static_cast<float> (sc220Fader.getValue())); };
    pcFader.onValueChange = [this] { audioEngine.setPcAudioGainDb (static_cast<float> (pcFader.getValue())); };
    duckAmount.onValueChange = [this]
    {
        audioEngine.setDuckingDepthDb (-static_cast<float> (duckAmount.getValue()));
        audioEngine.setDuckingEnabled (duckAmount.getValue() > 0.0);
    };
    streamFader.onValueChange = [this] { audioEngine.setOutputTrimDb (static_cast<float> (streamFader.getValue())); };

    macroKnobs[0].onValueChange = [this] { audioEngine.setEnhance (static_cast<float> (macroKnobs[0].getValue())); };
    macroKnobs[1].onValueChange = [this] { audioEngine.setSmartBass (static_cast<float> (macroKnobs[1].getValue())); };
    macroKnobs[2].onValueChange = [this] { audioEngine.setSmartTreble (static_cast<float> (macroKnobs[2].getValue())); };
    macroKnobs[3].onValueChange = [this] { audioEngine.setVocalBody (static_cast<float> (macroKnobs[3].getValue())); };
    macroKnobs[4].onValueChange = [this] { audioEngine.setStereoMagic (static_cast<float> (macroKnobs[4].getValue())); };

    sc220Mute.setClickingTogglesState (true);
    sc220Mute.onClick = [this] { audioEngine.setSc220Muted (sc220Mute.getToggleState()); };
    streamMute.setClickingTogglesState (true);
    streamMute.onClick = [this] { audioEngine.setOutputMuted (streamMute.getToggleState()); };

    monitorKnob.setEnabled (false);
    monitorKnob.setTooltip ("Physical monitor output is not active yet.");
}

void SC220MainComponent::refreshAllControls()
{
    auto& license = audioEngine.getLicenseManager();
    license.refresh();
    const bool creativeEditing = license.editingAllowed();
    presetBox.setEnabled (creativeEditing);
    for (auto& knob : macroKnobs)
        knob.setEnabled (creativeEditing);

    presetBox.setSelectedItemIndex (audioEngine.getPresetIndex(), juce::dontSendNotification);
    inputTrim.setValue (audioEngine.getInputTrimDb(), juce::dontSendNotification);
    sc220Fader.setValue (audioEngine.getSc220FaderDb(), juce::dontSendNotification);
    pcFader.setValue (audioEngine.getPcAudioGainDb(), juce::dontSendNotification);
    duckAmount.setValue (-audioEngine.getDuckingDepthDb(), juce::dontSendNotification);
    streamFader.setValue (audioEngine.getOutputTrimDb(), juce::dontSendNotification);
    macroKnobs[0].setValue (audioEngine.getEnhance(), juce::dontSendNotification);
    macroKnobs[1].setValue (audioEngine.getSmartBass(), juce::dontSendNotification);
    macroKnobs[2].setValue (audioEngine.getSmartTreble(), juce::dontSendNotification);
    macroKnobs[3].setValue (audioEngine.getVocalBody(), juce::dontSendNotification);
    macroKnobs[4].setValue (audioEngine.getStereoMagic(), juce::dontSendNotification);

    const bool stereo = audioEngine.getInputChannelMode() == 2;
    stereoButton.setToggleState (stereo, juce::dontSendNotification);
    dualMonoButton.setToggleState (! stereo, juce::dontSendNotification);
    sc220Mute.setToggleState (audioEngine.isSc220Muted(), juce::dontSendNotification);
    streamMute.setToggleState (audioEngine.isOutputMuted(), juce::dontSendNotification);
    enhancerPowerButton.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    goLiveButton.setToggleState (! audioEngine.isOutputMuted(), juce::dontSendNotification);
    goLiveButton.setButtonText (audioEngine.isOutputMuted() ? "GO LIVE" : "LIVE");

    const auto fxLabel = audioEngine.hasVoiceVst3()
        ? (audioEngine.isVoiceVst3Bypassed() ? juce::String ("FX BYPASS")
            : juce::String ("FX ") + audioEngine.getVoiceVst3Name())
        : juce::String ("+ VST3 FX");
    if (voiceFxButton.getButtonText() != fxLabel)
        voiceFxButton.setButtonText (fxLabel);
    const auto status = audioEngine.getStatus();
    const auto outputIndex = audioEngine.getOutputDeviceNames().indexOf (status.outputDevice);
    if (outputIndex >= 0)
        outputBox.setSelectedItemIndex (outputIndex, juce::dontSendNotification);
}

void SC220MainComponent::setUiActive (bool active)
{
    uiActive = active;
    if (active) startTimerHz (30); else stopTimer();
}

void SC220MainComponent::timerCallback()
{
    static int licenceRefreshDivider = 0;
    if (++licenceRefreshDivider >= 30)
    {
        licenceRefreshDivider = 0;
        refreshAllControls();
    }

    lastStatus = audioEngine.getStatus();
    sc220Meter->setLevels (lastStatus.inputPeakDbL, lastStatus.inputPeakDbR);
    pcMeter->setLevels (lastStatus.pcPeakDbL, lastStatus.pcPeakDbR);
    streamMeter->setLevels (lastStatus.outputPeakDbL, lastStatus.outputPeakDbR);
    loudnessDisplay->setReading (lastStatus.loudnessLufs, lastStatus.truePeakDbtp,
                                 lastStatus.loudnessElapsedSeconds,
                                 lastStatus.loudnessReady);
    spectrumDisplay->updateFromTap (audioEngine.getVoiceEnhancerBridge().analysisTap(),
                                    lastStatus.sampleRate);
    repaint();
}

void SC220MainComponent::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Audio Setup (WASAPI / ASIO)");
    menu.addItem (2, "License & Activation");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
                        {
                            if (result == 1)
                                showAudioSettings();
                            else if (result == 2)
                            {
                                licensePanel->setVisible (true);
                                licensePanel->toFront (true);
                            }
                        });
}

class VoiceFxEditorWindow final : public juce::DocumentWindow
{
public:
    VoiceFxEditorWindow (const juce::String& title, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow (title, juce::Colours::darkgrey,
                                juce::DocumentWindow::closeButton, true)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (false, false);
        centreWithSize (juce::jmax (350, editor->getWidth()),
                        juce::jmax (250, editor->getHeight()));
    }

    void closeButtonPressed() override { setVisible (false); }
};

void SC220MainComponent::showVoiceFxMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Load VST3 effect...");
    const bool loaded = audioEngine.hasVoiceVst3();
    menu.addItem (2, "Open plugin editor", loaded);
    menu.addItem (3, audioEngine.isVoiceVst3Bypassed() ? "Enable effect" : "Bypass effect", loaded);
    menu.addItem (4, "Remove effect", loaded);
    juce::Component::SafePointer<SC220MainComponent> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&voiceFxButton),
                        [safe] (int result)
                        {
                            if (safe == nullptr) return;
                            if (result == 1) safe->chooseVoiceVst3();
                            if (result == 2) safe->showVoiceVst3Editor();
                            if (result == 3)
                                safe->audioEngine.setVoiceVst3Bypassed (! safe->audioEngine.isVoiceVst3Bypassed());
                            if (result == 4)
                            {
                                safe->voiceFxEditorWindow.reset();
                                safe->audioEngine.removeVoiceVst3();
                            }
                            if (result == 3 || result == 4) safe->refreshAllControls();
                        });
}

void SC220MainComponent::chooseVoiceVst3()
{
    voiceFxChooser = std::make_unique<juce::FileChooser> (
        "Choose a trusted VST3 effect (ArSonVerb)", juce::File(), "*.vst3");
    juce::Component::SafePointer<SC220MainComponent> safe (this);
    voiceFxChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::canSelectDirectories,
                                [safe] (const juce::FileChooser& chooser)
                                {
                                    if (safe == nullptr) return;
                                    const auto file = chooser.getResult();
                                    if (file == juce::File()) return;
                                    safe->voiceFxEditorWindow.reset();
                                    const auto error = safe->audioEngine.loadVoiceVst3 (file);
                                    if (error.isNotEmpty())
                                        juce::AlertWindow::showMessageBoxAsync (
                                            juce::MessageBoxIconType::WarningIcon, "VST3 unavailable", error);
                                    safe->refreshAllControls();
                                });
}

void SC220MainComponent::showVoiceVst3Editor()
{
    if (auto* processor = audioEngine.getVoiceVst3Processor())
    {
        if (voiceFxEditorWindow == nullptr)
        {
            auto* editor = processor->createEditorIfNeeded();
            if (editor == nullptr)
                editor = new juce::GenericAudioProcessorEditor (*processor);
            voiceFxEditorWindow = std::make_unique<VoiceFxEditorWindow> (
                audioEngine.getVoiceVst3Name(), editor);
        }
        voiceFxEditorWindow->setVisible (true);
        voiceFxEditorWindow->toFront (true);
    }
}

void SC220MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        audioEngine.getDeviceManager(), 1, 2, 1, 2, false, false, true, false);
    selector->setSize (720, 520);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "SC220 Live Audio Setup";
    options.dialogBackgroundColour = palette::ivory;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

bool SC220MainComponent::looksLikeSc220Input (const juce::String& name) noexcept
{
    const auto normalised = name.trim().toUpperCase();
    return normalised == "MIXER"
        || normalised.startsWith ("MIXER (")
        || normalised.contains ("MIXER-01")
        || normalised.contains ("SC220")
        || normalised.contains ("RECORDING TECH");
}

bool SC220MainComponent::headerDragAllowedAt (juce::Point<int> point) const noexcept
{
    if (! headerArea.contains (point))
        return false;

    for (const auto* control : std::initializer_list<const juce::Component*> {
             &presetBox, &settingsButton, &minimiseButton, &maximiseButton,
             &closeButton, &goLiveButton })
        if (control->getBounds().contains (point))
            return false;

    return true;
}

void SC220MainComponent::mouseDown (const juce::MouseEvent& event)
{
    draggingWindow = headerDragAllowedAt (event.getPosition());
    if (draggingWindow)
        if (auto* top = getTopLevelComponent())
            windowDragger.startDraggingComponent (top, event);
}

void SC220MainComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingWindow)
        if (auto* top = getTopLevelComponent())
            windowDragger.dragComponent (top, event, nullptr);
}

void SC220MainComponent::mouseUp (const juce::MouseEvent&)
{
    draggingWindow = false;
}

void SC220MainComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (headerDragAllowedAt (event.getPosition()) && windowActions.toggleMaximise)
        windowActions.toggleMaximise();
}

void SC220MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (palette::header);
    drawHeader (g);
    drawInputSurface (g);
    drawSignatureSurface (g);
    drawMasterSurface (g);
    drawFooter (g);
}

void SC220MainComponent::drawHeader (juce::Graphics& g)
{
    g.setColour (palette::header);
    g.fillRect (headerArea);

    if (recordingTechLogo.isValid())
        g.drawImageWithin (recordingTechLogo, headerArea.getX() + 14, headerArea.getY() + 13,
                           145, 38, juce::RectanglePlacement::centred, false);

    g.setColour (juce::Colours::white);
    g.setFont (interFont (18.0f, true));
    g.drawText ("SC220", 176, 0, 70, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (palette::orange);
    g.drawText ("LIVE", 244, 0, 52, headerArea.getHeight(), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    for (int x : { 164, 320, 548, 716 })
        g.drawVerticalLine (x, 14.0f, static_cast<float> (headerArea.getBottom() - 14));

    g.setColour (juce::Colours::white.withAlpha (0.88f));
    g.setFont (interFont (11.5f));
    g.drawText (juce::String (lastStatus.sampleRate > 0.0 ? lastStatus.sampleRate / 1000.0 : 48.0, 0) + " kHz",
                562, 0, 72, headerArea.getHeight(), juce::Justification::centred);
    g.drawText (juce::String (lastStatus.bufferSize > 0 ? lastStatus.bufferSize : 128) + " samples",
                630, 0, 92, headerArea.getHeight(), juce::Justification::centred);

    const bool sc220Detected = looksLikeSc220Input (lastStatus.inputDevice);
    const bool stable = sc220Detected && lastStatus.deviceOpen && lastStatus.outputIsVirtualCable
                     && lastStatus.pcLoopbackRunning && lastStatus.lastError.isEmpty();
    g.setColour (stable ? palette::green : palette::amber);
    g.fillEllipse (746.0f, 29.0f, 8.0f, 8.0f);
    g.setFont (interFont (11.5f, true));
    g.drawText (stable ? "STABLE" : "SETUP", 760, 0, 70, headerArea.getHeight(),
                juce::Justification::centredLeft);
}

void SC220MainComponent::drawInputSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (inputArea);
    g.setColour (palette::border);
    g.drawVerticalLine (inputArea.getRight() - 1, static_cast<float> (inputArea.getY()),
                        static_cast<float> (inputArea.getBottom()));

    g.setColour (palette::ink);
    g.setFont (interFont (15.0f, true));
    g.drawText ("INPUT MIX", inputArea.withHeight (50), juce::Justification::centred);
    g.setColour (palette::borderSoft);
    g.drawVerticalLine (sc220StripArea.getRight(), static_cast<float> (sc220StripArea.getY() + 10),
                        static_cast<float> (sc220StripArea.getBottom() - 10));

    const bool sc220Detected = looksLikeSc220Input (lastStatus.inputDevice);
    if (sc220Image.isValid())
    {
        g.setOpacity (sc220Detected ? 1.0f : 0.32f);
        const int sourceX = 36;
        const int sourceY = juce::roundToInt (static_cast<float> (sc220Image.getHeight()) * 0.245f);
        const int sourceWidth = sc220Image.getWidth() - sourceX * 2;
        const int sourceHeight = juce::jmin (
            sc220Image.getHeight() - sourceY,
            juce::roundToInt (static_cast<float> (sc220Image.getHeight()) * 0.59f));
        g.drawImage (sc220Image,
                     sc220StripArea.getCentreX() - 72, sc220StripArea.getY() + 6, 144, 82,
                     sourceX, sourceY, sourceWidth, sourceHeight, false);
        g.setOpacity (1.0f);
    }

    g.setColour (sc220Detected ? palette::green : palette::red);
    g.fillEllipse (static_cast<float> (sc220StripArea.getCentreX() - 80),
                   static_cast<float> (sc220StripArea.getY() + 91), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (10.0f, true));
    g.drawText (sc220Detected ? "SC220 CONNECTED" : "SC220 NOT DETECTED",
                sc220StripArea.getCentreX() - 66, sc220StripArea.getY() + 84,
                150, 22, juce::Justification::centredLeft);
    if (! sc220Detected && lastStatus.inputDevice.isNotEmpty())
    {
        g.setColour (palette::muted);
        g.setFont (interFont (8.5f));
        g.drawFittedText ("ACTIVE: " + lastStatus.inputDevice,
                          sc220StripArea.getX() + 18, sc220StripArea.getY() + 105,
                          sc220StripArea.getWidth() - 36, 18,
                          juce::Justification::centred, 1);
    }

    g.setColour (palette::graphite);
    g.setFont (interFont (11.5f, true));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 27,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
    g.setColour (lastStatus.pcLoopbackRunning ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 73),
                   static_cast<float> (pcStripArea.getY() + 70), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (9.8f, true));
    g.drawText (lastStatus.pcLoopbackRunning ? "LOOPBACK ACTIVE" : "LOOPBACK STARTING",
                pcStripArea.getCentreX() - 58, pcStripArea.getY() + 63, 142, 22,
                juce::Justification::centredLeft);

    drawFaderScale (g, sc220Fader.getBounds());
    drawFaderScale (g, pcFader.getBounds());

    g.setColour (palette::muted);
    g.setFont (interFont (9.0f, true));
    g.drawText ("INPUT TRIM", inputTrim.getX() - 14, inputTrim.getY() - 22,
                inputTrim.getWidth() + 28, 18, juce::Justification::centred);
    g.drawText ("SC220", sc220Fader.getX() - 15, sc220Fader.getBottom() + 2,
                sc220Fader.getWidth() + 30, 18, juce::Justification::centred);
    g.drawText ("PC AUDIO", pcFader.getX() - 20, pcFader.getBottom() + 2,
                pcFader.getWidth() + 40, 18, juce::Justification::centred);
    g.drawText ("DUCK", duckAmount.getX() - 10, duckAmount.getY() - 17,
                duckAmount.getWidth() + 20, 18, juce::Justification::centred);

    const bool ducking = audioEngine.isDuckingEnabled();
    g.setColour (ducking ? palette::green : palette::muted);
    g.fillEllipse (static_cast<float> (duckAmount.getRight() + 8),
                   static_cast<float> (duckAmount.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setColour (palette::ink);
    g.setFont (interFont (8.8f, true));
    g.drawText ("KEY: SC220", duckAmount.getRight() + 20, duckAmount.getY(),
                82, duckAmount.getHeight(), juce::Justification::centredLeft);
}

void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)
{
    g.setColour (palette::ivory);
    g.fillRect (signatureArea);
    g.setColour (palette::ink);
    g.setFont (interFont (16.0f, true));
    g.drawText ("SMART ENHANCER", signatureArea.getX(), signatureArea.getY() + 20,
                signatureArea.getWidth(), 24, juce::Justification::centred);
    g.setColour (palette::muted);
    g.setFont (interFont (9.5f, true));
    g.drawText ("MAS ARI SIGNATURE ENGINE", signatureArea.getX(), signatureArea.getY() + 42,
                signatureArea.getWidth(), 18, juce::Justification::centred);

    const bool active = ! audioEngine.isBypassed();
    g.setColour (active ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 56),
                   static_cast<float> (enhancerPowerButton.getBounds().getCentreY() - 4), 8.0f, 8.0f);
    g.setFont (interFont (9.0f, true));
    g.drawText (active ? "ON" : "BYPASS", enhancerPowerButton.getX() - 44,
                enhancerPowerButton.getY(), 42, enhancerPowerButton.getHeight(),
                juce::Justification::centredLeft);

    static constexpr const char* names[] = {
        "ENHANCE", "SMART BASS", "SMART TREBLE", "VOCAL BODY", "STEREO MAGIC"
    };
    for (size_t i = 0; i < macroKnobs.size(); ++i)
    {
        const auto& knob = macroKnobs[i];
        g.setColour (palette::ink);
        g.setFont (interFont (10.5f, true));
        g.drawText (names[i], knob.getX() - 16, knob.getY() - 25, knob.getWidth() + 32, 18,
                    juce::Justification::centred);
        g.setFont (interFont (14.0f));
        g.drawText (juce::String (knob.getValue(), 0) + "%", knob.getX(), knob.getY() - 6,
                    knob.getWidth(), 22, juce::Justification::centred);
        g.setColour (palette::graphite);
        g.setFont (interFont (10.0f));
        g.drawText ("0%", knob.getX() - 9, knob.getBottom() - 16, 30, 16,
                    juce::Justification::centredLeft);
        g.drawText ("100%", knob.getRight() - 28, knob.getBottom() - 16, 38, 16,
                    juce::Justification::centredRight);
    }

    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (signatureArea.getCentreX() - 94),
                   static_cast<float> (signatureArea.getBottom() - 42), 8.0f, 8.0f);
    g.setFont (interFont (9.5f, true));
    g.drawText ("SMART PROTECT  /  ON", signatureArea.getCentreX() - 80,
                signatureArea.getBottom() - 52, 170, 28, juce::Justification::centredLeft);
}

void SC220MainComponent::drawMasterSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (masterArea);
    g.setColour (palette::border);
    g.drawVerticalLine (masterArea.getX(), static_cast<float> (masterArea.getY()),
                        static_cast<float> (masterArea.getBottom()));
    drawFaderScale (g, streamFader.getBounds());
    g.setColour (palette::ink);
    g.setFont (interFont (11.5f, true));
    g.drawText ("LOUDNESS", masterArea.getX(), masterArea.getY() + 20,
                masterArea.getWidth(), 22, juce::Justification::centred);
    g.drawText ("STREAM", streamFader.getX() - 25, streamFader.getY() - 28,
                streamFader.getWidth() + 50, 22, juce::Justification::centred);
    g.drawText ("MONITOR", monitorKnob.getX() - 16, monitorKnob.getY() - 24,
                monitorKnob.getWidth() + 32, 22, juce::Justification::centred);
    g.setColour (palette::muted);
    g.setFont (interFont (9.0f, true));
    g.drawText ("STREAM OUTPUT", outputBox.getX(), outputBox.getY() - 21,
                outputBox.getWidth(), 18, juce::Justification::centred);
}

void SC220MainComponent::drawFooter (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (footerArea);
    g.setColour (palette::border);
    g.drawHorizontalLine (footerArea.getY(), 0.0f, static_cast<float> (getWidth()));
    g.setColour (palette::ink);
    g.setFont (interFont (10.5f, true));

    const auto pathText = "SC220 + PC AUDIO    >    VOICE / MUSIC SIGNATURE    >    VB-CABLE";
    g.drawText (pathText, footerArea.getX() + 42, footerArea.getY(), 560,
                footerArea.getHeight(), juce::Justification::centredLeft);

    const float latencyMs = lastStatus.sampleRate > 0.0
        ? static_cast<float> (lastStatus.totalLatencySamples / lastStatus.sampleRate * 1000.0)
        : 0.0f;
    const juce::String health = "CPU  " + juce::String (lastStatus.cpuPercent, 1) + "%      "
                              + "DROP  " + juce::String (lastStatus.dropouts) + "      "
                              + "LATENCY  " + juce::String (latencyMs, 1) + " ms";
    g.drawText (health, footerArea.getCentreX(), footerArea.getY(),
                footerArea.getWidth() / 2 - 42, footerArea.getHeight(),
                juce::Justification::centredRight);
}

void SC220MainComponent::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (66);
    footerArea = area.removeFromBottom (58);
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.325f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.455f);
    inputArea = area.removeFromLeft (inputWidth);
    signatureArea = area.removeFromLeft (signatureWidth);
    masterArea = area;

    closeButton.setBounds (getWidth() - 48, 13, 38, 38);
    maximiseButton.setBounds (getWidth() - 90, 13, 38, 38);
    minimiseButton.setBounds (getWidth() - 132, 13, 38, 38);
    settingsButton.setBounds (getWidth() - 184, 13, 38, 38);
    goLiveButton.setBounds (getWidth() - 322, 10, 126, 46);
    presetBox.setBounds (338, 15, 192, 36);

    auto inputContent = inputArea.reduced (18, 48);
    sc220StripArea = inputContent.removeFromLeft (inputContent.getWidth() / 2);
    pcStripArea = inputContent;

    stereoButton.setBounds (sc220StripArea.getX() + 10, sc220StripArea.getY() + 126,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 46, sc220StripArea.getY() + 186, 92, 92);
    sc220Meter->setBounds (sc220StripArea.getX() + 18, sc220StripArea.getY() + 306,
                           30, sc220StripArea.getHeight() - 392);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - 34, sc220StripArea.getY() + 302,
                          68, sc220StripArea.getHeight() - 388);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36, sc220StripArea.getBottom() - 42, 72, 31);

    pcSourceBox.setBounds (pcStripArea.getX() + 30, pcStripArea.getY() + 104,
                           pcStripArea.getWidth() - 60, 34);
    pcMeter->setBounds (pcStripArea.getX() + 18, pcStripArea.getY() + 218,
                        30, pcStripArea.getHeight() - 304);
    pcFader.setBounds (pcStripArea.getCentreX() - 34, pcStripArea.getY() + 214,
                       68, pcStripArea.getHeight() - 300);
    duckAmount.setBounds (pcStripArea.getX() + 24, pcStripArea.getBottom() - 82, 70, 70);

    // Outside the console-grid replacement anchor; retained in the generated production UI.
    voiceFxButton.setBounds (sc220StripArea.getCentreX() - 74, sc220StripArea.getY() + 160, 148, 22);
    presetBox.setBounds (signatureArea.getCentreX() - 102, signatureArea.getY() + 72, 204, 36);
    enhancerPowerButton.setBounds (signatureArea.getRight() - 54, signatureArea.getY() + 20, 36, 36);
    spectrumDisplay->setBounds (signatureArea.reduced (34, 0).withTop (signatureArea.getY() + 128)
                                .withBottom (signatureArea.getY() + juce::roundToInt (signatureArea.getHeight() * 0.56f)));

    auto macroRow = signatureArea.reduced (24, 0);
    macroRow.setTop (spectrumDisplay->getBottom() + 52);
    macroRow.setBottom (signatureArea.getBottom() - 62);
    const int macroWidth = macroRow.getWidth() / static_cast<int> (macroKnobs.size());
    for (auto& knob : macroKnobs)
    {
        auto slot = macroRow.removeFromLeft (macroWidth);
        const int knobWidth = juce::jmin (88, slot.getWidth() - 12);
        const int knobHeight = juce::jmin (132, slot.getHeight());
        knob.setBounds (slot.withSizeKeepingCentre (knobWidth, knobHeight));
    }

    loudnessDisplay->setBounds (masterArea.getCentreX() - 100, masterArea.getY() + 48, 200, 200);
    auto masterLower = masterArea.reduced (24, 0);
    masterLower.setTop (masterArea.getY() + 300);
    streamMeter->setBounds (masterLower.getX(), masterLower.getY() + 42, 32,
                            masterLower.getHeight() - 140);
    streamFader.setBounds (masterLower.getX() + 40, masterLower.getY() + 38, 72,
                           masterLower.getHeight() - 134);
    streamMute.setBounds (streamFader.getBounds().getCentreX() - 36, masterArea.getBottom() - 44, 72, 31);
    monitorKnob.setBounds (masterArea.getRight() - 164, masterLower.getY() + 52, 108, 108);
    outputBox.setBounds (masterArea.getRight() - 184, masterArea.getBottom() - 108, 154, 36);

    licensePanel->setBounds (getLocalBounds());
}

} // namespace askp::live
