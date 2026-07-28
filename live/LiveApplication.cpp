#include "LiveMainComponent.h"
#include "LiveAudioEngine.h"
#include "LiveIcons.h"
#include "LiveTheme.h"

#include "BinaryData.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <initializer_list>

namespace askp::live {

class MiniControllerComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    MiniControllerComponent (LiveAudioEngine& engine, std::function<void()> openMain,
                             std::function<void()> closeMini)
        : audioEngine (engine), openMainWindow (std::move (openMain)), closeMiniWindow (std::move (closeMini))
    {
        setLookAndFeel (&lookAndFeel);
        for (juce::Component* component : std::initializer_list<juce::Component*> {
                 &closeButton, &powerButton, &openButton, &bypassToggle,
                 &restartButton, &presetBox, &outputStatus })
            addAndMakeVisible (component);

        closeButton.onClick = [this] { if (closeMiniWindow) closeMiniWindow(); };
        powerButton.setClickingTogglesState (true);
        powerButton.setToggleState (audioEngine.isEngineEnabled(), juce::dontSendNotification);
        powerButton.onClick = [this] { audioEngine.setEngineEnabled (powerButton.getToggleState()); };
        openButton.onClick = [this] { if (openMainWindow) openMainWindow(); };
        bypassToggle.onClick = [this] { audioEngine.setBypassed (bypassToggle.getToggleState()); };
        restartButton.onClick = [this] { audioEngine.restartAudio(); };

        presetBox.addItemList (audioEngine.getPresetNames(), 1);
        presetBox.setSelectedItemIndex (audioEngine.getPresetIndex(), juce::dontSendNotification);
        presetBox.onChange = [this]
        {
            if (presetBox.getSelectedItemIndex() >= 0)
                audioEngine.setPresetIndex (presetBox.getSelectedItemIndex());
        };

        startTimerHz (12);
    }

    ~MiniControllerComponent() override
    {
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::appBg);
        fillPremiumPanel (g, getLocalBounds().toFloat().reduced (0.5f), 12.0f);

        if (auto icon = createLucideIcon ("audio_waveform_svg", colours::cyan))
            icon->drawWithin (g, juce::Rectangle<float> (16.0f, 14.0f, 20.0f, 20.0f),
                              juce::RectanglePlacement::centred, 1.0f);
        g.setColour (colours::text);
        g.setFont (lookAndFeel.jakarta (14.5f, true));
        g.drawText ("Sonkupik Live", 45, 6, 190, 38, juce::Justification::centredLeft, false);
        g.setColour (colours::border);
        g.drawHorizontalLine (44, 12.0f, static_cast<float> (getWidth() - 12));

        auto powerArea = juce::Rectangle<float> (20.0f, 66.0f, 98.0f, 98.0f);
        g.setColour (powerButton.getToggleState() ? colours::cyan.withAlpha (0.16f)
                                                  : colours::red.withAlpha (0.10f));
        g.fillEllipse (powerArea);
        g.setColour (powerButton.getToggleState() ? colours::cyan : colours::red);
        g.drawEllipse (powerArea.reduced (6.0f), 2.0f);
        g.setFont (lookAndFeel.jakarta (11.5f, true));
        g.drawText (powerButton.getToggleState() ? "ON" : "OFF", 20, 164, 98, 24,
                    juce::Justification::centred, false);

        g.setColour (colours::textMuted);
        g.setFont (lookAndFeel.jakarta (9.5f, true));
        g.drawText ("PRESET", 138, 162, 120, 20, juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 44, 7, 36, 36);
        powerButton.setBounds (42, 87, 54, 54);
        openButton.setBounds (136, 62, 138, 36);
        bypassToggle.setBounds (136, 104, 138, 32);
        restartButton.setBounds (136, 138, 138, 36);
        presetBox.setBounds (136, 184, 138, 38);
        outputStatus.setBounds (20, getHeight() - 50, getWidth() - 40, 36);
    }

