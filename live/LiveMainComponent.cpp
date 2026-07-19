#include "LiveMainComponent.h"

#include "BinaryData.h"

#include <initializer_list>
#include <memory>

namespace askp::live {
namespace {
constexpr int headerHeight = metrics::headerHeight;
constexpr int footerHeight = metrics::footerHeight;
constexpr int outerPad = metrics::space12;

const juce::Image& getBrandLogo()
{
    static juce::Image image = []
    {
        int bytes = 0;
        if (const auto* data = BinaryData::getNamedResource ("askp_app_logo_png", bytes))
            if (auto img = juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (bytes)); img.isValid())
                return img;
        if (const auto* data = BinaryData::getNamedResource ("favicon_96x96_png", bytes))
            if (auto img = juce::ImageFileFormat::loadFrom (data, static_cast<size_t> (bytes)); img.isValid())
                return img;
        return juce::Image();
    }();
    return image;
}

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

struct WorkspaceLayout
{
    juce::Rectangle<int> header;
    juce::Rectangle<int> footer;
    juce::Rectangle<int> leftPanel;
    juce::Rectangle<int> rightPanel;
    juce::Rectangle<int> ioStrip;
    juce::Rectangle<int> noiseCard;
    juce::Rectangle<int> enhancerCard;

    juce::Rectangle<int> logo;
    juce::Rectangle<int> statusPill;
    juce::Rectangle<int> settings;
    juce::Rectangle<int> tray;
    juce::Rectangle<int> minimise;
    juce::Rectangle<int> maximise;
    juce::Rectangle<int> close;

    juce::Rectangle<int> leftTitle;
    juce::Rectangle<int> inputTrimRow;
    juce::Rectangle<int> noiseRow;
    juce::Rectangle<int> vstPreRow;
    juce::Rectangle<int> inputFader;

    juce::Rectangle<int> rightTitle;
    juce::Rectangle<int> vstPostRow;
    juce::Rectangle<int> outputTrimRow;
    juce::Rectangle<int> limiterRow;
    juce::Rectangle<int> outputFader;

    juce::Rectangle<int> inputLabel;
    juce::Rectangle<int> inputCombo;
    juce::Rectangle<int> outputLabel;
    juce::Rectangle<int> outputCombo;

    juce::Rectangle<int> noiseHeader;
    juce::Rectangle<int> noiseBypass;
    juce::Rectangle<int> noiseKnobs[3];
    juce::Rectangle<int> profile;
    juce::Rectangle<int> learn;
    juce::Rectangle<int> expanderMeter;
    juce::Rectangle<int> visualizer;
    juce::Rectangle<int> noiseDivider;

    juce::Rectangle<int> enhancerHeader;
    juce::Rectangle<int> enhancerBypass;
    juce::Rectangle<int> presetCaption;
    juce::Rectangle<int> presetCombo;
    juce::Rectangle<int> enhancerKnobs[5];

