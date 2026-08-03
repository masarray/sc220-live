#include "SC220MainComponent.h"

#include "BinaryData.h"

#include <cmath>
#include <initializer_list>

namespace askp::live {
namespace palette {
const auto ivory = juce::Colour (0xfff3f0e9);
const auto surface = juce::Colour (0xfffaf8f3);
const auto ink = juce::Colour (0xff141516);
const auto graphite = juce::Colour (0xff2a2d2e);
const auto muted = juce::Colour (0xff6f716e);
const auto border = juce::Colour (0xffc9c3b8);
const auto orange = juce::Colour (0xffd86a18);
const auto green = juce::Colour (0xff23834a);
const auto amber = juce::Colour (0xffd99a22);
const auto red = juce::Colour (0xffc94332);
}

namespace {
juce::Image loadImage (const char* resourceName)
{
    int bytes = 0;
    if (const auto* data = BinaryData::getNamedResource (resourceName, bytes))
        return juce::ImageCache::getFromMemory (data, bytes);
    return {};
}

juce::Typeface::Ptr loadTypeface (const char* resourceName)
{
    int bytes = 0;
    if (const auto* data = BinaryData::getNamedResource (resourceName, bytes))
        return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (bytes));
    return {};
}

juce::String valueWithUnit (double value, const juce::String& suffix, int decimals = 1)
{
    return juce::String (value, decimals) + suffix;
}
} // namespace