private:
    void timerCallback() override
    {
        const auto status = audioEngine.getStatus();
        powerButton.setToggleState (audioEngine.isEngineEnabled(), juce::dontSendNotification);
        bypassToggle.setToggleState (audioEngine.isBypassed(), juce::dontSendNotification);
        outputStatus.setStatus (status.outputIsVirtualCable ? "VB-CABLE Ready" : "Output needs setup",
                                status.outputIsVirtualCable ? colours::green : colours::amber,
                                status.outputIsVirtualCable);
        repaint();
    }

    LiveAudioEngine& audioEngine;
    std::function<void()> openMainWindow;
    std::function<void()> closeMiniWindow;
    LiveLookAndFeel lookAndFeel;
    LucideIconButton closeButton { "Close", "x_svg", colours::textMuted };
    LucideIconButton powerButton { "Power", "power_svg", colours::cyan };
    juce::TextButton openButton { "Open Control Panel" };
    juce::ToggleButton bypassToggle { "Bypass" };
    juce::TextButton restartButton { "Restart Audio" };
    juce::ComboBox presetBox;
    StatusPill outputStatus;
};

class LiveMainWindow final : public juce::DocumentWindow
{
public:
    LiveMainWindow (LiveAudioEngine& engine,
                    std::function<void()> showMini,
                    std::function<void()> hideMain)
        : juce::DocumentWindow ("Sonkupik Live", colours::appBg,
                                juce::DocumentWindow::allButtons, true),
          hideMainWindow (std::move (hideMain))
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0);
        setResizable (true, true);
        setResizeLimits (1180, 720, 1920, 1200);
        setDropShadowEnabled (true);

        LiveMainComponent::WindowActions actions;
        actions.minimise = [this] { setMinimised (true); };
        actions.toggleMaximise = [this]
        {
            if (isFullScreen())
                setFullScreen (false);
            else
                setFullScreen (true);
        };
        actions.hideToTray = [this]
        {
            if (hideMainWindow) hideMainWindow(); else setVisible (false);
        };
        actions.showMiniController = std::move (showMini);

        mainComponent = new LiveMainComponent (engine, std::move (actions));
        setContentOwned (mainComponent, true);
        centreWithSize (1440, 860);
    }

    void closeButtonPressed() override
    {
        if (hideMainWindow) hideMainWindow(); else setVisible (false);
    }

    LiveMainComponent* getMainComponent() const noexcept { return mainComponent; }

private:
    std::function<void()> hideMainWindow;
    LiveMainComponent* mainComponent = nullptr;
};

class TrayIcon final : public juce::SystemTrayIconComponent
{
public:
    TrayIcon (LiveAudioEngine& engine,
              std::function<void()> showMain,
              std::function<void()> showMini,
              std::function<void()> quit)
        : audioEngine (engine), showMainWindow (std::move (showMain)),
          showMiniController (std::move (showMini)), quitApplication (std::move (quit))
    {
        int iconBytes = 0;
        const auto* iconData = BinaryData::getNamedResource ("favicon_96x96_png", iconBytes);
        if (iconData == nullptr)
            iconData = BinaryData::getNamedResource ("askp_app_logo_png", iconBytes);
        if (iconData != nullptr && iconBytes > 0)
        {
            const auto icon = juce::ImageCache::getFromMemory (iconData, iconBytes);
            if (icon.isValid())
                setIconImage (icon, icon);
        }
        setIconTooltip ("Sonkupik Live - audio continues in the background");
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Open Control Panel");
            menu.addItem (2, "Compact Controller");
            menu.addSeparator();
            menu.addItem (3, audioEngine.isEngineEnabled() ? "Stop Engine" : "Start Engine");
            menu.addItem (4, audioEngine.isBypassed() ? "Disable Bypass" : "Bypass Enhancer");
            menu.addItem (5, "Restart Audio Engine");
            menu.addSeparator();
            menu.addItem (6, "Exit Sonkupik Live");
            menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
            {
                switch (result)
                {
                    case 1: if (showMainWindow) showMainWindow(); break;
                    case 2: if (showMiniController) showMiniController(); break;
                    case 3: audioEngine.setEngineEnabled (! audioEngine.isEngineEnabled()); break;
                    case 4: audioEngine.setBypassed (! audioEngine.isBypassed()); break;
                    case 5: audioEngine.restartAudio(); break;
                    case 6: if (quitApplication) quitApplication(); break;
                    default: break;
                }
            });
        }
        else if (showMiniController)
        {
            showMiniController();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (showMainWindow) showMainWindow();
    }

