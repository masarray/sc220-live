# P11 - Professional digital mixer FX workflow. Runs AFTER P10 and never
# rewrites DSP/monitor generation. The approved SC220 SVG fader stays untouched.
if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P11 requires generated UI")
endif()
file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p11_once variable label anchor replacement)
    set(_s "${${variable}}")
    string(FIND "${_s}" "${anchor}" _i)
    if(_i EQUAL -1)
        message(FATAL_ERROR "SC220 P11 UI anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _s "${_s}")
    set(${variable} "${_s}" PARENT_SCOPE)
endfunction()
function(sc220_p11_range variable label start_marker end_marker replacement)
    set(_s "${${variable}}")
    string(FIND "${_s}" "${start_marker}" _a)
    string(FIND "${_s}" "${end_marker}" _b)
    if(_a EQUAL -1 OR _b EQUAL -1 OR _b LESS _a)
        message(FATAL_ERROR "SC220 P11 UI range moved: ${label}")
    endif()
    string(SUBSTRING "${_s}" 0 ${_a} _prefix)
    string(SUBSTRING "${_s}" ${_b} -1 _suffix)
    set(${variable} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

sc220_p11_once(_ui "three FX controls per channel"
[=[    for (int channel = 0; channel < 2; ++channel)
        for (int slot = 0; slot < 3; ++slot)
        {
            auto& button = channelInsertButtons[static_cast<size_t>(channel)][static_cast<size_t>(slot)];
            addAndMakeVisible (button);
            button.setButtonText (juce::String (slot + 1) + " FX");
            button.setTooltip ("Click for Insert FX load, edit, bypass or remove");
            button.onClick = [this, channel, slot] { showChannelInsertMenu (channel, slot); };
        }
]=]
[=[    for (int channel = 0; channel < 2; ++channel)
        for (int slot = 0; slot < 3; ++slot)
        {
            const auto c = static_cast<size_t> (channel);
            const auto s = static_cast<size_t> (slot);
            auto& button = channelInsertButtons[c][s];
            auto& power = channelInsertPowers[c][s];
            auto& menu = channelInsertMenus[c][s];
            addAndMakeVisible (button);
            addAndMakeVisible (power);
            addAndMakeVisible (menu);
            button.getProperties().set ("sc220FxSlot", true);
            power.getProperties().set ("sc220FxPower", true);
            menu.getProperties().set ("sc220FxMenu", true);
            // One-click DAW workflow: empty -> Explorer, occupied -> VST editor.
            button.onClick = [this, channel, slot]
            {
                if (audioEngine.hasChannelFx (channel, slot))
                    showChannelInsertEditor (channel, slot);
                else
                    chooseChannelInsert (channel, slot);
            };
            // Separate power; never remove or open a VST to bypass its DSP.
            power.onClick = [this, channel, slot]
            {
                if (! audioEngine.hasChannelFx (channel, slot)) return;
                audioEngine.setChannelFxBypassed (channel, slot,
                    ! audioEngine.isChannelFxBypassed (channel, slot));
                refreshChannelInserts();
            };
            menu.setButtonText ("...");
            menu.onClick = [this, channel, slot] { showChannelInsertMenu (channel, slot); };
        }
]=])

sc220_p11_range(_ui "loaded click VST editor / empty click explorer and power refresh"
[=[void SC220MainComponent::refreshChannelInserts()]=] [=[void SC220MainComponent::showChannelInsertMenu]=]
[=[void SC220MainComponent::refreshChannelInserts()
{
    for (int channel = 0; channel < 2; ++channel)
        for (int slot = 0; slot < 3; ++slot)
        {
            const auto c = static_cast<size_t> (channel);
            const auto s = static_cast<size_t> (slot);
            auto& button = channelInsertButtons[c][s];
            auto& power = channelInsertPowers[c][s];
            auto& menu = channelInsertMenus[c][s];
            const auto name = audioEngine.getChannelFxName (channel, slot);
            const bool loaded = name.isNotEmpty();
            const bool bypassed = loaded && audioEngine.isChannelFxBypassed (channel, slot);
            const auto label = juce::String (slot + 1).paddedLeft ('0', 2) + "   "
                + (loaded ? name : juce::String ("+ LOAD VST3"));
            if (button.getButtonText() != label)
                button.setButtonText (label);
            const auto explanation = juce::String (channel == 0 ? "VOICE" : "PC MUSIC")
                + " / FX " + juce::String (slot + 1) + ": "
                + (loaded ? name + (bypassed ? " [OFF]" : " [ON]")
                          + ". Click to open editor; use ... to replace or remove."
                          : "Click to browse for a .vst3 effect.");
            button.setTooltip (explanation);
            power.setButtonText (bypassed || ! loaded ? "OFF" : "ON");
            power.setEnabled (loaded);
            power.setTooltip (loaded ? (bypassed ? "Enable this FX slot" : "Bypass this FX slot")
                                     : "Load a VST3 to enable this insert");
            menu.setTooltip (loaded ? "More: replace, open, bypass, remove"
                                    : "Browse for a VST3 plugin");
            for (auto* b : { &button, &power, &menu })
            {
                b->getProperties().set ("sc220FxLoaded", loaded);
                b->getProperties().set ("sc220FxBypassed", bypassed);
                b->repaint();
            }
        }
}

]=])

sc220_p11_range(_ui "FX context actions and replacement"
[=[void SC220MainComponent::showChannelInsertMenu]=] [=[void SC220MainComponent::chooseChannelInsert]=]
[=[void SC220MainComponent::showChannelInsertMenu (int channel, int slot)
{
    if (! juce::isPositiveAndBelow (channel, 2) || ! juce::isPositiveAndBelow (slot, 3)) return;
    auto& button = channelInsertButtons[static_cast<size_t>(channel)][static_cast<size_t>(slot)];
    const bool loaded = audioEngine.hasChannelFx (channel, slot);
    juce::PopupMenu menu;
    menu.addSectionHeader ((channel == 0 ? "SC220 VOICE" : "PC AUDIO")
        + juce::String ("  -  FX ") + juce::String (slot + 1));
    if (loaded)
        menu.addItem (1, audioEngine.getChannelFxName (channel, slot), false);
    menu.addItem (2, loaded ? "Replace VST3..." : "Load VST3...");
    menu.addItem (3, "Open VST editor", loaded);
    menu.addItem (4, audioEngine.isChannelFxBypassed (channel, slot)
                         ? "Enable insert" : "Bypass insert", loaded);
    menu.addItem (5, "Remove effect", loaded);
    juce::Component::SafePointer<SC220MainComponent> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (
                            &channelInsertMenus[static_cast<size_t>(channel)][static_cast<size_t>(slot)]),
        [safe, channel, slot] (int selected)
        {
            if (safe == nullptr) return;
            if (selected == 2) safe->chooseChannelInsert (channel, slot);
            else if (selected == 3) safe->showChannelInsertEditor (channel, slot);
            else if (selected == 4)
                safe->audioEngine.setChannelFxBypassed (channel, slot,
                    ! safe->audioEngine.isChannelFxBypassed (channel, slot));
            else if (selected == 5)
            {
                safe->channelInsertEditors[static_cast<size_t>(channel)][static_cast<size_t>(slot)].reset();
                safe->audioEngine.removeChannelFx (channel, slot);
            }
            if (selected == 4 || selected == 5) safe->refreshChannelInserts();
        });
}

]=])

sc220_p11_range(_ui "official editor window icon"
[=[class VoiceFxEditorWindow final]=] [=[void SC220MainComponent::showVoiceFxMenu]=]
[=[class VoiceFxEditorWindow final : public juce::DocumentWindow
{
public:
    VoiceFxEditorWindow (const juce::String& title, juce::AudioProcessorEditor* editor)
        : juce::DocumentWindow (title, palette::surface,
                                juce::DocumentWindow::closeButton, true)
    {
        setUsingNativeTitleBar (true);
        // Match the SC220 Live executable/tray identity instead of a VST vendor icon.
        setIcon (loadImage ("favicon_96x96_png"));
        setContentOwned (editor, true);
        setResizable (false, false);
        centreWithSize (juce::jmax (350, editor->getWidth()),
                        juce::jmax (250, editor->getHeight()));
    }

    void closeButtonPressed() override { setVisible (false); }
};

]=])

sc220_p11_range(_ui "branded VST slot editor"
[=[void SC220MainComponent::showChannelInsertEditor]=] [=[void SC220MainComponent::showAudioSettings]=]
[=[void SC220MainComponent::showChannelInsertEditor (int channel, int slot)
{
    if (auto* processor = audioEngine.getChannelFxProcessor (channel, slot))
    {
        auto& window = channelInsertEditors[static_cast<size_t>(channel)][static_cast<size_t>(slot)];
        if (window == nullptr)
        {
            auto* editor = processor->createEditorIfNeeded();
            if (editor == nullptr)
                editor = new juce::GenericAudioProcessorEditor (*processor);
            window = std::make_unique<VoiceFxEditorWindow> (
                "SC220 Live  |  " + juce::String (channel == 0 ? "VOICE" : "PC MUSIC")
                + " FX" + juce::String (slot + 1) + "  |  "
                + audioEngine.getChannelFxName (channel, slot), editor);
        }
        window->setVisible (true);
        window->toFront (true);
    }
}

]=])

sc220_p11_once(_ui "editor lifecycle cleanup"
[=[    voiceFxEditorWindow.reset(); // Editor must die before its VST3 AudioProcessor.]=]
[=[    for (auto& channel : channelInsertEditors)
        for (auto& editor : channel)
            editor.reset(); // Close plugin UI before the plugin is destroyed.
    channelInsertChooser.reset();
    voiceFxEditorWindow.reset();]=])

sc220_p11_once(_ui "native FX button hover/power cosmetics"
[=[        const auto area = button.getLocalBounds().toFloat().reduced (0.7f);]=]
[=[        const auto area = button.getLocalBounds().toFloat().reduced (0.7f);
        const auto& props = button.getProperties();
        const bool fxSlot = static_cast<bool> (props["sc220FxSlot"]);
        const bool fxPower = static_cast<bool> (props["sc220FxPower"]);
        const bool fxMenu = static_cast<bool> (props["sc220FxMenu"]);
        if (fxSlot || fxPower || fxMenu)
        {
            const bool loaded = static_cast<bool> (props["sc220FxLoaded"]);
            const bool active = loaded && ! static_cast<bool> (props["sc220FxBypassed"]);
            auto fill = fxPower ? (active ? palette::cyanDeep : palette::surface)
                                : (fxSlot && loaded ? palette::surfaceAlt : palette::screen);
            if (highlighted && button.isEnabled())
                fill = fill.brighter (0.16f);
            if (down)
                fill = fill.darker (0.17f);
            g.setColour (fill);
            g.fillRoundedRectangle (area, 4.0f);
            g.setColour (fxPower && active ? palette::cyan.withAlpha (0.8f)
                        : fxSlot && active ? palette::cyan.withAlpha (0.45f)
                        : palette::borderSoft);
            g.drawRoundedRectangle (area, 4.0f, 1.0f);
            if (fxSlot)
            {
                g.setColour (active ? palette::cyan : loaded ? palette::amber : palette::muted);
                g.fillRoundedRectangle (area.getX() + 1.0f, area.getY() + 4.0f,
                                        2.0f, area.getHeight() - 8.0f, 1.0f);
            }
            return;
        }]=])

sc220_p11_once(_ui "native FX slot names ON-OFF rendering"
[=[    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
]=]
[=[    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        const auto& props = button.getProperties();
        const bool fxSlot = static_cast<bool> (props["sc220FxSlot"]);
        const bool fxPower = static_cast<bool> (props["sc220FxPower"]);
        const bool fxMenu = static_cast<bool> (props["sc220FxMenu"]);
        if (fxSlot || fxPower || fxMenu)
        {
            const bool active = static_cast<bool> (props["sc220FxLoaded"])
                                && ! static_cast<bool> (props["sc220FxBypassed"]);
            g.setColour (! button.isEnabled() ? palette::muted.withAlpha (0.52f)
                         : fxPower && active ? juce::Colours::white
                         : fxPower ? palette::muted
                         : fxSlot && active ? palette::ink : palette::graphite);
            g.setFont (fxSlot ? interMediumFont (11.0f) : interFont (10.0f, fxPower));
            g.drawFittedText (button.getButtonText(),
                              button.getLocalBounds().reduced (fxSlot ? 8 : 2, 2),
                              fxSlot ? juce::Justification::centredLeft
                                     : juce::Justification::centred, 1);
            return;
        }
]=])

sc220_p11_once(_ui "mixer workspace ratios"
[=[    const int inputWidth = juce::roundToInt (area.getWidth() * 0.310f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.420f);]=]
[=[    const int inputWidth = juce::roundToInt (area.getWidth() * 0.440f);
    const int signatureWidth = juce::roundToInt (area.getWidth() * 0.310f);]=])

sc220_p11_range(_ui "final FX layout with original fader and dual-mono support"
[=[    // FX rows live INSIDE their channel strips]=] [=[    licensePanel->setBounds (getLocalBounds());]=]
[=[    // Space-saving DAW insert bay beside each real channel strip. The stock
    // SVG fader is still drawn by the existing JUCE LookAndFeel.
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto strip = channel == 0 ? sc220StripArea : pcStripArea;
        if (channel == 0)
        {
            sc220Fader.setBounds (strip.getX() + 26, sharedFaderTop,
                                  54, sharedFaderHeight);
            sc220MonoLeftFader.setBounds (strip.getX() + 11, sharedFaderTop,
                                          42, sharedFaderHeight);
            sc220MonoRightFader.setBounds (strip.getX() + 60, sharedFaderTop,
                                           42, sharedFaderHeight);
            sc220Meter->setBounds (strip.getX() + 3, sharedFaderTop + 4,
                                    18, sharedFaderHeight - 8);
            sc220Mute.setBounds (strip.getX() + 19,
                                  inputArea.getBottom() - 50, 68, 28);
        }
        else
        {
            pcFader.setBounds (strip.getX() + 26, sharedFaderTop,
                               54, sharedFaderHeight);
            pcMeter->setBounds (strip.getX() + 3, sharedFaderTop + 4,
                                18, sharedFaderHeight - 8);
        }

        const int rackX = strip.getX() + 107;
        const int rackWidth = juce::jmax (92, strip.getRight() - rackX - 9);
        const int rackY = sharedFaderTop + 4;
        channelRackAreas[static_cast<size_t> (channel)]
            = { rackX, rackY, rackWidth, 136 };
        for (int slot = 0; slot < 3; ++slot)
        {
            const auto c = static_cast<size_t> (channel);
            const auto s = static_cast<size_t> (slot);
            const int rowY = rackY + 30 + slot * 33;
            const int nameWidth = juce::jmax (34, rackWidth - 57);
            channelInsertButtons[c][s].setBounds (rackX + 5, rowY, nameWidth, 28);
            channelInsertPowers[c][s].setBounds (rackX + rackWidth - 48, rowY, 27, 28);
            channelInsertMenus[c][s].setBounds (rackX + rackWidth - 20, rowY, 16, 28);
        }
    }
]=])

sc220_p11_once(_ui "FX rack chassis and always-visible gain value"
[=[}

void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)]=]
[=[    for (int channel = 0; channel < 2; ++channel)
    {
        const auto rack = channelRackAreas[static_cast<size_t> (channel)];
        if (rack.isEmpty()) continue;
        g.setColour (palette::header.withAlpha (0.78f));
        g.fillRoundedRectangle (rack.toFloat(), 5.0f);
        g.setColour (palette::border);
        g.drawRoundedRectangle (rack.toFloat().reduced (0.5f), 4.5f, 1.0f);
        g.setColour (palette::graphite);
        g.setFont (interMediumFont (9.3f));
        g.drawText (channel == 0 ? "VOICE  /  FX INSERTS" : "PC MUSIC  /  FX INSERTS",
                    rack.reduced (7, 0).withHeight (25), juce::Justification::centredLeft);
        g.setColour (palette::borderSoft);
        g.drawHorizontalLine (rack.getY() + 26,
                              static_cast<float> (rack.getX() + 6),
                              static_cast<float> (rack.getRight() - 6));
    }
    g.setColour (palette::cyan);
    g.setFont (interMediumFont (10.6f));
    if (audioEngine.getInputChannelMode() == 2)
        g.drawText (juce::String (sc220Fader.getValue(), 1) + " dB",
                    sc220Fader.getX() - 7, sc220Fader.getY() - 20,
                    sc220Fader.getWidth() + 14, 16, juce::Justification::centred);
    else
    {
        g.drawText (juce::String (sc220MonoLeftFader.getValue(), 1) + " dB",
                    sc220MonoLeftFader.getX() - 3, sc220MonoLeftFader.getY() - 20,
                    sc220MonoLeftFader.getWidth() + 6, 16, juce::Justification::centred);
        g.drawText (juce::String (sc220MonoRightFader.getValue(), 1) + " dB",
                    sc220MonoRightFader.getX() - 3, sc220MonoRightFader.getY() - 20,
                    sc220MonoRightFader.getWidth() + 6, 16, juce::Justification::centred);
    }
    g.drawText (juce::String (pcFader.getValue(), 1) + " dB",
                pcFader.getX() - 7, pcFader.getY() - 20,
                pcFader.getWidth() + 14, 16, juce::Justification::centred);
}

void SC220MainComponent::drawSignatureSurface (juce::Graphics& g)]=])

# Late-stage contract checks. This checks the actual compiled UI, not the raw
# legacy component that is used as source for earlier generators.
foreach(_required IN ITEMS
        "channelInsertPowers" "channelInsertMenus" "channelRackAreas"
        "showChannelInsertEditor (channel, slot)"
        "chooseChannelInsert (channel, slot)"
        "setIcon (loadImage (\"favicon_96x96_png\"))"
        "sc220_fader_thumb_svg")
    string(FIND "${_ui}" "${_required}" _i)
    if(_i EQUAL -1)
        message(FATAL_ERROR "SC220 P11 FX workflow contract missing: ${_required}")
    endif()
endforeach()
file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P11: original SVG faders + 6 independent DAW FX inserts")
