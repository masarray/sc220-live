#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"

namespace sc220::visual
{
inline const juce::Colour canvas { 0xff10151a };
inline const juce::Colour header { 0xff090d11 };
inline const juce::Colour panel { 0xff151b21 };
inline const juce::Colour panelHi { 0xff1d252d };
inline const juce::Colour inset { 0xff0b1117 };
inline const juce::Colour border { 0xff36434d };
inline const juce::Colour borderSoft { 0xff27323a };
inline const juce::Colour text { 0xffedf3f6 };
inline const juce::Colour muted { 0xff84919a };
inline const juce::Colour cyan { 0xff35c4d8 };
inline const juce::Colour orange { 0xffe8792f };
inline const juce::Colour green { 0xff3ac985 };
inline const juce::Colour amber { 0xffe4b34b };
inline const juce::Colour red { 0xffe05b55 };

inline juce::Typeface::Ptr typeface (const char* name)
{
    int bytes = 0;
    const auto* source = BinaryData::getNamedResource (name, bytes);
    return source != nullptr && bytes > 0
         ? juce::Typeface::createSystemTypefaceFor (source, static_cast<size_t>(bytes))
         : juce::Typeface::Ptr {};
}
inline juce::Font font (float height, bool bold = false)
{
    static auto regular = typeface ("inter_regular_ttf");
    static auto semi = typeface ("inter_semibold_ttf");
    if (auto face = bold ? semi : regular)
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}
inline juce::Image image (const char* resource)
{
    int bytes = 0;
    const auto* source = BinaryData::getNamedResource (resource, bytes);
    return source != nullptr && bytes > 0
         ? juce::ImageCache::getFromMemory (source, bytes) : juce::Image {};
}

inline void panelSurface (juce::Graphics& g, juce::Rectangle<float> box, float radius = 7.0f)
{
    juce::ColourGradient fill (panelHi, box.getX(), box.getY(),
                               panel, box.getRight(), box.getBottom(), false);
    fill.addColour (0.52, panel);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (box, radius);
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawRoundedRectangle (box.reduced (0.5f), radius, 1.0f);
    g.setColour (borderSoft.withAlpha (0.80f));
    g.drawHorizontalLine (juce::roundToInt (box.getY() + 1),
                          box.getX() + 10.0f, box.getRight() - 10.0f);
}
inline void caption (juce::Graphics& g, const juce::String& str,
                     juce::Rectangle<int> area, float size = 11.0f,
                     juce::Colour tint = muted, bool bold = true,
                     juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour (tint);
    g.setFont (font (size, bold));
    g.drawFittedText (str, area, align, 1);
}

class Look final : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, cyan);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, inset);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff176d7c));
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, inset);
        setColour (juce::ComboBox::outlineColourId, border);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return font (juce::jlimit (10.0f, 12.0f, height * 0.38f), true);
    }
    juce::Font getLabelFont (juce::Label& label) override
    {
        return font (juce::jlimit (10.0f, 14.0f, label.getFont().getHeight()), false);
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                const juce::Colour&, bool hover, bool down) override
    {
        auto area = button.getLocalBounds().toFloat().reduced (0.6f);
        const bool active = button.getToggleState();
        const bool live = button.getButtonText() == "GO LIVE" || button.getButtonText() == "ON AIR";
        auto base = active ? (live ? juce::Colour(0xff196d55) : juce::Colour(0xff19545c))
                           : (live ? juce::Colour(0xff5c301f) : panelHi);
        if (hover) base = base.brighter (0.16f);
        if (down) base = base.darker (0.13f);
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.2f), 5.0f);
        juce::ColourGradient grad (base.brighter(0.08f), area.getCentreX(), area.getY(),
                                   base.darker(0.18f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour ((active ? (live ? green : cyan) : border).withAlpha(0.80f));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        g.drawHorizontalLine (juce::roundToInt(area.getY() + 1.5f),
            area.getX() + 6.0f, area.getRight() - 6.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        g.setColour (button.getToggleState() ? juce::Colours::white : text);
        g.setFont (font (juce::jlimit(10.0f, 12.0f, button.getHeight() * 0.35f), true));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced(3,1),
                          juce::Justification::centred, 1);
    }
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider(g,x,y,w,h,pos,minPos,maxPos,style,slider);
            return;
        }
        const auto cx = static_cast<float>(x + w / 2);
        const auto track = juce::Rectangle<float>(cx - 5.0f, y + 11.0f,
                                                  10.0f, h - 22.0f);
        g.setColour (inset);
        g.fillRoundedRectangle (track.expanded(1.2f), 4.0f);
        g.setColour (border);
        g.drawRoundedRectangle (track, 4.0f, 0.8f);
        auto filled = track.withTop (pos);
        if (filled.getHeight() > 1)
        {
            g.setGradientFill (juce::ColourGradient(cyan, cx, filled.getY(),
                                                    juce::Colour(0xff246a86), cx,
                                                    filled.getBottom(), false));
            g.fillRoundedRectangle(filled, 3.5f);
        }
        const auto thumb = juce::Rectangle<float>(cx - 21.0f, pos - 13.0f, 42.0f, 26.0f);
        g.setColour(juce::Colours::black.withAlpha(0.50f));
        g.fillRoundedRectangle(thumb.translated(0,2.5f),3.0f);
        juce::ColourGradient metal (juce::Colour(0xffd8dcdd), thumb.getCentreX(),thumb.getY(),
                                   juce::Colour(0xff626e75), thumb.getCentreX(),thumb.getBottom(),false);
        metal.addColour(0.48,juce::Colour(0xffa6b0b5));
        g.setGradientFill(metal);
        g.fillRoundedRectangle(thumb,3.0f);
        g.setColour(juce::Colour(0xff3a434a));
        g.drawRoundedRectangle(thumb,3.0f,0.9f);
        g.drawLine(thumb.getX()+10,thumb.getCentreY(),thumb.getRight()-10,thumb.getCentreY(),1.4f);
    }
};

class Meter final : public juce::Component
{
public:
    void setLevels (float l, float r)
    {
        left = l; right = r;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().reduced(1).toFloat();
        const auto mid = b.getCentreX();
        for (int c = 0; c < 2; ++c)
        {
            auto bar = juce::Rectangle<float>(mid + (c==0 ? -12.0f : 3.0f),
                                              b.getY(), 9.0f, b.getHeight());
            g.setColour(inset);
            g.fillRoundedRectangle(bar,2.0f);
            const float level = c == 0 ? left : right;
            const float db = juce::Decibels::gainToDecibels(level, -84.0f);
            const int active = juce::jlimit(0, 26,
                juce::roundToInt((db + 72.0f) / 78.0f * 26.0f));
            const float segment = bar.getHeight() / 26.0f;
            for(int i = 0; i < 26; ++i)
            {
                const float y = bar.getBottom() - segment * (i + 1);
                const auto tint = i > 23 ? red : i > 19 ? amber : green;
                g.setColour(i < active ? tint : tint.withAlpha(0.13f));
                g.fillRect(bar.getX()+0.8f, y+0.8f, bar.getWidth()-1.6f,
                           juce::jmax(1.0f,segment-2.0f));
            }
        }
        caption(g, "L", {static_cast<int>(mid-15), getHeight()-17, 12, 14},9.0f);
        caption(g, "R", {static_cast<int>(mid+3), getHeight()-17, 12, 14},9.0f);
    }
private:
    float left = 0.0f, right = 0.0f;
};
} // namespace sc220::visual
