#include "LiveMainComponent.h"

#include "BinaryData.h"

#include <initializer_list>
#include <memory>

namespace askp::live {
namespace {
constexpr int headerHeight = 58;
constexpr int footerHeight = 54;
constexpr int outerPad = 12;

void styleCombo (juce::ComboBox& box, const juce::String& tooltip)
{
    box.setTooltip (tooltip);
    box.setJustificationType (juce::Justification::centredLeft);
}

void drawFieldLabel (juce::Graphics& g, LiveLookAndFeel& lf,
                     juce::Rectangle<int> bounds, const juce::String& text)
{
    g.setColour (colours::textMuted);
    g.setFont (lf.jakarta (9.5f, true));
    g.drawText (text.toUpperCase(), bounds, juce::Justification::centredLeft, false);
}

void selectComboText (juce::ComboBox& combo, const juce::String& text)
{
    for (int index = 0; index < combo.getNumItems(); ++index)
    {
        if (combo.getItemText (index) == text)
        {
            combo.setSelectedItemIndex (index, juce::dontSendNotification);
            return;
        }
    }
    combo.setText (text, juce::dontSendNotification);
}

class AudioSettingsCard final : public juce::Component
{
public:
    AudioSettingsCard (LiveAudioEngine& engineToUse,
                       std::function<void()> refreshCallback)
        : engine (engineToUse), refresh (std::move (refreshCallback))
    {
        setLookAndFeel (&cardLookAndFeel);
        setSize (338, 456);

        for (juce::Component* component : std::initializer_list<juce::Component*> {
                 &sampleRate, &bufferSize, &channelMode, &noiseQuality,
                 &hearRemoved, &restartButton, &refreshButton, &referenceButton, &learnButton, &message })
            addAndMakeVisible (component);

        styleCombo (sampleRate, "Audio engine sample rate");
        styleCombo (bufferSize, "Audio engine buffer size");
        styleCombo (channelMode, "Input channel mode");
        styleCombo (noiseQuality, "Musical Noise Clean FFT quality");

        channelMode.addItem ("Mono to Dual Mono", 1);
        channelMode.addItem ("Stereo", 2);
        channelMode.setSelectedId (engine.getInputChannelMode() == 1 ? 1 : 2,
                                   juce::dontSendNotification);

        noiseQuality.addItem ("Live - FFT 256", 1);
        noiseQuality.addItem ("Clean - FFT 512", 2);
        noiseQuality.setSelectedId (
            engine.getNoiseClean().getQuality() == MusicalNoiseClean::Quality::live256 ? 1 : 2,
            juce::dontSendNotification);

        hearRemoved.setButtonText ("Hear Removed Noise");
        hearRemoved.setToggleState (engine.getNoiseClean().isListeningRemoved(),
                                    juce::dontSendNotification);

        restartButton.setButtonText ("Restart Audio Engine");
        refreshButton.setButtonText ("Refresh Device Lists");
        referenceButton.setButtonText ("Match VST Reference");
        referenceButton.setTooltip ("Bypass Live-only Noise Clean and Expander, set I/O trims to 0 dB, and keep the authoritative enhancer active for A/B comparison");
        learnButton.setButtonText ("Learn Noise Again");

        message.setJustificationType (juce::Justification::centredLeft);
        message.setColour (juce::Label::textColourId, colours::amber);
        message.setFont (cardLookAndFeel.jakarta (10.5f));

        populateAudioChoices();

        sampleRate.onChange = [this]
        {
            const auto value = sampleRate.getText().retainCharacters ("0123456789.").getDoubleValue() * 1000.0;
            if (value > 0.0)
                showResult (engine.setSampleRate (value));
        };
        bufferSize.onChange = [this]
        {
            const int value = bufferSize.getText().getIntValue();
            if (value > 0)
                showResult (engine.setBufferSize (value));
        };
        channelMode.onChange = [this]
        {
            engine.setInputChannelMode (channelMode.getSelectedId() == 1 ? 1 : 2);
            if (refresh) refresh();
        };
        noiseQuality.onChange = [this]
        {
            engine.getNoiseClean().setQuality (
                noiseQuality.getSelectedId() == 2
                    ? MusicalNoiseClean::Quality::clean512
                    : MusicalNoiseClean::Quality::live256);
            engine.restartAudio();
            if (refresh) refresh();
        };
        hearRemoved.onClick = [this]
        {
            engine.getNoiseClean().setListenRemoved (hearRemoved.getToggleState());
        };
        restartButton.onClick = [this]
        {
            engine.restartAudio();
            populateAudioChoices();
            if (refresh) refresh();
        };
        refreshButton.onClick = [this]
        {
            populateAudioChoices();
            if (refresh) refresh();
        };
        referenceButton.onClick = [this]
        {
            engine.applyVstReferenceConditions();
            message.setText ("VST reference conditions applied. Align recorded files by reported latency.",
                             juce::dontSendNotification);
            message.setColour (juce::Label::textColourId, colours::green);
            if (refresh) refresh();
        };
        learnButton.onClick = [this]
        {
            engine.getNoiseClean().startLearning (2.0);
            message.setText ("Listening to static noise for 2 seconds...", juce::dontSendNotification);
        };
    }

