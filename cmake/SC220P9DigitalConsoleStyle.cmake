# P9 presentation authority. Unify the complete application around one premium
# digital-console language: graphite chassis, inset channel bays, cool cyan
# readouts, restrained status colour, and consistent top-left lighting.

if(NOT DEFINED SC220_UI_MONITOR_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P9 digital-console style requires the generated UI source")
endif()

file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)

function(sc220_p9_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P9 style anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

sc220_p9_once(_ui "dark post-master analyzer screen"
[=[        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::ink);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (juce::Colour (0xff3d4040));]=]
[=[        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (palette::screen);
        g.fillRoundedRectangle (area, 7.0f);
        g.setColour (palette::cyanDeep.withAlpha (0.72f));]=])

sc220_p9_once(_ui "graphite input chassis"
[=[    g.setColour (palette::surface);
    g.fillRect (inputArea);
    g.setColour (palette::border);]=]
[=[    g.setGradientFill (juce::ColourGradient (
        palette::surfaceAlt, static_cast<float> (inputArea.getX()),
        static_cast<float> (inputArea.getY()), palette::surface,
        static_cast<float> (inputArea.getRight()),
        static_cast<float> (inputArea.getBottom()), false));
    g.fillRect (inputArea);

    const auto sc220ChannelBay = juce::Rectangle<float> (
        static_cast<float> (sc220StripArea.getX() + 4),
        static_cast<float> (sc220StripArea.getY() - 6),
        static_cast<float> (sc220StripArea.getWidth() - 8),
        static_cast<float> (inputArea.getBottom() - sc220StripArea.getY() - 2));
    const auto pcChannelBay = juce::Rectangle<float> (
        static_cast<float> (pcStripArea.getX() + 4),
        static_cast<float> (pcStripArea.getY() - 6),
        static_cast<float> (pcStripArea.getWidth() - 8),
        static_cast<float> (inputArea.getBottom() - pcStripArea.getY() - 2));
    for (const auto bay : { sc220ChannelBay, pcChannelBay })
    {
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (bay.translated (0.0f, 2.0f), 5.0f);
        g.setGradientFill (juce::ColourGradient (
            palette::surfaceAlt.brighter (0.025f), bay.getX(), bay.getY(),
            palette::surface.darker (0.10f), bay.getRight(), bay.getBottom(), false));
        g.fillRoundedRectangle (bay, 5.0f);
        g.setColour (palette::border.withAlpha (0.76f));
        g.drawRoundedRectangle (bay.reduced (0.5f), 4.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawHorizontalLine (juce::roundToInt (bay.getY() + 1.0f),
                              bay.getX() + 5.0f, bay.getRight() - 5.0f);
    }
    g.setColour (palette::border);]=])

sc220_p9_once(_ui "graphite enhancer chassis"
[=[    g.setColour (palette::ivory);
    g.fillRect (signatureArea);
    g.setColour (palette::ink);]=]
[=[    g.setGradientFill (juce::ColourGradient (
        palette::surfaceAlt.brighter (0.035f),
        static_cast<float> (signatureArea.getX()),
        static_cast<float> (signatureArea.getY()),
        palette::ivory,
        static_cast<float> (signatureArea.getRight()),
        static_cast<float> (signatureArea.getBottom()), false));
    g.fillRect (signatureArea);

    const auto enhancerHeaderBay = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getX() + 10),
        static_cast<float> (signatureArea.getY() + 8),
        static_cast<float> (signatureArea.getWidth() - 20), 54.0f);
    g.setColour (palette::header.withAlpha (0.72f));
    g.fillRoundedRectangle (enhancerHeaderBay, 5.0f);
    g.setColour (palette::border.withAlpha (0.88f));
    g.drawRoundedRectangle (enhancerHeaderBay.reduced (0.5f), 4.5f, 1.0f);

    const auto macroConsoleBay = juce::Rectangle<float> (
        static_cast<float> (signatureArea.getX() + 14),
        static_cast<float> (spectrumDisplay->getBottom() + 16),
        static_cast<float> (signatureArea.getWidth() - 28),
        static_cast<float> (signatureArea.getBottom() - spectrumDisplay->getBottom() - 76));
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    g.fillRoundedRectangle (macroConsoleBay.translated (0.0f, 2.0f), 6.0f);
    g.setColour (palette::surface.withAlpha (0.78f));
    g.fillRoundedRectangle (macroConsoleBay, 6.0f);
    g.setColour (palette::borderSoft);
    g.drawRoundedRectangle (macroConsoleBay.reduced (0.5f), 5.5f, 1.0f);
    g.setColour (palette::ink);]=])