class SC220MainComponent::SurfaceLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SurfaceLookAndFeel()
        : regular (loadTypeface ("plus_jakarta_sans_regular_ttf")),
          semibold (loadTypeface ("plus_jakarta_sans_semibold_ttf"))
    {
        setColour (juce::ComboBox::backgroundColourId, palette::surface);
        setColour (juce::ComboBox::textColourId, palette::ink);
        setColour (juce::ComboBox::outlineColourId, palette::border);
        setColour (juce::PopupMenu::backgroundColourId, palette::surface);
        setColour (juce::PopupMenu::textColourId, palette::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::ink);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, palette::ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        if (font.isBold() && semibold != nullptr)
            return semibold;
        if (regular != nullptr)
            return regular;
        return juce::LookAndFeel_V4::getTypefaceForFont (font);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (10.0f, 13.0f, height * 0.34f),
                                              juce::Font::bold));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (10.0f, 13.0f, box.getHeight() * 0.34f)));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        const auto area = button.getLocalBounds().toFloat().reduced (0.7f);
        auto fill = button.getToggleState() ? palette::ink : palette::surface;
        if (button.getButtonText() == "GO LIVE")
            fill = button.getToggleState() ? palette::green : palette::orange;
        if (highlighted)
            fill = fill.interpolatedWith (juce::Colours::white, 0.08f);
        if (down)
            fill = fill.darker (0.12f);

        g.setColour (fill);
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (button.getToggleState() || button.getButtonText() == "GO LIVE"
                         ? juce::Colours::transparentBlack : palette::border);
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        const bool inverse = button.getToggleState() || button.getButtonText() == "GO LIVE";
        g.setColour (inverse ? juce::Colours::white : palette::ink);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 2),
                          juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox&) override
    {
        const auto area = juce::Rectangle<float> (0.5f, 0.5f,
                                                   static_cast<float> (width - 1),
                                                   static_cast<float> (height - 1));
        g.setColour (palette::surface);
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (palette::border);
        g.drawRoundedRectangle (area, 4.0f, 1.0f);

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
        label.setBounds (10, 1, box.getWidth() - 36, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const float diameter = static_cast<float> (juce::jmin (width, height)) - 10.0f;
        const float radius = diameter * 0.5f;
        const auto centre = juce::Point<float> (static_cast<float> (x + width) * 0.5f,
                                                static_cast<float> (y + height) * 0.5f);
        const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillEllipse (knob.translated (0.0f, 3.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff414445), knob.getTopLeft(),
                                                 juce::Colour (0xff111213), knob.getBottomRight(), false));
        g.fillEllipse (knob);
        g.setColour (juce::Colour (0xff8a8c89));
        g.drawEllipse (knob, 1.4f);
        g.setColour (juce::Colour (0xff050606));
        g.drawEllipse (knob.reduced (4.0f), 1.0f);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius + 4.0f, radius + 4.0f, 0.0f,
                           startAngle, startAngle + position * (endAngle - startAngle), true);
        g.setColour (palette::orange);
        g.strokePath (arc, juce::PathStrokeType (2.3f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const float angle = startAngle + position * (endAngle - startAngle);
        const auto pointerStart = centre.getPointOnCircumference (radius * 0.22f, angle);
        const auto pointerEnd = centre.getPointOnCircumference (radius * 0.72f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
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

        const float centreX = static_cast<float> (x + width) * 0.5f;
        const auto track = juce::Rectangle<float> (centreX - 4.0f, static_cast<float> (y + 3),
                                                    8.0f, static_cast<float> (height - 6));
        g.setColour (juce::Colour (0xff171919));
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.drawRoundedRectangle (track, 3.0f, 1.0f);

        const auto cap = juce::Rectangle<float> (centreX - 22.0f, sliderPos - 16.0f, 44.0f, 32.0f);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (cap.translated (0.0f, 3.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff5f5f2), cap.getTopLeft(),
                                                 juce::Colour (0xff8f918e), cap.getBottomLeft(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (juce::Colour (0xff626462));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);
        g.setColour (juce::Colour (0xff555755));
        g.drawHorizontalLine (juce::roundToInt (sliderPos), cap.getX() + 3.0f, cap.getRight() - 3.0f);
    }

private:
    juce::Typeface::Ptr regular;
    juce::Typeface::Ptr semibold;
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
        const int gap = 7;
        const int barWidth = juce::jmax (6, (area.getWidth() - gap) / 2);
        drawBar (g, area.removeFromLeft (barWidth), left);
        area.removeFromLeft (gap);
        drawBar (g, area.removeFromLeft (barWidth), right);
    }

private:
    static void drawBar (juce::Graphics& g, juce::Rectangle<int> area, float db)
    {
        g.setColour (juce::Colour (0xff222424));
        g.fillRect (area);
        constexpr int segments = 24;
        const float normal = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        const int active = juce::roundToInt (normal * segments);
        const float segmentHeight = static_cast<float> (area.getHeight()) / segments;
        for (int i = 0; i < segments; ++i)
        {
            const bool lit = i < active;
            const float y = static_cast<float> (area.getBottom()) - (i + 1) * segmentHeight;
            auto colour = i > 20 ? palette::red : i > 16 ? palette::amber : palette::green;
            g.setColour (lit ? colour : colour.withAlpha (0.15f));
            g.fillRect (static_cast<float> (area.getX() + 1), y + 1.0f,
                        static_cast<float> (area.getWidth() - 2), juce::jmax (1.0f, segmentHeight - 2.0f));
        }
    }

    float left = -120.0f;
    float right = -120.0f;
};

class SC220MainComponent::SignatureDisplay final : public juce::Component
{
public:
    void setValues (const std::array<float, 5>& next)
    {
        values = next;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::ink);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (juce::Colour (0xff3d4040));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);

        auto graph = area.reduced (36.0f, 24.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
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

        juce::Path curve;
        const float bass = (values[1] - 50.0f) / 50.0f;
        const float treble = (values[2] - 50.0f) / 50.0f;
        const float body = (values[3] - 50.0f) / 50.0f;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = static_cast<float> (i) / 120.0f;
            const float low = bass * std::exp (-7.0f * t);
            const float mid = body * std::exp (-std::pow ((t - 0.43f) / 0.22f, 2.0f));
            const float high = treble * std::pow (t, 2.2f);
            const float rolloff = t > 0.86f ? (t - 0.86f) * 3.5f : 0.0f;
            const float response = low * 0.17f + mid * 0.12f + high * 0.15f - rolloff;
            const float x = graph.getX() + t * graph.getWidth();
            const float y = graph.getCentreY() - response * graph.getHeight();
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (juce::Colours::white.withAlpha (0.82f));
        g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (juce::Colours::white.withAlpha (0.58f));
        g.setFont (10.0f);
        const auto labels = graph.toNearestInt();
        g.drawText ("20", labels.getX(), labels.getBottom() + 4, 30, 16,
                    juce::Justification::centredLeft);
        g.drawText ("1k", labels.getCentreX() - 15, labels.getBottom() + 4, 30, 16,
                    juce::Justification::centred);
        g.drawText ("20k", labels.getRight() - 34, labels.getBottom() + 4, 34, 16,
                    juce::Justification::centredRight);
    }

private:
    std::array<float, 5> values { 78.0f, 68.0f, 80.0f, 76.0f, 88.0f };
};