    ~AudioSettingsCard() override
    {
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panelHi);
        fillPremiumPanel (g, getLocalBounds().toFloat().reduced (0.5f), 12.0f);

        g.setColour (colours::text);
        g.setFont (cardLookAndFeel.jakarta (15.0f, true));
        g.drawText ("SETTINGS", 18, 12, getWidth() - 36, 24,
                    juce::Justification::centredLeft, false);

        g.setColour (colours::textMuted);
        g.setFont (cardLookAndFeel.jakarta (9.0f, true));
        g.drawText ("AUDIO SETTINGS", 18, 45, getWidth() - 36, 18,
                    juce::Justification::centredLeft, false);
        drawFieldLabel (g, cardLookAndFeel, { 18, 69, 110, 20 }, "Sample rate");
        drawFieldLabel (g, cardLookAndFeel, { 18, 113, 110, 20 }, "Buffer size");
        drawFieldLabel (g, cardLookAndFeel, { 18, 157, 110, 20 }, "Channel mode");
        drawFieldLabel (g, cardLookAndFeel, { 18, 201, 110, 20 }, "Noise quality");

        g.setColour (colours::border);
        g.drawHorizontalLine (252, 18.0f, static_cast<float> (getWidth() - 18));
        g.setColour (colours::textMuted);
        g.setFont (cardLookAndFeel.jakarta (9.0f, true));
        g.drawText ("AUDIO ENGINE", 18, 260, getWidth() - 36, 18,
                    juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        sampleRate.setBounds (142, 66, 178, 34);
        bufferSize.setBounds (142, 110, 178, 34);
        channelMode.setBounds (142, 154, 178, 34);
        noiseQuality.setBounds (142, 198, 178, 34);
        hearRemoved.setBounds (18, 232, 210, 28);

        restartButton.setBounds (18, 284, 146, 36);
        refreshButton.setBounds (174, 284, 146, 36);
        referenceButton.setBounds (18, 330, 302, 36);
        learnButton.setBounds (18, 376, 302, 36);
        message.setBounds (18, 416, 302, 30);
    }

private:
    void populateAudioChoices()
    {
        const auto status = engine.getStatus();

        sampleRate.clear (juce::dontSendNotification);
        int id = 1;
        for (const auto rate : engine.getAvailableSampleRates())
            sampleRate.addItem (juce::String (rate / 1000.0, rate == 44100.0 ? 1 : 0) + " kHz", id++);
        selectComboText (sampleRate,
                         juce::String (status.sampleRate / 1000.0,
                                       status.sampleRate == 44100.0 ? 1 : 0) + " kHz");

        bufferSize.clear (juce::dontSendNotification);
        id = 1;
        for (const auto size : engine.getAvailableBufferSizes())
            bufferSize.addItem (juce::String (size) + " samples", id++);
        selectComboText (bufferSize, juce::String (status.bufferSize) + " samples");
    }

    void showResult (const juce::String& result)
    {
        message.setText (result.isEmpty() ? "Audio setting applied." : result,
                         juce::dontSendNotification);
        message.setColour (juce::Label::textColourId,
                           result.isEmpty() ? colours::green : colours::amber);
        if (refresh) refresh();
    }