private:
    LiveAudioEngine& audioEngine;
    std::function<void()> showMainWindow;
    std::function<void()> showMiniController;
    std::function<void()> quitApplication;
};

class SonkupikLiveApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Sonkupik Live"; }
    const juce::String getApplicationVersion() override { return "0.13.1"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Sonkupik Live";
        options.filenameSuffix = ".settings";
        options.folderName = "MasArray";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        properties.setStorageParameters (options);

        std::unique_ptr<juce::XmlElement> deviceState;
        if (auto* file = properties.getUserSettings())
        {
            const auto xml = file->getValue ("audioDeviceState");
            if (xml.isNotEmpty())
                deviceState = juce::XmlDocument::parse (xml);
        }

        audioEngine = std::make_unique<LiveAudioEngine>();
        if (auto* file = properties.getUserSettings())
        {
            audioEngine->setInputChannelMode (file->getIntValue ("inputChannelMode", 2));
            audioEngine->setInputTrimDb (static_cast<float> (file->getDoubleValue ("inputTrimDb", 0.0)));
            audioEngine->setOutputTrimDb (static_cast<float> (file->getDoubleValue ("outputTrimDb", 0.0)));
            audioEngine->setInputTrimBypassed (file->getBoolValue ("inputTrimBypassed", false));
            audioEngine->setOutputTrimBypassed (file->getBoolValue ("outputTrimBypassed", false));
            audioEngine->setPresetIndex (file->getIntValue ("presetIndex", 0));
            audioEngine->setEnhance (static_cast<float> (file->getDoubleValue ("enhance", audioEngine->getEnhance())));
            audioEngine->setSmartBass (static_cast<float> (file->getDoubleValue ("smartBass", audioEngine->getSmartBass())));
            audioEngine->setSmartTreble (static_cast<float> (file->getDoubleValue ("smartTreble", audioEngine->getSmartTreble())));
            audioEngine->setVocalBody (static_cast<float> (file->getDoubleValue ("vocalBody", audioEngine->getVocalBody())));
            audioEngine->setStereoMagic (static_cast<float> (file->getDoubleValue ("stereoMagic", audioEngine->getStereoMagic())));
            audioEngine->getNoiseClean().setReductionDb (static_cast<float> (file->getDoubleValue ("noiseReductionDb", 5.0)));
            audioEngine->getNoiseClean().setPreserve (static_cast<float> (file->getDoubleValue ("noisePreserve", 0.75)));
            audioEngine->getNoiseClean().setQuality (file->getIntValue ("noiseQuality", 0) == 1
                ? MusicalNoiseClean::Quality::clean512
                : MusicalNoiseClean::Quality::live256);
            audioEngine->getExpander().setAmount (static_cast<float> (file->getDoubleValue ("expanderAmount", 0.55)));
            const bool noiseEnabled = file->getBoolValue ("noiseCleanEnabled", true);
            audioEngine->getNoiseClean().setEnabled (noiseEnabled);
            audioEngine->getExpander().setEnabled (noiseEnabled);
            audioEngine->setBypassed (file->getBoolValue ("enhancerBypassed", false));
            audioEngine->setEngineEnabled (file->getBoolValue ("engineEnabled", true));
        }
        audioEngine->initialise (deviceState.get());

        mainWindow = std::make_unique<LiveMainWindow> (
            *audioEngine,
            [this] { showMiniController(); },
            [this] { hideMainWindow(); });

        trayIcon = std::make_unique<TrayIcon> (
            *audioEngine,
            [this] { showMainWindow(); },
            [this] { showMiniController(); },
            [this] { quit(); });

        showMainWindow();
    }

    void shutdown() override
    {
        saveState();
        miniWindow.reset();
        trayIcon.reset();
        mainWindow.reset();
        if (audioEngine != nullptr)
            audioEngine->shutdown();
        audioEngine.reset();
        properties.closeFiles();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        showMainWindow();
    }

