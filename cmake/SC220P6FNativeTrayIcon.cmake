# P6F Windows tray icon fidelity.
#
# P6E made tray registration reliable, but it still rasterised the 96 px web
# PNG into the notification area. Hardware review showed that Windows rendered
# that path as a nearly solid black square. The repository already contains the
# canonical multi-size favicon.ico (48/32/16), so P6F decodes its 32-bit ICO DIB
# frame directly and gives JUCE the exact small-icon artwork.
#
# This pass changes application/tray chrome only. Audio, DSP, routing and the
# P6E entitlement/payment policy are untouched.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6F requires the generated application source")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p6f_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

# Give the generated application access to a dedicated BinaryData namespace so
# favicon.ico is not confused with the existing SC220LiveAssets BinaryData.h.
sc220_p6f_once(_app "ICO BinaryData include"
[=[
#include "BinaryData.h"
]=]
[=[
#include "BinaryData.h"
#include "SC220TrayIconData.h"
]=])

# Replace P6E's PNG raster path with an ICO reader for the canonical Windows
# small-icon asset. RealFaviconGenerator's file stores 48/32/16 32-bit DIB
# entries; choose the exact 32 px frame when available, preserve BGRA alpha,
# and honour the legacy AND mask if an ICO frame happens to contain zero alpha.
sc220_p6f_once(_app "native favicon ICO tray renderer"
[=[
    void installTrayIcon()
    {
        int iconBytes = 0;
        const char* iconData = BinaryData::getNamedResource ("favicon_96x96_png", iconBytes);
        if (iconData == nullptr || iconBytes <= 0)
            iconData = BinaryData::getNamedResource ("favicon_512x512_png", iconBytes);

        juce::Image source;
        if (iconData != nullptr && iconBytes > 0)
            source = juce::ImageCache::getFromMemory (iconData, iconBytes);

        trayImage = juce::Image (juce::Image::ARGB, 32, 32, true);
        juce::Graphics iconGraphics (trayImage);
        if (source.isValid())
        {
            iconGraphics.drawImageWithin (source, 1, 1, 30, 30,
                                          juce::RectanglePlacement::centred, false);
        }
        else
        {
            // Brand-safe emergency fallback. This is deliberately SC220-only;
            // the removed ASK-P/VST icon can never reappear in the tray.
            iconGraphics.setColour (juce::Colour (0xff0d1922));
            iconGraphics.fillRoundedRectangle ({ 1.0f, 1.0f, 30.0f, 30.0f }, 7.0f);
            iconGraphics.setColour (juce::Colour (0xff13bfe8));
            iconGraphics.drawEllipse ({ 6.0f, 6.0f, 20.0f, 20.0f }, 2.0f);
            iconGraphics.setColour (juce::Colours::white);
            iconGraphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            iconGraphics.drawText ("SC", 5, 7, 22, 18, juce::Justification::centred);
        }

        setIconImage (trayImage, {});
        setIconTooltip ("SC220 Live - running in background");
    }
]=]
[=[
    void installTrayIcon()
    {
        const auto* iconData = reinterpret_cast<const unsigned char*> (
            SC220TrayIconData::favicon_ico);
        const int iconBytes = SC220TrayIconData::favicon_icoSize;

        auto readU16 = [] (const unsigned char* p) -> unsigned int
        {
            return static_cast<unsigned int> (p[0])
                 | (static_cast<unsigned int> (p[1]) << 8u);
        };
        auto readU32 = [] (const unsigned char* p) -> unsigned int
        {
            return static_cast<unsigned int> (p[0])
                 | (static_cast<unsigned int> (p[1]) << 8u)
                 | (static_cast<unsigned int> (p[2]) << 16u)
                 | (static_cast<unsigned int> (p[3]) << 24u);
        };

        juce::Image decoded;
        if (iconData != nullptr && iconBytes >= 22
            && readU16 (iconData) == 0u
            && readU16 (iconData + 2) == 1u)
        {
            const int entryCount = juce::jmin (32, static_cast<int> (readU16 (iconData + 4)));
            int bestOffset = -1;
            int bestSize = 0;
            int bestWidth = 0;
            int bestHeight = 0;
            int bestScore = 100000;

            for (int i = 0; i < entryCount; ++i)
            {
                const int directoryOffset = 6 + i * 16;
                if (directoryOffset + 16 > iconBytes)
                    break;

                const auto* entry = iconData + directoryOffset;
                const int width = entry[0] == 0 ? 256 : static_cast<int> (entry[0]);
                const int height = entry[1] == 0 ? 256 : static_cast<int> (entry[1]);
                const int bitDepth = static_cast<int> (readU16 (entry + 6));
                const int bytesInFrame = static_cast<int> (readU32 (entry + 8));
                const int frameOffset = static_cast<int> (readU32 (entry + 12));
                if (width != height || bitDepth != 32
                    || frameOffset < 0 || bytesInFrame <= 40
                    || frameOffset + bytesInFrame > iconBytes)
                    continue;

                const int score = width == 32 ? 0
                                : width > 32 ? width - 32
                                             : 100 + (32 - width);
                if (score < bestScore)
                {
                    bestScore = score;
                    bestOffset = frameOffset;
                    bestSize = bytesInFrame;
                    bestWidth = width;
                    bestHeight = height;
                }
            }

            if (bestOffset >= 0 && bestSize > 40)
            {
                const auto* dib = iconData + bestOffset;
                const int headerSize = static_cast<int> (readU32 (dib));
                const int dibWidth = static_cast<int> (readU32 (dib + 4));
                const int storedHeight = static_cast<int> (readU32 (dib + 8));
                const int bitDepth = static_cast<int> (readU16 (dib + 14));
                const unsigned int compression = readU32 (dib + 16);
                const int xorHeight = storedHeight > 0 ? storedHeight / 2 : (-storedHeight) / 2;
                const bool bottomUp = storedHeight > 0;

                if (headerSize >= 40 && dibWidth == bestWidth
                    && xorHeight == bestHeight && bitDepth == 32 && compression == 0u)
                {
                    const int rowBytes = bestWidth * 4;
                    const int pixelBytes = rowBytes * bestHeight;
                    if (headerSize + pixelBytes <= bestSize)
                    {
                        decoded = juce::Image (juce::Image::ARGB, bestWidth, bestHeight, true);
                        const auto* pixels = dib + headerSize;
                        bool hasAlpha = false;

                        for (int y = 0; y < bestHeight; ++y)
                        {
                            const int sourceY = bottomUp ? bestHeight - 1 - y : y;
                            const auto* row = pixels + sourceY * rowBytes;
                            for (int x = 0; x < bestWidth; ++x)
                            {
                                const auto* px = row + x * 4;
                                const auto alpha = px[3];
                                hasAlpha = hasAlpha || alpha != 0;
                                decoded.setPixelAt (x, y,
                                    juce::Colour::fromRGBA (px[2], px[1], px[0], alpha));
                            }
                        }

                        if (! hasAlpha)
                        {
                            const int maskStride = ((bestWidth + 31) / 32) * 4;
                            const int maskBytes = maskStride * bestHeight;
                            if (headerSize + pixelBytes + maskBytes <= bestSize)
                            {
                                const auto* mask = dib + headerSize + pixelBytes;
                                for (int y = 0; y < bestHeight; ++y)
                                {
                                    const int sourceY = bottomUp ? bestHeight - 1 - y : y;
                                    const auto* maskRow = mask + sourceY * maskStride;
                                    for (int x = 0; x < bestWidth; ++x)
                                    {
                                        const bool transparent =
                                            (maskRow[x / 8] & (0x80u >> (x & 7))) != 0;
                                        auto colour = decoded.getPixelAt (x, y);
                                        decoded.setPixelAt (x, y,
                                            colour.withAlpha (transparent ? 0.0f : 1.0f));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (decoded.isValid())
        {
            // Use the exact Windows small-icon frame instead of rasterising the
            // web PNG. This preserves the ICO's authored transparency and
            // avoids the black-square result observed in the notification area.
            trayImage = decoded;
        }
        else
        {
            // A visible SC220-only emergency mark is preferable to a black or
            // missing icon if a future favicon.ico changes format unexpectedly.
            trayImage = juce::Image (juce::Image::ARGB, 32, 32, true);
            juce::Graphics g (trayImage);
            g.setColour (juce::Colour (0xff0d1922));
            g.fillRoundedRectangle ({ 2.0f, 2.0f, 28.0f, 28.0f }, 6.0f);
            g.setColour (juce::Colour (0xff13bfe8));
            g.drawEllipse ({ 6.0f, 6.0f, 20.0f, 20.0f }, 2.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            g.drawText ("SC", 5, 7, 22, 18, juce::Justification::centred);
        }

        setIconImage (trayImage, trayImage);
        setIconTooltip ("SC220 Live - running in background");
    }
]=])

foreach(_required IN ITEMS
        "#include \"SC220TrayIconData.h\""
        "SC220TrayIconData::favicon_ico"
        "SC220TrayIconData::favicon_icoSize"
        "const int bitDepth = static_cast<int> (readU16 (entry + 6))"
        "juce::Colour::fromRGBA (px[2], px[1], px[0], alpha)"
        "setIconImage (trayImage, trayImage)")
    string(FIND "${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F required ICO authority missing: ${_required}")
    endif()
endforeach()

# Only the old tray-specific declaration is forbidden. P6D intentionally still
# uses the 96 px PNG inside the compact-controller header, which is unrelated to
# the Windows notification-area icon and must not trip this guard.
string(FIND "${_app}"
    "const char* iconData = BinaryData::getNamedResource (\"favicon_96x96_png\", iconBytes);"
    _legacy_tray_png)
if(NOT _legacy_tray_png EQUAL -1)
    message(FATAL_ERROR "SC220 P6F stale PNG tray renderer remains")
endif()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    get_filename_component(_p6f_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    juce_add_binary_data(SC220TrayIconData
        HEADER_NAME SC220TrayIconData.h
        NAMESPACE SC220TrayIconData
        SOURCES "${_p6f_root}/assets/ui/favicon.ico")
    target_link_libraries(SC220Live PRIVATE SC220TrayIconData)
endif()

file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P6F: Windows tray now renders the canonical multi-size favicon.ico")