class SC220MainComponent::LoudnessDisplay final : public juce::Component
{
public:
    void setReading (float nextLufs, float nextPeak)
    {
        lufs = nextLufs;
        peak = nextPeak;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (8.0f);
        const auto dial = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                                       juce::jmin (area.getWidth(), area.getHeight()));
        g.setColour (palette::ink);
        g.fillEllipse (dial);
        g.setColour (palette::border);
        g.drawEllipse (dial, 1.5f);

        constexpr float start = juce::MathConstants<float>::pi * 0.72f;
        constexpr float end = juce::MathConstants<float>::pi * 2.28f;
        const float normal = juce::jlimit (0.0f, 1.0f, (lufs + 36.0f) / 30.0f);
        const float radius = dial.getWidth() * 0.43f;
        const auto centre = dial.getCentre();
        for (int i = 0; i < 32; ++i)
        {
            const float t = static_cast<float> (i) / 31.0f;
            const float angle = start + t * (end - start);
            auto colour = t > 0.83f ? palette::red : t > 0.62f ? palette::amber : palette::green;
            g.setColour (i <= juce::roundToInt (normal * 31.0f) ? colour : colour.withAlpha (0.18f));
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.83f, angle),
                          centre.getPointOnCircumference (radius, angle) }, 4.0f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (dial.getWidth() * 0.20f));
        g.drawText (lufs <= -100.0f ? "--" : juce::String (lufs, 1),
                    dial.withTrimmedTop (dial.getHeight() * 0.28f).withHeight (dial.getHeight() * 0.25f),
                    juce::Justification::centred);
        g.setFont (juce::FontOptions (dial.getWidth() * 0.075f));
        g.drawText ("LUFS", dial.withTrimmedTop (dial.getHeight() * 0.52f).withHeight (22.0f),
                    juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (juce::String (peak, 1) + " dBFS",
                    dial.withTrimmedTop (dial.getHeight() * 0.67f).withHeight (20.0f),
                    juce::Justification::centred);
    }

private:
    float lufs = -120.0f;
    float peak = -120.0f;
};

SC220MainComponent::SC220MainComponent (LiveAudioEngine& engine, WindowActions actions)
    : audioEngine (engine), windowActions (std::move (actions)),
      lookAndFeel (std::make_unique<SurfaceLookAndFeel>()),
      sc220Meter (std::make_unique<StereoMeter>()),
      pcMeter (std::make_unique<StereoMeter>()),
      streamMeter (std::make_unique<StereoMeter>()),
      signatureDisplay (std::make_unique<SignatureDisplay>()),
      loudnessDisplay (std::make_unique<LoudnessDisplay>())
{
    setOpaque (true);
    setLookAndFeel (lookAndFeel.get());
    recordingTechLogo = loadImage ("recording_tech_logo_png");
    sc220Image = loadImage ("sc220_mkii_device_png");

    configureControls();
    refreshAllControls();
    startTimerHz (30);
}

SC220MainComponent::~SC220MainComponent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SC220MainComponent::configureControls()
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &minimiseButton, &maximiseButton, &closeButton, &settingsButton, &goLiveButton,
             &presetBox, &outputBox, &stereoButton, &dualMonoButton, &pcSourceBox, &inputTrim,
             &sc220Fader, &pcFader, &duckAmount, &streamFader, &monitorKnob, &sc220Mute,
             &streamMute, &detailsButton })
        addAndMakeVisible (component);
    for (auto& knob : macroKnobs)
        addAndMakeVisible (knob);
    for (auto* component : std::initializer_list<juce::Component*> {
             sc220Meter.get(), pcMeter.get(), streamMeter.get(), signatureDisplay.get(),
             loudnessDisplay.get() })
        addAndMakeVisible (component);

    minimiseButton.onClick = [this] { if (windowActions.minimise) windowActions.minimise(); };
    maximiseButton.onClick = [this] { if (windowActions.toggleMaximise) windowActions.toggleMaximise(); };
    closeButton.onClick = [this] { if (windowActions.hideToTray) windowActions.hideToTray(); };
    settingsButton.onClick = [this] { showAudioSettings(); };

    goLiveButton.setClickingTogglesState (true);
    goLiveButton.onClick = [this]
    {
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
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
        slider.setTextValueSuffix (suffix);
        slider.setDoubleClickReturnValue (true, minimum == 0.0 ? 0.0 : 0.0);
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
        configureKnob (knob, 0.0, 100.0, 0.1, "%");

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
    monitorKnob.setTooltip ("Monitor output activates after a physical monitor endpoint is configured.");
    detailsButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "SC220 Live Details",
            "Advanced ducking, device policy, gain-match and recovery controls will remain outside the Live surface.\n\nThe active path is SC220 + System Mix -> ASK-P Signature -> VB-CABLE.");
    };
}