    LiveAudioEngine& engine;
    LiveLookAndFeel cardLookAndFeel;
    std::function<void()> refresh;
    juce::ComboBox sampleRate;
    juce::ComboBox bufferSize;
    juce::ComboBox channelMode;
    juce::ComboBox noiseQuality;
    juce::ToggleButton hearRemoved;
    juce::TextButton restartButton;
    juce::TextButton refreshButton;
    juce::TextButton referenceButton;
    juce::TextButton learnButton;
    juce::Label message;
};
} // namespace

LiveMainComponent::LiveMainComponent (LiveAudioEngine& engine, WindowActions actions)
    : audioEngine (engine), windowActions (std::move (actions)), visualizer (engine.getAskpProcessor())
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    configureControls();
    refreshAllControls();
    // The top-level window owns the active/inactive lifecycle explicitly.
    // At construction time isShowing() is normally false, and relying on a
    // later visibilityChanged() notification is not reliable across all JUCE
    // window creation paths. The application calls setUiActive(true) after the
    // window is visible.
    audioEngine.setVisualTapEnabled (false);
}

LiveMainComponent::~LiveMainComponent()
{
    stopTimer();
    audioEngine.setVisualTapEnabled (false);
    setLookAndFeel (nullptr);
}

void LiveMainComponent::configureControls()
{
    for (juce::Component* component : std::initializer_list<juce::Component*> {
             &settingsButton, &trayButton, &minimiseButton, &maximiseButton, &closeButton,
             &inputDevice, &outputDevice,
             &inputTrimRow, &noiseRow, &vstPreRow, &inputFader,
             &noiseReduction, &silenceAmount, &preserveAmount, &visualizer,
             &profileStatus, &learnNoise, &noiseBypass,
             &preset, &enhanceKnob, &bassKnob, &trebleKnob, &vocalKnob, &stereoKnob,
             &gainReductionMeter, &enhancerPower,
             &vstPostRow, &outputTrimRow, &limiterRow, &outputFader,
             &cpuStat, &latencyStat, &dropoutsStat, &profileStat, &trayStat })
        addAndMakeVisible (component);

    settingsButton.setTooltip ("Audio engine and Musical Noise Clean settings");
    trayButton.setTooltip ("Open compact tray controller");
    minimiseButton.setTooltip ("Minimise");
    maximiseButton.setTooltip ("Maximise or restore");
    closeButton.setTooltip ("Hide to tray; audio keeps running");

    settingsButton.onClick = [this] { showSettingsCard(); };
    trayButton.onClick = [this] { if (windowActions.showMiniController) windowActions.showMiniController(); };
    minimiseButton.onClick = [this] { if (windowActions.minimise) windowActions.minimise(); };
    maximiseButton.onClick = [this] { if (windowActions.toggleMaximise) windowActions.toggleMaximise(); };
    closeButton.onClick = [this] { if (windowActions.hideToTray) windowActions.hideToTray(); };

    noiseBypass.setTooltip ("Bypass Musical Noise Clean and the smooth expander");
    noiseBypass.setClickingTogglesState (true);
    noiseBypass.setToggleState (! audioEngine.getNoiseClean().isEnabled() ? false : true, juce::dontSendNotification);
    noiseBypass.onClick = [this]
    {
        const bool enabled = noiseBypass.getToggleState();
        audioEngine.getNoiseClean().setEnabled (enabled);
        audioEngine.getExpander().setEnabled (enabled);
        noiseRow.setBypassed (! enabled);
        repaint();
    };

    enhancerPower.setTooltip ("Bypass only the authoritative ArSonKuPik Smart Enhancer");
    enhancerPower.setClickingTogglesState (true);
    enhancerPower.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    enhancerPower.onClick = [this]
    {
        audioEngine.setBypassed (! enhancerPower.getToggleState());
        updateStatus();
        repaint();
    };

    styleCombo (inputDevice, "Audio interface input device");
    styleCombo (outputDevice, "Audio output device; select CABLE Input for streaming");
    styleCombo (preset, "Smart Enhancer factory preset");

    inputDevice.onChange = [this]
    {
        if (! updatingControls)
        {
            const auto error = audioEngine.selectInputDevice (inputDevice.getText());
            if (error.isNotEmpty()) showError ("Input device", error);
        }
    };
    outputDevice.onChange = [this]
    {
        if (! updatingControls)
        {
            const auto error = audioEngine.selectOutputDevice (outputDevice.getText());
            if (error.isNotEmpty()) showError ("Output device", error);
        }
    };

    learnNoise.setTooltip ("Play only the static noise, then press Learn Noise for two seconds");
    learnNoise.onClick = [this] { audioEngine.getNoiseClean().startLearning (2.0); };

    noiseReduction.setFormatter ([] (double value) { return juce::String (value, 1) + " dB"; });
    silenceAmount.setFormatter ([] (double value) { return juce::String (static_cast<int> (value)) + "%"; });
    preserveAmount.setFormatter ([] (double value) { return juce::String (static_cast<int> (value)) + "%"; });
    enhanceKnob.setFormatter ([] (double value) { return juce::String (value, 1) + "%"; });
    bassKnob.setFormatter ([] (double value) { return juce::String (value, 1) + "%"; });
    trebleKnob.setFormatter ([] (double value) { return juce::String (value, 1) + "%"; });
    vocalKnob.setFormatter ([] (double value) { return juce::String (value, 1) + "%"; });
    stereoKnob.setFormatter ([] (double value) { return juce::String (value, 1) + "%"; });

    noiseReduction.slider().onValueChange = [this]
    {
        if (! updatingControls)
            audioEngine.getNoiseClean().setReductionDb (static_cast<float> (noiseReduction.getValue()));
    };
    silenceAmount.slider().onValueChange = [this]
    {
        if (! updatingControls)
            audioEngine.getExpander().setAmount (static_cast<float> (silenceAmount.getValue() / 100.0));
    };
    preserveAmount.slider().onValueChange = [this]
    {
        if (! updatingControls)
            audioEngine.getNoiseClean().setPreserve (static_cast<float> (preserveAmount.getValue() / 100.0));
    };

    preset.onChange = [this]
    {
        if (! updatingControls && preset.getSelectedItemIndex() >= 0)
        {
            audioEngine.setPresetIndex (preset.getSelectedItemIndex());
            refreshAllControls();
        }
    };

    enhanceKnob.slider().onValueChange = [this]
    {
        if (! updatingControls) audioEngine.setEnhance (static_cast<float> (enhanceKnob.getValue()));
    };
    bassKnob.slider().onValueChange = [this]
    {
        if (! updatingControls) audioEngine.setSmartBass (static_cast<float> (bassKnob.getValue()));
    };
    trebleKnob.slider().onValueChange = [this]
    {
        if (! updatingControls) audioEngine.setSmartTreble (static_cast<float> (trebleKnob.getValue()));
    };
    vocalKnob.slider().onValueChange = [this]
    {
        if (! updatingControls) audioEngine.setVocalBody (static_cast<float> (vocalKnob.getValue()));
    };
    stereoKnob.slider().onValueChange = [this]
    {
        if (! updatingControls) audioEngine.setStereoMagic (static_cast<float> (stereoKnob.getValue()));
    };

    inputFader.onValueChanged = [this] (float valueDb)
    {
        if (! updatingControls) audioEngine.setInputTrimDb (valueDb);
    };
    outputFader.onValueChanged = [this] (float valueDb)
    {
        if (! updatingControls) audioEngine.setOutputTrimDb (valueDb);
    };

    inputTrimRow.onBypassChanged = [this] (bool bypassed)
    {
        audioEngine.setInputTrimBypassed (bypassed);
    };
    noiseRow.onBypassChanged = [this] (bool bypassed)
    {
        audioEngine.getNoiseClean().setEnabled (! bypassed);
        audioEngine.getExpander().setEnabled (! bypassed);
        noiseBypass.setToggleState (! bypassed, juce::dontSendNotification);
    };
    outputTrimRow.onBypassChanged = [this] (bool bypassed)
    {
        audioEngine.setOutputTrimBypassed (bypassed);
    };
    vstPreRow.onOpen = vstPostRow.onOpen = [this] { showExternalVstRoadmap(); };
}