    juce::Rectangle<int> stats[5];
};

WorkspaceLayout makeWorkspaceLayout (juce::Rectangle<int> bounds)
{
    WorkspaceLayout l;
    l.header = bounds.removeFromTop (headerHeight);
    l.footer = bounds.removeFromBottom (footerHeight);

    l.logo = { metrics::space16, metrics::space8, 42, 42 };
    const int headerButton = 36;
    int x = l.header.getRight() - metrics::space12;
    l.close = { x - headerButton, metrics::space12, headerButton, headerButton };
    x -= headerButton + metrics::space4;
    l.maximise = { x - headerButton, metrics::space12, headerButton, headerButton };
    x -= headerButton + metrics::space4;
    l.minimise = { x - headerButton, metrics::space12, headerButton, headerButton };
    x -= headerButton + metrics::space8;
    l.tray = { x - headerButton, metrics::space12, headerButton, headerButton };
    x -= headerButton + metrics::space4;
    l.settings = { x - headerButton, metrics::space12, headerButton, headerButton };
    l.statusPill = { 276, 15, juce::jlimit (250, 340, x - 294), 30 };

    auto workspace = bounds.reduced (outerPad, metrics::space10);
    const int sideWidth = juce::jlimit (224, metrics::sidePanelWidth,
                                        static_cast<int> (workspace.getWidth() * 0.172f));
    l.leftPanel = workspace.removeFromLeft (sideWidth);
    workspace.removeFromLeft (metrics::space12);
    l.rightPanel = workspace.removeFromRight (sideWidth);
    workspace.removeFromRight (metrics::space12);

    auto centre = workspace;
    l.ioStrip = centre.removeFromTop (48);
    centre.removeFromTop (metrics::space10);
    const int noiseHeight = juce::jlimit (328, 356,
                                          static_cast<int> (centre.getHeight() * 0.545f));
    l.noiseCard = centre.removeFromTop (noiseHeight);
    centre.removeFromTop (metrics::space10);
    l.enhancerCard = centre;

    auto left = l.leftPanel.reduced (metrics::space12);
    l.leftTitle = left.removeFromTop (42);
    l.inputTrimRow = left.removeFromTop (metrics::rackRowHeight);
    left.removeFromTop (metrics::space6);
    l.noiseRow = left.removeFromTop (metrics::rackRowHeight);
    left.removeFromTop (metrics::space6);
    l.vstPreRow = left.removeFromTop (metrics::rackRowHeight);
    left.removeFromTop (metrics::space10);
    l.inputFader = left;

    auto right = l.rightPanel.reduced (metrics::space12);
    l.rightTitle = right.removeFromTop (42);
    l.vstPostRow = right.removeFromTop (metrics::rackRowHeight);
    right.removeFromTop (metrics::space6);
    l.outputTrimRow = right.removeFromTop (metrics::rackRowHeight);
    right.removeFromTop (metrics::space6);
    l.limiterRow = right.removeFromTop (metrics::rackRowHeight);
    right.removeFromTop (metrics::space10);
    l.outputFader = right;

    auto io = l.ioStrip;
    auto inputHalf = io.removeFromLeft (io.getWidth() / 2).reduced (metrics::space10, metrics::space4);
    auto outputHalf = io.reduced (metrics::space10, metrics::space4);
    l.inputLabel = inputHalf.removeFromLeft (52);
    l.inputCombo = inputHalf;
    l.outputLabel = outputHalf.removeFromLeft (58);
    l.outputCombo = outputHalf;

    auto noiseInner = l.noiseCard.reduced (metrics::space14, metrics::space12);
    l.noiseHeader = noiseInner.removeFromTop (40);
    l.noiseBypass = l.noiseHeader.removeFromRight (34).withSizeKeepingCentre (34, 34);
    auto noiseBody = noiseInner;
    const int controlWidth = juce::jlimit (350, 382,
                                           static_cast<int> (noiseBody.getWidth() * 0.42f));
    auto controls = noiseBody.removeFromLeft (controlWidth);
    l.noiseDivider = noiseBody.removeFromLeft (metrics::space16)
                            .withSizeKeepingCentre (1, juce::jmax (80, noiseBody.getHeight() - 24));
    l.visualizer = noiseBody.reduced (metrics::space4, 0);

    auto meter = controls.removeFromRight (72);
    controls.removeFromRight (metrics::space12);
    auto knobRow = controls.removeFromTop (140);
    const int knobWidth = knobRow.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        l.noiseKnobs[i] = knobRow.removeFromLeft (i == 2 ? knobRow.getWidth() : knobWidth).reduced (metrics::space4);
    controls.removeFromTop (metrics::space10);
    l.profile = controls.removeFromTop (34);
    controls.removeFromTop (metrics::space8);
    l.learn = controls.removeFromTop (36);
    l.expanderMeter = meter.withSizeKeepingCentre (68, juce::jmin (186, meter.getHeight() - 8));

    auto enhancerInner = l.enhancerCard.reduced (metrics::space14, metrics::space12);
    l.enhancerHeader = enhancerInner.removeFromTop (40);
    l.enhancerBypass = l.enhancerHeader.removeFromRight (34).withSizeKeepingCentre (34, 34);
    auto presetStrip = enhancerInner.removeFromTop (58);
    auto captionRow = presetStrip.removeFromTop (15);
    l.presetCaption = captionRow.removeFromLeft (350);
    auto comboRow = presetStrip.removeFromTop (38);
    l.presetCombo = comboRow.removeFromLeft (350);
    enhancerInner.removeFromTop (metrics::space8);
    const int enhancerKnobWidth = enhancerInner.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        l.enhancerKnobs[i] = enhancerInner.removeFromLeft (i == 4 ? enhancerInner.getWidth() : enhancerKnobWidth)
                                                   .reduced (metrics::space6, metrics::space2);

    auto footer = l.footer.reduced (metrics::space12, 0);
    const int statWidth = footer.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
        l.stats[i] = footer.removeFromLeft (i == 4 ? footer.getWidth() : statWidth);

    return l;
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

    settingsButton.setOffColour (colours::textMuted);
    trayButton.setOffColour (colours::textMuted);
    minimiseButton.setOffColour (colours::textMuted);
    maximiseButton.setOffColour (colours::textMuted);
    closeButton.setOffColour (colours::textMuted);
    noiseBypass.setOffColour (colours::textMuted.withAlpha (0.78f));
    enhancerPower.setOffColour (colours::textMuted.withAlpha (0.78f));

    gainReductionMeter.setVertical (true);
    gainReductionMeter.setTitle ("EXPANDER");

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

    noiseBypass.setTooltip ("Noise Clean bypass: disables spectral denoise and smooth expander");
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

    enhancerPower.setTooltip ("Smart Sound Enhancer bypass: keeps the live engine running while muting enhancer processing");
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
        if (! updatingControls)
        {
            if (audioEngine.isInputTrimBypassed())
            {
                audioEngine.setInputTrimBypassed (false);
                inputTrimRow.setBypassed (false);
            }
            audioEngine.setInputTrimDb (valueDb);
        }
    };
    outputFader.onValueChanged = [this] (float valueDb)
    {
        if (! updatingControls)
        {
            if (audioEngine.isOutputTrimBypassed())
            {
                audioEngine.setOutputTrimBypassed (false);
                outputTrimRow.setBypassed (false);
            }
            audioEngine.setOutputTrimDb (valueDb);
        }
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
    const auto layout = makeWorkspaceLayout (getLocalBounds());
    g.fillAll (colours::appBg);

    juce::ColourGradient headerGradient (juce::Colour (0xff0d1725), 0.0f, 0.0f,
                                         colours::appBg, static_cast<float> (getWidth()), 0.0f, false);
    headerGradient.addColour (0.58, colours::panelLo.withAlpha (0.98f));
    g.setGradientFill (headerGradient);
    g.fillRect (layout.header);
    g.setColour (colours::border);
    g.drawHorizontalLine (layout.header.getBottom() - 1, 0.0f, static_cast<float> (getWidth()));

    auto& lf = lookAndFeel;
    auto logoBox = layout.logo.toFloat();
    juce::ColourGradient logoSurface (colours::panelHi.brighter (0.035f), logoBox.getX(), logoBox.getY(),
                                      colours::panelLo, logoBox.getRight(), logoBox.getBottom(), false);
    g.setGradientFill (logoSurface);
    g.fillRoundedRectangle (logoBox, 10.0f);
    g.setColour (colours::border.withAlpha (0.92f));
    g.drawRoundedRectangle (logoBox.reduced (0.5f), 10.0f, 1.0f);
    if (const auto& brandLogo = getBrandLogo(); brandLogo.isValid())
        g.drawImageWithin (brandLogo, layout.logo.getX() + 5, layout.logo.getY() + 5,
                           layout.logo.getWidth() - 10, layout.logo.getHeight() - 10,
                           juce::RectanglePlacement::centred, false);

    g.setFont (lf.jakarta (18.0f, true));
    g.setColour (colours::text);
    g.drawText ("Sonkupik Live", layout.logo.getRight() + 12, 0, 188, headerHeight,
                juce::Justification::centredLeft, false);
    g.setColour (colours::border);
    g.drawVerticalLine (260, 15.0f, 45.0f);

    const auto headerStatus = audioEngine.getStatus();
    const auto statusColour = ! headerStatus.deviceOpen ? colours::red
                            : ! audioEngine.isEngineEnabled() ? colours::amber
                            : headerStatus.outputIsVirtualCable ? colours::green : colours::amber;
    const juce::String statusText = ! headerStatus.deviceOpen ? "AUDIO DISCONNECTED"
                                  : ! audioEngine.isEngineEnabled() ? "ENGINE STOPPED"
                                  : headerStatus.outputIsVirtualCable ? "LIVE ENGINE  /  VB-CABLE READY"
                                  : "LIVE ENGINE  /  SELECT VB-CABLE";
    const auto pill = layout.statusPill.toFloat();
    g.setColour (statusColour.withAlpha (0.075f));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (statusColour.withAlpha (0.26f));
    g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
    g.setColour (statusColour);
    g.fillEllipse (pill.getX() + 11.0f, pill.getCentreY() - 3.5f, 7.0f, 7.0f);
    g.setFont (lf.jakarta (10.0f, true));
    g.drawText (statusText, layout.statusPill.reduced (25, 0), juce::Justification::centredLeft, false);

    fillPremiumPanel (g, layout.leftPanel.toFloat(), metrics::cardRadius);
    fillPremiumPanel (g, layout.rightPanel.toFloat(), metrics::cardRadius);
    fillPremiumPanel (g, layout.ioStrip.toFloat(), metrics::controlRadius);
    fillPremiumPanel (g, layout.noiseCard.toFloat(), metrics::cardRadius);
    fillPremiumPanel (g, layout.enhancerCard.toFloat(), metrics::cardRadius);

    drawSectionTitle (g, layout.leftTitle.reduced (2, 0).toFloat(), "BEFORE ENHANCER");
    drawSectionTitle (g, layout.rightTitle.reduced (2, 0).toFloat(), "AFTER CHAIN");
    drawFieldLabel (g, lf, layout.inputLabel, "Input");
    drawFieldLabel (g, lf, layout.outputLabel, "Output");
    drawSectionTitle (g, layout.noiseHeader.toFloat(), "MUSICAL NOISE CLEAN");
    drawSectionTitle (g, layout.enhancerHeader.toFloat(), "SMART SOUND ENHANCER");

    g.setColour (colours::borderSoft.withAlpha (0.48f));
    g.drawHorizontalLine (layout.noiseHeader.getBottom() + 1,
                          (float) layout.noiseCard.getX() + metrics::space14,
                          (float) layout.noiseCard.getRight() - metrics::space14);
    g.drawHorizontalLine (layout.enhancerHeader.getBottom() + 1,
                          (float) layout.enhancerCard.getX() + metrics::space14,
                          (float) layout.enhancerCard.getRight() - metrics::space14);

    g.setColour (colours::borderSoft.withAlpha (0.78f));
    g.fillRect (layout.noiseDivider.toFloat());

    g.setColour (colours::cyan.withAlpha (0.70f));
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) layout.presetCaption.getX(),
                                                     (float) layout.presetCaption.getBottom() - 2.0f,
                                                     18.0f, 2.0f), 1.0f);
    g.setColour (colours::textMuted);
    g.setFont (lf.jakarta (9.0f, true));
    g.drawText ("PRESET", layout.presetCaption, juce::Justification::centredLeft, false);

    g.setColour (colours::border);
    g.drawHorizontalLine (layout.footer.getY(), 12.0f, static_cast<float> (getWidth() - 12));
}

