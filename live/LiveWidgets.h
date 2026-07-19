#pragma once

#include "LiveTheme.h"
#include "LiveIcons.h"
#include <functional>

namespace askp::live {

class KnobControl final : public juce::Component,
                          private juce::Slider::Listener
{
public:
    KnobControl (const juce::String& labelText, const juce::String& suffix,
                 juce::Colour accent, double min, double max, double step);
    ~KnobControl() override;

    juce::Slider& slider() noexcept { return knob; }
    void setValue (double value, juce::NotificationType notification = juce::dontSendNotification);
    double getValue() const noexcept { return knob.getValue(); }
    void setFormatter (std::function<juce::String(double)> formatter);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void sliderValueChanged (juce::Slider*) override;

    juce::String title;
    juce::String unit;
    juce::Colour accentColour;
    juce::Slider knob;
    std::function<juce::String(double)> valueFormatter;
};

class StereoMeter final : public juce::Component
{
public:
    void setLevels (float leftDb, float rightDb);
    void setCompact (bool value) noexcept { compact = value; repaint(); }
    void paint (juce::Graphics&) override;

private:
    float levelL = -120.0f;
    float levelR = -120.0f;
    float peakHoldL = -120.0f;
    float peakHoldR = -120.0f;
    int peakHoldFramesL = 0;
    int peakHoldFramesR = 0;
    bool compact = false;
};

class VolumeFader final : public juce::Component,
                          private juce::Slider::Listener
{
public:
    explicit VolumeFader (const juce::String& labelText);
    ~VolumeFader() override;

    std::function<void(float)> onValueChanged;

    void setValue (float valueDb, juce::NotificationType notification = juce::dontSendNotification);
    float getValue() const noexcept { return static_cast<float> (fader.getValue()); }
    void setLevels (float leftDb, float rightDb);
    void setBypassed (bool value) noexcept;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void sliderValueChanged (juce::Slider*) override;

    juce::String label;
    juce::Slider fader;
    StereoMeter meter;
    bool bypassed = false;
};

class GainReductionMeter final : public juce::Component
{
public:
    void setReduction (float db) noexcept;
    void setVertical (bool shouldBeVertical) noexcept { vertical = shouldBeVertical; repaint(); }
    void setTitle (juce::String newTitle) { title = std::move (newTitle); repaint(); }
    void paint (juce::Graphics&) override;
private:
    float reductionDb = 0.0f;
    bool vertical = false;
    juce::String title { "GAIN REDUCTION" };
};

class StatusPill final : public juce::Component
{
public:
    void setStatus (const juce::String& text, juce::Colour colour, bool active);
    void paint (juce::Graphics&) override;
private:
    juce::String statusText;
    juce::Colour statusColour { colours::textMuted };
    bool statusActive = false;
};

class CompactRackRow final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    CompactRackRow (int index, const juce::String& iconResource,
                    const juce::String& name, bool canBypass,
                    bool selected = false, bool locked = false,
                    bool openOnly = false);

    std::function<void(bool)> onBypassChanged;
    std::function<void()> onOpen;

    void setBypassed (bool value);
    bool isBypassed() const noexcept { return bypassed; }
    void setSelected (bool value);
    void setLocked (bool value);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void toggleBypassFromUi();
    int rowIndex = 0;
    juce::String moduleName;
    bool bypassAllowed = false;
    bool selectedState = false;
    bool lockedState = false;
    bool openOnlyState = false;
    bool bypassed = false;
    std::unique_ptr<juce::Drawable> moduleIcon;
    std::unique_ptr<LucideIconButton> actionButton;
};

class MiniStat final : public juce::Component
{
public:
    MiniStat (const juce::String& label, const juce::String& iconResource = {});
    void setValue (const juce::String& value, juce::Colour colour = colours::text);
    void paint (juce::Graphics&) override;
private:
    juce::String labelText;
    juce::String valueText;
    juce::Colour valueColour { colours::text };
    std::unique_ptr<juce::Drawable> icon;
};

} // namespace askp::live