void LiveMainComponent::refreshAllControls()
{
    updatingControls = true;
    updateDeviceLists();

    preset.clear (juce::dontSendNotification);
    preset.addItemList (audioEngine.getPresetNames(), 1);
    preset.setSelectedItemIndex (audioEngine.getPresetIndex(), juce::dontSendNotification);

    noiseReduction.setValue (audioEngine.getNoiseClean().getReductionDb());
    silenceAmount.setValue (audioEngine.getExpander().getAmount() * 100.0);
    preserveAmount.setValue (audioEngine.getNoiseClean().getPreserve() * 100.0);
    enhanceKnob.setValue (audioEngine.getEnhance());
    bassKnob.setValue (audioEngine.getSmartBass());
    trebleKnob.setValue (audioEngine.getSmartTreble());
    vocalKnob.setValue (audioEngine.getVocalBody());
    stereoKnob.setValue (audioEngine.getStereoMagic());
    inputFader.setValue (audioEngine.getInputTrimDb());
    outputFader.setValue (audioEngine.getOutputTrimDb());
    inputTrimRow.setBypassed (audioEngine.isInputTrimBypassed());
    noiseRow.setBypassed (! audioEngine.getNoiseClean().isEnabled());
    noiseBypass.setToggleState (audioEngine.getNoiseClean().isEnabled(), juce::dontSendNotification);
    outputTrimRow.setBypassed (audioEngine.isOutputTrimBypassed());
    enhancerPower.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    updatingControls = false;
}