void LiveMainComponent::resized()
{
    const auto layout = makeWorkspaceLayout (getLocalBounds());

    settingsButton.setBounds (layout.settings);
    trayButton.setBounds (layout.tray);
    minimiseButton.setBounds (layout.minimise);
    maximiseButton.setBounds (layout.maximise);
    closeButton.setBounds (layout.close);

    inputTrimRow.setBounds (layout.inputTrimRow);
    noiseRow.setBounds (layout.noiseRow);
    vstPreRow.setBounds (layout.vstPreRow);
    inputFader.setBounds (layout.inputFader);

    vstPostRow.setBounds (layout.vstPostRow);
    outputTrimRow.setBounds (layout.outputTrimRow);
    limiterRow.setBounds (layout.limiterRow);
    outputFader.setBounds (layout.outputFader);

    inputDevice.setBounds (layout.inputCombo);
    outputDevice.setBounds (layout.outputCombo);

    noiseBypass.setBounds (layout.noiseBypass);
    noiseReduction.setBounds (layout.noiseKnobs[0]);
    silenceAmount.setBounds (layout.noiseKnobs[1]);
    preserveAmount.setBounds (layout.noiseKnobs[2]);
    profileStatus.setBounds (layout.profile);
    learnNoise.setBounds (layout.learn);
    gainReductionMeter.setBounds (layout.expanderMeter);
    visualizer.setBounds (layout.visualizer);

    enhancerPower.setBounds (layout.enhancerBypass);
    preset.setBounds (layout.presetCombo);
    enhanceKnob.setBounds (layout.enhancerKnobs[0]);
    bassKnob.setBounds (layout.enhancerKnobs[1]);
    trebleKnob.setBounds (layout.enhancerKnobs[2]);
    vocalKnob.setBounds (layout.enhancerKnobs[3]);
    stereoKnob.setBounds (layout.enhancerKnobs[4]);

    cpuStat.setBounds (layout.stats[0]);
    latencyStat.setBounds (layout.stats[1]);
    dropoutsStat.setBounds (layout.stats[2]);
    profileStat.setBounds (layout.stats[3]);
    trayStat.setBounds (layout.stats[4]);
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
    gainReductionMeter.setReduction (status.expanderReductionDb);

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
    gainReductionMeter.setEnabled (noiseEnabled);

    const bool enhancerEnabled = ! audioEngine.isBypassed();
    preset.setEnabled (enhancerEnabled);
    enhanceKnob.setEnabled (enhancerEnabled);
    bassKnob.setEnabled (enhancerEnabled);
    trebleKnob.setEnabled (enhancerEnabled);
    vocalKnob.setEnabled (enhancerEnabled);
    stereoKnob.setEnabled (enhancerEnabled);

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