void SC220MainComponent::refreshAllControls()
{
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
    goLiveButton.setToggleState (! audioEngine.isOutputMuted(), juce::dontSendNotification);
    goLiveButton.setButtonText (audioEngine.isOutputMuted() ? "GO LIVE" : "LIVE");
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
    lastStatus = audioEngine.getStatus();
    sc220Meter->setLevels (lastStatus.inputPeakDbL, lastStatus.inputPeakDbR);
    pcMeter->setLevels (lastStatus.pcPeakDbL, lastStatus.pcPeakDbR);
    streamMeter->setLevels (lastStatus.outputPeakDbL, lastStatus.outputPeakDbR);
    loudnessDisplay->setReading (lastStatus.loudnessLufs,
                                 juce::jmax (lastStatus.outputPeakDbL, lastStatus.outputPeakDbR));
    signatureDisplay->setValues ({ audioEngine.getEnhance(), audioEngine.getSmartBass(),
                                   audioEngine.getSmartTreble(), audioEngine.getVocalBody(),
                                   audioEngine.getStereoMagic() });
    repaint();
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

void SC220MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (palette::ivory);
    drawHeader (g);
    drawInputSurface (g);
    drawSignatureSurface (g);
    drawMasterSurface (g);
    drawFooter (g);
}

void SC220MainComponent::drawHeader (juce::Graphics& g)
{
    g.setColour (palette::ink);
    g.fillRect (headerArea);

    if (recordingTechLogo.isValid())
        g.drawImageWithin (recordingTechLogo, headerArea.getX() + 14, headerArea.getY() + 13,
                           145, 38, juce::RectanglePlacement::centred, false);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("SC220", 176, 0, 70, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (palette::orange);
    g.drawText ("LIVE", 244, 0, 52, headerArea.getHeight(), juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.16f));
    for (int x : { 164, 320, 522, 690 })
        g.drawVerticalLine (x, 14.0f, static_cast<float> (headerArea.getBottom() - 14));

    g.setColour (juce::Colours::white.withAlpha (0.88f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String (lastStatus.sampleRate > 0.0 ? lastStatus.sampleRate / 1000.0 : 48.0, 0) + " kHz",
                540, 0, 72, headerArea.getHeight(), juce::Justification::centred);
    g.drawText (juce::String (lastStatus.bufferSize > 0 ? lastStatus.bufferSize : 128) + " samples",
                610, 0, 92, headerArea.getHeight(), juce::Justification::centred);

    const bool stable = lastStatus.deviceOpen && lastStatus.outputIsVirtualCable
                     && lastStatus.pcLoopbackRunning && lastStatus.lastError.isEmpty();
    g.setColour (stable ? palette::green : palette::amber);
    g.fillEllipse (720.0f, 29.0f, 9.0f, 9.0f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (stable ? "STABLE" : "SETUP", 735, 0, 68, headerArea.getHeight(),
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
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("INPUT MIX", inputArea.withHeight (50), juce::Justification::centred);
    g.setColour (palette::border);
    g.drawVerticalLine (sc220StripArea.getRight(), static_cast<float> (sc220StripArea.getY() + 10),
                        static_cast<float> (sc220StripArea.getBottom() - 10));

    if (sc220Image.isValid())
        g.drawImageWithin (sc220Image, sc220StripArea.getCentreX() - 64, sc220StripArea.getY() + 12,
                           128, 82, juce::RectanglePlacement::centred, false);

    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (sc220StripArea.getCentreX() - 78),
                   static_cast<float> (sc220StripArea.getY() + 101), 9.0f, 9.0f);
    g.setColour (palette::ink);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("SC220 CONNECTED", sc220StripArea.getCentreX() - 64, sc220StripArea.getY() + 94,
                145, 24, juce::Justification::centredLeft);

    g.setColour (palette::graphite);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("PC AUDIO", pcStripArea.getX(), pcStripArea.getY() + 32,
                pcStripArea.getWidth(), 24, juce::Justification::centred);
    g.setColour (lastStatus.pcLoopbackRunning ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (pcStripArea.getCentreX() - 73),
                   static_cast<float> (pcStripArea.getY() + 74), 9.0f, 9.0f);
    g.setColour (palette::ink);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (lastStatus.pcLoopbackRunning ? "LOOPBACK ACTIVE" : "LOOPBACK STARTING",
                pcStripArea.getCentreX() - 58, pcStripArea.getY() + 67, 140, 24,
                juce::Justification::centredLeft);

    g.setColour (palette::muted);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
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
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText ("KEY: SC220", duckAmount.getRight() + 20, duckAmount.getY(),
                82, duckAmount.getHeight(), juce::Justification::centredLeft);
}

void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)
{
    g.setColour (palette::ivory);
    g.fillRect (signatureArea);
    g.setColour (palette::ink);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("MAS ARI SIGNATURE", signatureArea.getX(), signatureArea.getY() + 24,
                signatureArea.getWidth(), 28, juce::Justification::centred);

    static constexpr const char* names[] = {
        "ENHANCE", "SMART BASS", "SMART TREBLE", "VOCAL BODY", "STEREO MAGIC"
    };
    for (size_t i = 0; i < macroKnobs.size(); ++i)
    {
        const auto& knob = macroKnobs[i];
        g.setColour (palette::ink);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (names[i], knob.getX() - 16, knob.getY() - 25, knob.getWidth() + 32, 18,
                    juce::Justification::centred);
        g.setFont (juce::FontOptions (16.0f));
        g.drawText (juce::String (knob.getValue(), 0) + "%", knob.getX(), knob.getY() - 6,
                    knob.getWidth(), 22, juce::Justification::centred);
    }

    g.setColour (palette::green);
    g.fillEllipse (static_cast<float> (signatureArea.getCentreX() - 96),
                   static_cast<float> (signatureArea.getBottom() - 50), 8.0f, 8.0f);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("SMART PROTECT  /  ON", signatureArea.getCentreX() - 82,
                signatureArea.getBottom() - 60, 170, 28, juce::Justification::centredLeft);
}

void SC220MainComponent::drawMasterSurface (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (masterArea);
    g.setColour (palette::border);
    g.drawVerticalLine (masterArea.getX(), static_cast<float> (masterArea.getY()),
                        static_cast<float> (masterArea.getBottom()));
    g.setColour (palette::ink);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("LOUDNESS", masterArea.getX(), masterArea.getY() + 20,
                masterArea.getWidth(), 22, juce::Justification::centred);
    g.drawText ("STREAM", streamFader.getX() - 25, streamFader.getY() - 28,
                streamFader.getWidth() + 50, 22, juce::Justification::centred);
    g.drawText ("MONITOR", monitorKnob.getX() - 16, monitorKnob.getY() - 24,
                monitorKnob.getWidth() + 32, 22, juce::Justification::centred);
    g.setColour (palette::muted);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("OUTPUT", masterArea.getRight() - 174, masterArea.getBottom() - 168,
                138, 18, juce::Justification::centred);
    g.setColour (lastStatus.outputIsVirtualCable ? palette::green : palette::amber);
    g.fillEllipse (static_cast<float> (masterArea.getRight() - 150),
                   static_cast<float> (masterArea.getBottom() - 62), 9.0f, 9.0f);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (lastStatus.outputIsVirtualCable ? "READY" : "SELECT VB-CABLE",
                masterArea.getRight() - 133, masterArea.getBottom() - 71, 112, 28,
                juce::Justification::centredLeft);
}

void SC220MainComponent::drawFooter (juce::Graphics& g)
{
    g.setColour (palette::surface);
    g.fillRect (footerArea);
    g.setColour (palette::border);
    g.drawHorizontalLine (footerArea.getY(), 0.0f, static_cast<float> (getWidth()));
    g.setColour (palette::ink);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    const auto pathText = "SC220 + PC AUDIO    >    SIGNATURE    >    VB-CABLE";
    g.drawText (pathText, footerArea.getX() + 48, footerArea.getY(), 470,
                footerArea.getHeight(), juce::Justification::centredLeft);

    const float latencyMs = lastStatus.sampleRate > 0.0
        ? static_cast<float> (lastStatus.totalLatencySamples / lastStatus.sampleRate * 1000.0)
        : 0.0f;
    const juce::String health = "CPU  " + juce::String (lastStatus.cpuPercent, 1) + "%      "
                              + "DROP  " + juce::String (lastStatus.dropouts) + "      "
                              + "LATENCY  " + juce::String (latencyMs, 1) + " ms";
    g.drawText (health, footerArea.getCentreX(), footerArea.getY(),
                footerArea.getWidth() / 2 - 48, footerArea.getHeight(),
                juce::Justification::centredRight);
}

void SC220MainComponent::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (66);
    footerArea = area.removeFromBottom (70);
    const int inputWidth = juce::roundToInt (area.getWidth() * 0.325f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.455f);
    inputArea = area.removeFromLeft (inputWidth);
    signatureArea = area.removeFromLeft (signatureWidth);
    masterArea = area;

    minimiseButton.setBounds (getWidth() - 284, 14, 42, 38);
    maximiseButton.setBounds (getWidth() - 238, 14, 42, 38);
    closeButton.setBounds (getWidth() - 192, 14, 42, 38);
    settingsButton.setBounds (getWidth() - 440, 14, 120, 38);
    goLiveButton.setBounds (getWidth() - 144, 10, 132, 46);
    presetBox.setBounds (338, 15, 178, 36);

    auto inputContent = inputArea.reduced (18, 48);
    sc220StripArea = inputContent.removeFromLeft (inputContent.getWidth() / 2);
    pcStripArea = inputContent;

    stereoButton.setBounds (sc220StripArea.getX() + 10, sc220StripArea.getY() + 122,
                            sc220StripArea.getWidth() / 2 - 10, 31);
    dualMonoButton.setBounds (stereoButton.getRight(), stereoButton.getY(),
                              sc220StripArea.getWidth() / 2 - 10, 31);
    inputTrim.setBounds (sc220StripArea.getCentreX() - 48, sc220StripArea.getY() + 184, 96, 96);
    sc220Meter->setBounds (sc220StripArea.getX() + 18, sc220StripArea.getY() + 304,
                           30, sc220StripArea.getHeight() - 390);
    sc220Fader.setBounds (sc220StripArea.getCentreX() - 35, sc220StripArea.getY() + 300,
                          70, sc220StripArea.getHeight() - 386);
    sc220Mute.setBounds (sc220StripArea.getCentreX() - 36, sc220StripArea.getBottom() - 42, 72, 31);

    pcSourceBox.setBounds (pcStripArea.getX() + 30, pcStripArea.getY() + 104,
                           pcStripArea.getWidth() - 60, 34);
    pcMeter->setBounds (pcStripArea.getX() + 18, pcStripArea.getY() + 218,
                        30, pcStripArea.getHeight() - 304);
    pcFader.setBounds (pcStripArea.getCentreX() - 35, pcStripArea.getY() + 214,
                       70, pcStripArea.getHeight() - 300);
    duckAmount.setBounds (pcStripArea.getX() + 24, pcStripArea.getBottom() - 82, 70, 70);

    presetBox.setBounds (signatureArea.getCentreX() - 96, signatureArea.getY() + 60, 192, 36);
    signatureDisplay->setBounds (signatureArea.reduced (34, 0).withTop (signatureArea.getY() + 128)
                                 .withBottom (signatureArea.getY() + juce::roundToInt (signatureArea.getHeight() * 0.56f)));

    auto macroRow = signatureArea.reduced (24, 0);
    macroRow.setTop (signatureDisplay->getBottom() + 54);
    macroRow.setBottom (signatureArea.getBottom() - 72);
    const int macroWidth = macroRow.getWidth() / static_cast<int> (macroKnobs.size());
    for (auto& knob : macroKnobs)
    {
        auto slot = macroRow.removeFromLeft (macroWidth);
        knob.setBounds (slot.reduced (12, 2));
    }
    detailsButton.setBounds (signatureArea.getRight() - 112, signatureArea.getBottom() - 54, 88, 32);

    loudnessDisplay->setBounds (masterArea.getCentreX() - 103, masterArea.getY() + 48, 206, 206);
    auto masterLower = masterArea.reduced (24, 0);
    masterLower.setTop (masterArea.getY() + 300);
    streamMeter->setBounds (masterLower.getX(), masterLower.getY() + 42, 32,
                            masterLower.getHeight() - 140);
    streamFader.setBounds (masterLower.getX() + 40, masterLower.getY() + 38, 72,
                           masterLower.getHeight() - 134);
    streamMute.setBounds (streamFader.getBounds().getCentreX() - 36, masterArea.getBottom() - 44, 72, 31);
    monitorKnob.setBounds (masterArea.getRight() - 164, masterLower.getY() + 52, 108, 108);

    outputBox.setBounds (masterArea.getRight() - 184, masterArea.getBottom() - 144, 154, 36);
}

} // namespace askp::live