void LiveMainComponent::updateDeviceLists()
{
    const auto status = audioEngine.getStatus();

    inputDevice.clear (juce::dontSendNotification);
    inputDevice.addItemList (audioEngine.getInputDeviceNames(), 1);
    setComboTextWithoutNotification (inputDevice, status.inputDevice);

    outputDevice.clear (juce::dontSendNotification);
    outputDevice.addItemList (audioEngine.getOutputDeviceNames(), 1);
    setComboTextWithoutNotification (outputDevice, status.outputDevice);
}

void LiveMainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::appBg);

    juce::ColourGradient headerGradient (juce::Colour (0xff0d1725), 0.0f, 0.0f,
                                         colours::appBg, static_cast<float> (getWidth()), 0.0f, false);
    g.setGradientFill (headerGradient);
    g.fillRect (0, 0, getWidth(), headerHeight);
    g.setColour (colours::border);
    g.drawHorizontalLine (headerHeight - 1, 0.0f, static_cast<float> (getWidth()));

    auto& lf = lookAndFeel;
    g.setColour (colours::cyan.withAlpha (0.15f));
    g.fillRoundedRectangle (juce::Rectangle<float> (18.0f, 10.0f, 38.0f, 38.0f), 9.0f);
    if (auto icon = createLucideIcon ("audio_waveform_svg", colours::cyan))
        icon->drawWithin (g, juce::Rectangle<float> (27.0f, 19.0f, 20.0f, 20.0f),
                          juce::RectanglePlacement::centred, 1.0f);

    g.setFont (lf.jakarta (19.0f, true));
    g.setColour (colours::text);
    g.drawText ("Sonkupik Live", 68, 0, 190, headerHeight,
                juce::Justification::centredLeft, false);
    g.setColour (colours::border);
    g.drawVerticalLine (260, 14.0f, 44.0f);

    const auto headerStatus = audioEngine.getStatus();
    const auto statusColour = ! headerStatus.deviceOpen ? colours::red
                            : ! audioEngine.isEngineEnabled() ? colours::amber
                            : headerStatus.outputIsVirtualCable ? colours::green : colours::amber;
    const juce::String statusText = ! headerStatus.deviceOpen ? "AUDIO DISCONNECTED"
                                  : ! audioEngine.isEngineEnabled() ? "ENGINE STOPPED"
                                  : headerStatus.outputIsVirtualCable ? "ENGINE RUNNING / VB-CABLE READY"
                                  : "ENGINE RUNNING / SELECT VB-CABLE";
    g.setColour (statusColour);
    g.fillEllipse (278.0f, 26.0f, 6.0f, 6.0f);
    g.setFont (lf.jakarta (10.5f, true));
    g.drawText (statusText, 293, 0, 330, headerHeight,
                juce::Justification::centredLeft, false);

    auto content = getLocalBounds().withTrimmedTop (headerHeight).withTrimmedBottom (footerHeight).reduced (outerPad);
    const int sideWidth = juce::jlimit (210, 244, static_cast<int> (content.getWidth() * 0.17f));
    auto left = content.removeFromLeft (sideWidth);
    content.removeFromLeft (10);
    auto right = content.removeFromRight (sideWidth);
    content.removeFromRight (10);
    auto centre = content;

    fillPremiumPanel (g, left.toFloat());
    fillPremiumPanel (g, right.toFloat());

    auto ioStrip = centre.removeFromTop (48);
    centre.removeFromTop (8);
    const int noiseHeight = juce::jlimit (300, 360, static_cast<int> (centre.getHeight() * 0.54f));
    auto noiseArea = centre.removeFromTop (noiseHeight);
    centre.removeFromTop (10);
    auto enhancerArea = centre;
    fillPremiumPanel (g, noiseArea.toFloat());
    fillPremiumPanel (g, enhancerArea.toFloat());

    drawSectionTitle (g, left.removeFromTop (44).reduced (14, 0).toFloat(), "BEFORE ENHANCER");
    drawSectionTitle (g, right.removeFromTop (44).reduced (14, 0).toFloat(), "AFTER CHAIN");

    auto inputLabelArea = ioStrip.removeFromLeft (ioStrip.getWidth() / 2).reduced (10, 0);
    auto outputLabelArea = ioStrip.reduced (10, 0);
    drawFieldLabel (g, lf, inputLabelArea.removeFromLeft (54), "Input");
    drawFieldLabel (g, lf, outputLabelArea.removeFromLeft (60), "Output");

    drawSectionTitle (g, noiseArea.removeFromTop (42).reduced (16, 0).toFloat(),
                      "MUSICAL NOISE CLEAN");
    drawSectionTitle (g, enhancerArea.removeFromTop (42).reduced (16, 0).toFloat(),
                      "SMART SOUND ENHANCER");

    g.setColour (colours::border);
    g.drawHorizontalLine (getHeight() - footerHeight, 12.0f, static_cast<float> (getWidth() - 12));
}