sc220_p9_once(_ui "graphite output chassis"
[=[    g.setColour (palette::surface);
    g.fillRect (masterArea);
    g.setColour (palette::border);]=]
[=[    g.setGradientFill (juce::ColourGradient (
        palette::surfaceAlt, static_cast<float> (masterArea.getX()),
        static_cast<float> (masterArea.getY()), palette::surface,
        static_cast<float> (masterArea.getRight()),
        static_cast<float> (masterArea.getBottom()), false));
    g.fillRect (masterArea);

    const int outputBayTop = outputBox.getY() - 34;
    const int outputBayBottom = masterArea.getBottom() - 8;
    const int outputBayDivider = (outputBox.getRight() + monitorOutputBox.getX()) / 2;
    const auto streamOutputBay = juce::Rectangle<float> (
        static_cast<float> (masterArea.getX() + 8),
        static_cast<float> (outputBayTop),
        static_cast<float> (outputBayDivider - masterArea.getX() - 12),
        static_cast<float> (outputBayBottom - outputBayTop));
    const auto monitorOutputBay = juce::Rectangle<float> (
        static_cast<float> (outputBayDivider + 4),
        static_cast<float> (outputBayTop),
        static_cast<float> (masterArea.getRight() - outputBayDivider - 12),
        static_cast<float> (outputBayBottom - outputBayTop));
    for (const auto bay : { streamOutputBay, monitorOutputBay })
    {
        g.setColour (juce::Colours::black.withAlpha (0.24f));
        g.fillRoundedRectangle (bay.translated (0.0f, 2.0f), 5.0f);
        g.setColour (palette::surface.darker (0.08f));
        g.fillRoundedRectangle (bay, 5.0f);
        g.setColour (palette::border.withAlpha (0.72f));
        g.drawRoundedRectangle (bay.reduced (0.5f), 4.5f, 1.0f);
    }
    g.setColour (palette::border);]=])

sc220_p9_once(_ui "dark digital footer"
[=[    g.setColour (palette::surface);
    g.fillRect (footerArea);]=]
[=[    g.setGradientFill (juce::ColourGradient (
        palette::header, 0.0f, static_cast<float> (footerArea.getY()),
        palette::surface, static_cast<float> (footerArea.getRight()),
        static_cast<float> (footerArea.getY()), false));
    g.fillRect (footerArea);]=])

foreach(_required IN ITEMS
        "const auto cyan = juce::Colour (0xff35c4d8)"
        "const auto screen = juce::Colour (0xff080d11)"
        "palette::cyanDeep.withAlpha (0.72f)"
        "const auto sc220ChannelBay"
        "const auto pcChannelBay"
        "const auto enhancerHeaderBay"
        "const auto macroConsoleBay"
        "const auto streamOutputBay"
        "const auto monitorOutputBay"
        "palette::header, 0.0f")
    string(FIND "${_ui}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P9 digital-console invariant missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "const auto ivory = juce::Colour (0xfff4f1ea)"
        "const auto surface = juce::Colour (0xfffbfaf7)"
        "const auto ink = juce::Colour (0xff121416)")
    string(FIND "${_ui}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P9 legacy light-console palette remains: ${_forbidden}")
    endif()
endforeach()

file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")
message(STATUS "SC220 P9: unified graphite/cyan premium digital console")