private:
    void showMainWindow()
    {
        if (mainWindow == nullptr)
            return;
        mainWindow->setVisible (true);
        mainWindow->setMinimised (false);
        mainWindow->toFront (true);
        if (auto* component = mainWindow->getMainComponent())
        {
            component->setUiActive (true);
            component->refreshAllControls();
        }
        if (miniWindow != nullptr)
            miniWindow->setVisible (false);
    }

    void hideMainWindow()
    {
        if (mainWindow != nullptr)
        {
            if (auto* component = mainWindow->getMainComponent())
                component->setUiActive (false);
            mainWindow->setVisible (false);
        }
    }

    void showMiniController()
    {
        if (audioEngine == nullptr)
            return;

        if (miniWindow == nullptr)
        {
            miniWindow = std::make_unique<juce::DocumentWindow> (
                "Sonkupik Live", colours::appBg, 0, true);
            miniWindow->setUsingNativeTitleBar (false);
            miniWindow->setTitleBarHeight (0);
            miniWindow->setAlwaysOnTop (true);
            miniWindow->setResizable (false, false);
            miniWindow->setDropShadowEnabled (true);
            miniWindow->setContentOwned (new MiniControllerComponent (
                *audioEngine,
                [this] { showMainWindow(); },
                [this] { if (miniWindow != nullptr) miniWindow->setVisible (false); }), true);
            miniWindow->setSize (300, 290);
        }

        auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        if (display != nullptr)
        {
            const auto work = display->userBounds;
            miniWindow->setTopLeftPosition (
                juce::roundToInt (work.getRight() - static_cast<float> (miniWindow->getWidth()) - 18.0f),
                juce::roundToInt (work.getBottom() - static_cast<float> (miniWindow->getHeight()) - 18.0f));
        }
        miniWindow->setVisible (true);
        miniWindow->toFront (true);
    }

    void saveState()
    {
        if (audioEngine == nullptr)
            return;
        if (auto* file = properties.getUserSettings())
        {
            if (auto state = audioEngine->createDeviceState())
                file->setValue ("audioDeviceState", state->toString());
            file->setValue ("presetIndex", audioEngine->getPresetIndex());
            file->setValue ("inputChannelMode", audioEngine->getInputChannelMode());
            file->setValue ("inputTrimDb", audioEngine->getInputTrimDb());
            file->setValue ("outputTrimDb", audioEngine->getOutputTrimDb());
            file->setValue ("inputTrimBypassed", audioEngine->isInputTrimBypassed());
            file->setValue ("outputTrimBypassed", audioEngine->isOutputTrimBypassed());
            file->setValue ("enhance", audioEngine->getEnhance());
            file->setValue ("smartBass", audioEngine->getSmartBass());
            file->setValue ("smartTreble", audioEngine->getSmartTreble());
            file->setValue ("vocalBody", audioEngine->getVocalBody());
            file->setValue ("stereoMagic", audioEngine->getStereoMagic());
            file->setValue ("noiseReductionDb", audioEngine->getNoiseClean().getReductionDb());
            file->setValue ("noisePreserve", audioEngine->getNoiseClean().getPreserve());
            file->setValue ("noiseQuality", audioEngine->getNoiseClean().getQuality() == MusicalNoiseClean::Quality::clean512 ? 1 : 0);
            file->setValue ("expanderAmount", audioEngine->getExpander().getAmount());
            file->setValue ("noiseCleanEnabled", audioEngine->getNoiseClean().isEnabled());
            file->setValue ("enhancerBypassed", audioEngine->isBypassed());
            file->setValue ("engineEnabled", audioEngine->isEngineEnabled());
            file->saveIfNeeded();
        }
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<LiveAudioEngine> audioEngine;
    std::unique_ptr<LiveMainWindow> mainWindow;
    std::unique_ptr<juce::DocumentWindow> miniWindow;
    std::unique_ptr<TrayIcon> trayIcon;
};

} // namespace askp::live

START_JUCE_APPLICATION (askp::live::SonkupikLiveApplication)