void LiveMainComponent::resized()
{
    const int titleButtonSize = 42;
    int x = getWidth() - 10;
    closeButton.setBounds (x -= titleButtonSize, 8, titleButtonSize, titleButtonSize);
    maximiseButton.setBounds (x -= titleButtonSize, 8, titleButtonSize, titleButtonSize);
    minimiseButton.setBounds (x -= titleButtonSize, 8, titleButtonSize, titleButtonSize);
    x -= 10;
    trayButton.setBounds (x -= titleButtonSize, 8, titleButtonSize, titleButtonSize);
    settingsButton.setBounds (x -= titleButtonSize, 8, titleButtonSize, titleButtonSize);

    auto content = getLocalBounds().withTrimmedTop (headerHeight).withTrimmedBottom (footerHeight).reduced (outerPad);
    const int sideWidth = juce::jlimit (210, 244, static_cast<int> (content.getWidth() * 0.17f));
    auto left = content.removeFromLeft (sideWidth).reduced (12);
    content.removeFromLeft (10);
    auto right = content.removeFromRight (sideWidth).reduced (12);
    content.removeFromRight (10);
    auto centre = content;

    left.removeFromTop (44);
    const int compactRowHeight = 48;
    inputTrimRow.setBounds (left.removeFromTop (compactRowHeight));
    left.removeFromTop (6);
    noiseRow.setBounds (left.removeFromTop (compactRowHeight));
    left.removeFromTop (6);
    vstPreRow.setBounds (left.removeFromTop (compactRowHeight));
    left.removeFromTop (10);
    inputFader.setBounds (left);

    right.removeFromTop (44);
    vstPostRow.setBounds (right.removeFromTop (compactRowHeight));
    right.removeFromTop (6);
    outputTrimRow.setBounds (right.removeFromTop (compactRowHeight));
    right.removeFromTop (6);
    limiterRow.setBounds (right.removeFromTop (compactRowHeight));
    right.removeFromTop (10);
    outputFader.setBounds (right);

    auto ioStrip = centre.removeFromTop (48);
    centre.removeFromTop (8);
    auto inputArea = ioStrip.removeFromLeft (ioStrip.getWidth() / 2).reduced (10, 5);
    auto outputArea = ioStrip.reduced (10, 5);
    inputArea.removeFromLeft (54);
    outputArea.removeFromLeft (60);
    inputDevice.setBounds (inputArea);
    outputDevice.setBounds (outputArea);

    const int noiseHeight = juce::jlimit (300, 360, static_cast<int> (centre.getHeight() * 0.54f));
    auto noiseArea = centre.removeFromTop (noiseHeight).reduced (14);
    centre.removeFromTop (10);
    auto enhancerArea = centre.reduced (14);

    auto noiseHeader = noiseArea.removeFromTop (42);
    noiseBypass.setBounds (noiseHeader.removeFromRight (34).withSizeKeepingCentre (34, 34));
    auto noiseControlColumn = noiseArea.removeFromLeft (320);
    noiseArea.removeFromLeft (14);

    auto knobBand = noiseControlColumn.removeFromTop (150);
    const int noiseKnobWidth = knobBand.getWidth() / 3;
    noiseReduction.setBounds (knobBand.removeFromLeft (noiseKnobWidth).reduced (4));
    silenceAmount.setBounds (knobBand.removeFromLeft (noiseKnobWidth).reduced (4));
    preserveAmount.setBounds (knobBand.reduced (4));
    noiseControlColumn.removeFromTop (10);
    profileStatus.setBounds (noiseControlColumn.removeFromTop (34));
    noiseControlColumn.removeFromTop (7);
    learnNoise.setBounds (noiseControlColumn.removeFromTop (36));

    visualizer.setBounds (noiseArea.reduced (2));

    auto enhancerHeader = enhancerArea.removeFromTop (42);
    enhancerPower.setBounds (enhancerHeader.removeFromRight (34).withSizeKeepingCentre (34, 34));
    auto topStrip = enhancerArea.removeFromTop (38);
    preset.setBounds (topStrip.removeFromLeft (240));
    topStrip.removeFromLeft (14);
    gainReductionMeter.setBounds (topStrip.reduced (0, 5));
    enhancerArea.removeFromTop (8);

    const int enhancerKnobWidth = enhancerArea.getWidth() / 5;
    enhanceKnob.setBounds (enhancerArea.removeFromLeft (enhancerKnobWidth).reduced (4));
    bassKnob.setBounds (enhancerArea.removeFromLeft (enhancerKnobWidth).reduced (4));
    trebleKnob.setBounds (enhancerArea.removeFromLeft (enhancerKnobWidth).reduced (4));
    vocalKnob.setBounds (enhancerArea.removeFromLeft (enhancerKnobWidth).reduced (4));
    stereoKnob.setBounds (enhancerArea.reduced (4));

    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (12, 0);
    const int statWidth = footer.getWidth() / 5;
    cpuStat.setBounds (footer.removeFromLeft (statWidth));
    latencyStat.setBounds (footer.removeFromLeft (statWidth));
    dropoutsStat.setBounds (footer.removeFromLeft (statWidth));
    profileStat.setBounds (footer.removeFromLeft (statWidth));
    trayStat.setBounds (footer);
}

void LiveMainComponent::mouseDown (const juce::MouseEvent& event)
{
    if (event.y < headerHeight && event.x < getWidth() - 230)
        windowDragger.startDraggingComponent (getTopLevelComponent(), event);
}

void LiveMainComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (event.getMouseDownY() < headerHeight && event.getMouseDownX() < getWidth() - 230)
        windowDragger.dragComponent (getTopLevelComponent(), event, nullptr);
}

void LiveMainComponent::timerCallback()
{
    if (uiActive)
        updateStatus();
}

void LiveMainComponent::setUiActive (bool active)
{
    if (uiActive == active)
    {
        // A newly shown window may request activation before the first timer
        // tick. Refresh immediately so meters/footer never appear frozen.
        if (active)
            updateStatus();
        return;
    }

    uiActive = active;
    audioEngine.setVisualTapEnabled (active);

    if (active)
    {
        startTimerHz (24);
        updateStatus();
    }
    else
    {
        stopTimer();
    }
}

void LiveMainComponent::visibilityChanged()
{
    // Fallback for native/host initiated visibility changes. The application
    // also calls setUiActive explicitly when showing or hiding the main window,
    // which avoids the JUCE construction-time isShowing() race.
    setUiActive (isShowing());
}

void LiveMainComponent::updateStatus()
{
    const auto status = audioEngine.getStatus();
    inputFader.setLevels (status.inputPeakDbL, status.inputPeakDbR);
    outputFader.setLevels (status.outputPeakDbL, status.outputPeakDbR);
    gainReductionMeter.setReduction (status.gainReductionDb + status.expanderReductionDb);

    const auto& cleaner = audioEngine.getNoiseClean();
    if (cleaner.isLearning())
    {
        profileStatus.setStatus ("Learning " + juce::String (static_cast<int> (cleaner.getLearningProgress() * 100.0f)) + "%",
                                 colours::violetHot, true);
        learnNoise.setButtonText ("Listening...");
    }
    else if (cleaner.hasProfile())
    {
        profileStatus.setStatus ("Noise Profile Learned", colours::cyan, true);
        learnNoise.setButtonText ("Learn Again");
    }
    else
    {
        profileStatus.setStatus ("No Noise Profile", colours::textMuted, false);
        learnNoise.setButtonText ("Learn Noise");
    }

    visualizer.refresh (audioEngine.isBypassed());

    const bool noiseEnabled = audioEngine.getNoiseClean().isEnabled();
    noiseReduction.setEnabled (noiseEnabled);
    silenceAmount.setEnabled (noiseEnabled);
    preserveAmount.setEnabled (noiseEnabled);
    learnNoise.setEnabled (noiseEnabled);

    const bool enhancerEnabled = ! audioEngine.isBypassed();
    preset.setEnabled (enhancerEnabled);
    enhanceKnob.setEnabled (enhancerEnabled);
    bassKnob.setEnabled (enhancerEnabled);
    trebleKnob.setEnabled (enhancerEnabled);
    vocalKnob.setEnabled (enhancerEnabled);
    stereoKnob.setEnabled (enhancerEnabled);
    gainReductionMeter.setEnabled (enhancerEnabled);

    if (enhancerPower.getToggleState() == audioEngine.isBypassed())
        enhancerPower.setToggleState (! audioEngine.isBypassed(), juce::dontSendNotification);
    if (noiseBypass.getToggleState() != audioEngine.getNoiseClean().isEnabled())
        noiseBypass.setToggleState (audioEngine.getNoiseClean().isEnabled(), juce::dontSendNotification);

    cpuStat.setValue (juce::String (status.cpuPercent, 1) + "% / peak "
                      + juce::String (status.callbackPeakPercent, 0) + "%",
                      status.callbackPeakPercent > 75.0f ? colours::red : colours::text);
    const double latencyMs = status.sampleRate > 0.0
                           ? static_cast<double> (status.totalLatencySamples) / status.sampleRate * 1000.0
                           : 0.0;
    latencyStat.setValue (juce::String (latencyMs, 1) + " ms",
                          latencyMs > 30.0 ? colours::amber : colours::text);
    dropoutsStat.setValue (juce::String (status.dropouts),
                           status.dropouts > 0 ? colours::red : colours::text);
    profileStat.setValue (cleaner.hasProfile() ? "Learned" : "Pending",
                          cleaner.hasProfile() ? colours::green : colours::textMuted);
    trayStat.setValue (status.running ? "Ready" : "Stopped",
                       status.running ? colours::cyan : colours::amber);

    repaint (0, 0, getWidth(), headerHeight);

    if (! updatingControls)
    {
        if (status.inputDevice.isNotEmpty() && inputDevice.getText() != status.inputDevice)
            setComboTextWithoutNotification (inputDevice, status.inputDevice);
        if (status.outputDevice.isNotEmpty() && outputDevice.getText() != status.outputDevice)
            setComboTextWithoutNotification (outputDevice, status.outputDevice);
    }
}

void LiveMainComponent::showSettingsCard()
{
    auto card = std::make_unique<AudioSettingsCard> (
        audioEngine, [this] { refreshAllControls(); });
    juce::CallOutBox::launchAsynchronously (std::move (card),
                                            settingsButton.getScreenBounds(), nullptr);
}

void LiveMainComponent::showExternalVstRoadmap()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "External VST Rack",
        "The rack reserves pre-enhancer and post-enhancer VST3 slots. "
        "They remain disabled until crash-isolated scanning, plugin latency reporting, "
        "state recall and safe bypass are complete. See docs/VST3_HOSTING_ROADMAP.md.");
}

void LiveMainComponent::setComboTextWithoutNotification (juce::ComboBox& combo,
                                                         const juce::String& text)
{
    selectComboText (combo, text);
}

void LiveMainComponent::showError (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, message);
}

} // namespace askp::live
