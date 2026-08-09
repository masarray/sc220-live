# P6F canonical SC220 Windows tray-icon authority.
#
# Hardware review showed that re-scaling the 96 px favicon PNG through JUCE could
# still appear as a black/blank notification-area square on Windows. The user-
# supplied assets/favicon bundle already contains the correct purpose-built
# multi-size Windows ICO (48/32/16, 32-bit). P6F now embeds that exact ICO and
# decodes its authored 32 px frame directly for SystemTrayIconComponent.
#
# This pass changes tray/application chrome only. Audio, DSP, routing, licensing
# and payment behaviour remain untouched.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6F requires the generated application source")
endif()

get_filename_component(_p6f_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_canonical_icon_ico "${_p6f_root}/assets/favicon/favicon.ico")
set(_canonical_icon_96 "${_p6f_root}/assets/favicon/favicon-96x96.png")
set(_canonical_icon_512 "${_p6f_root}/assets/favicon/web-app-manifest-512x512.png")
set(_juce_alias_icon_96 "${_p6f_root}/assets/ui/favicon-96x96.png")
set(_juce_alias_icon_512 "${_p6f_root}/assets/ui/favicon-512x512.png")

foreach(_icon IN ITEMS
        "${_canonical_icon_ico}"
        "${_canonical_icon_96}"
        "${_canonical_icon_512}"
        "${_juce_alias_icon_96}"
        "${_juce_alias_icon_512}")
    if(NOT EXISTS "${_icon}")
        message(FATAL_ERROR "SC220 P6F app icon is missing: ${_icon}")
    endif()
endforeach()

# Keep existing compact-controller / EXE aliases locked to the uploaded bundle.
file(SHA256 "${_canonical_icon_96}" _canonical_96_sha)
file(SHA256 "${_canonical_icon_512}" _canonical_512_sha)
file(SHA256 "${_juce_alias_icon_96}" _alias_96_sha)
file(SHA256 "${_juce_alias_icon_512}" _alias_512_sha)
if(NOT _canonical_96_sha STREQUAL _alias_96_sha)
    message(FATAL_ERROR "SC220 P6F 96px JUCE alias drifted from assets/favicon authority")
endif()
if(NOT _canonical_512_sha STREQUAL _alias_512_sha)
    message(FATAL_ERROR "SC220 P6F 512px JUCE alias drifted from assets/favicon authority")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

function(sc220_p6f_replace_once variable_name label anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${anchor}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F anchor changed: ${label}")
    endif()
    string(REPLACE "${anchor}" "${replacement}" _value "${_value}")
    set(${variable_name} "${_value}" PARENT_SCOPE)
endfunction()

function(sc220_p6f_replace_range variable_name label begin_anchor end_anchor replacement)
    set(_value "${${variable_name}}")
    string(FIND "${_value}" "${begin_anchor}" _begin)
    if(_begin EQUAL -1)
        message(FATAL_ERROR "SC220 P6F range begin changed: ${label}")
    endif()
    string(SUBSTRING "${_value}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "${end_anchor}" _relative_end)
    if(_relative_end EQUAL -1)
        message(FATAL_ERROR "SC220 P6F range end changed: ${label}")
    endif()
    math(EXPR _end "${_begin} + ${_relative_end}")
    string(SUBSTRING "${_value}" 0 ${_begin} _prefix)
    string(SUBSTRING "${_value}" ${_end} -1 _suffix)
    set(${variable_name} "${_prefix}${replacement}${_suffix}" PARENT_SCOPE)
endfunction()

# Dedicated BinaryData namespace avoids colliding with the existing UI assets.
sc220_p6f_replace_once(_app "canonical tray BinaryData include"
[=[
#include "BinaryData.h"
]=]
[=[
#include "BinaryData.h"
#include "SC220CanonicalTrayIconData.h"
]=])

# Replace P6E's PNG -> Graphics -> 32px raster path. The ICO parser deliberately
# accepts only an exact 32x32, 32-bit uncompressed DIB frame from favicon.ico.
# That is the frame Windows should receive for the notification area, so no web-
# asset down-scaling or shell-cache-dependent resampling remains in this path.
set(_tray_impl [=[
    void installTrayIcon()
    {
        const auto* iconData = reinterpret_cast<const unsigned char*> (
            SC220CanonicalTrayIconData::favicon_ico);
        const int iconBytes = SC220CanonicalTrayIconData::favicon_icoSize;

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
        if (iconData != nullptr && iconBytes >= 54
            && readU16 (iconData) == 0u
            && readU16 (iconData + 2) == 1u)
        {
            const int entryCount = juce::jmin (32, static_cast<int> (readU16 (iconData + 4)));
            int frameOffset = -1;
            int frameBytes = 0;

            for (int i = 0; i < entryCount; ++i)
            {
                const int directoryOffset = 6 + i * 16;
                if (directoryOffset + 16 > iconBytes)
                    break;

                const auto* entry = iconData + directoryOffset;
                const int width = entry[0] == 0 ? 256 : static_cast<int> (entry[0]);
                const int height = entry[1] == 0 ? 256 : static_cast<int> (entry[1]);
                const int bitDepth = static_cast<int> (readU16 (entry + 6));
                const int bytes = static_cast<int> (readU32 (entry + 8));
                const int offset = static_cast<int> (readU32 (entry + 12));

                if (width == 32 && height == 32 && bitDepth == 32
                    && bytes > 40 && offset >= 0 && offset + bytes <= iconBytes)
                {
                    frameOffset = offset;
                    frameBytes = bytes;
                    break;
                }
            }

            if (frameOffset >= 0)
            {
                const auto* dib = iconData + frameOffset;
                const int headerSize = static_cast<int> (readU32 (dib));
                const int width = static_cast<int> (readU32 (dib + 4));
                const int storedHeight = static_cast<int> (readU32 (dib + 8));
                const int bitDepth = static_cast<int> (readU16 (dib + 14));
                const unsigned int compression = readU32 (dib + 16);
                const int height = storedHeight > 0 ? storedHeight / 2 : (-storedHeight) / 2;
                const bool bottomUp = storedHeight > 0;

                if (headerSize >= 40 && width == 32 && height == 32
                    && bitDepth == 32 && compression == 0u)
                {
                    const int rowBytes = width * 4;
                    const int pixelBytes = rowBytes * height;
                    const int maskStride = ((width + 31) / 32) * 4;
                    const int maskBytes = maskStride * height;

                    if (headerSize + pixelBytes <= frameBytes)
                    {
                        const auto* pixels = dib + headerSize;
                        const bool maskAvailable = headerSize + pixelBytes + maskBytes <= frameBytes;
                        const auto* mask = maskAvailable ? dib + headerSize + pixelBytes : nullptr;
                        bool hasAlpha = false;

                        for (int y = 0; y < height && ! hasAlpha; ++y)
                        {
                            const auto* row = pixels + y * rowBytes;
                            for (int x = 0; x < width; ++x)
                            {
                                if (row[x * 4 + 3] != 0)
                                {
                                    hasAlpha = true;
                                    break;
                                }
                            }
                        }

                        decoded = juce::Image (juce::Image::ARGB, width, height, true);
                        for (int y = 0; y < height; ++y)
                        {
                            const int sourceY = bottomUp ? height - 1 - y : y;
                            const auto* row = pixels + sourceY * rowBytes;
                            const auto* maskRow = mask != nullptr ? mask + sourceY * maskStride : nullptr;

                            for (int x = 0; x < width; ++x)
                            {
                                const auto* px = row + x * 4;
                                unsigned char alpha = px[3];
                                if (! hasAlpha)
                                {
                                    const bool transparent = maskRow != nullptr
                                        && (maskRow[x / 8] & (0x80u >> (x & 7))) != 0;
                                    alpha = transparent ? 0u : 255u;
                                }

                                decoded.setPixelAt (x, y,
                                    juce::Colour::fromRGBA (px[2], px[1], px[0], alpha));
                            }
                        }
                    }
                }
            }
        }

        if (decoded.isValid())
        {
            trayImage = decoded;
        }
        else
        {
            // Fail visibly rather than silently returning another black square.
            // This fallback is SC220-only and contains no ASK-P/VST artwork.
            trayImage = juce::Image (juce::Image::ARGB, 32, 32, true);
            juce::Graphics g (trayImage);
            g.setColour (juce::Colour (0xff10202b));
            g.fillRoundedRectangle ({ 1.0f, 1.0f, 30.0f, 30.0f }, 7.0f);
            g.setColour (juce::Colour (0xff18c7ef));
            g.drawRoundedRectangle ({ 3.0f, 3.0f, 26.0f, 26.0f }, 6.0f, 2.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            g.drawText ("SC", 5, 7, 22, 18, juce::Justification::centred);
        }

        setIconImage (trayImage, {});
        setIconTooltip ("SC220 Live - running in background");
    }

]=])

sc220_p6f_replace_range(_app "canonical native 32px ICO tray renderer"
    "    void installTrayIcon()\n    {"
    "    LiveAudioEngine& audioEngine;"
    "${_tray_impl}")

foreach(_required IN ITEMS
        "#include \"SC220CanonicalTrayIconData.h\""
        "SC220CanonicalTrayIconData::favicon_ico"
        "width == 32 && height == 32 && bitDepth == 32"
        "juce::Colour::fromRGBA (px[2], px[1], px[0], alpha)"
        "setIconImage (trayImage, {})"
        "SC220 Live - running in background")
    string(FIND "${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F native tray authority missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "askp_app_logo_png"
        "const char* iconData = BinaryData::getNamedResource (\"favicon_96x96_png\"")
    string(FIND "${_app}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F stale tray renderer remains: ${_forbidden}")
    endif()
endforeach()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    juce_add_binary_data(SC220CanonicalTrayIconData
        HEADER_NAME SC220CanonicalTrayIconData.h
        NAMESPACE SC220CanonicalTrayIconData
        SOURCES "${_canonical_icon_ico}")
    target_link_libraries(SC220Live PRIVATE SC220CanonicalTrayIconData)
endif()

file(WRITE "${SC220_APP_GENERATED_SOURCE}" "${_app}")
message(STATUS "SC220 P6F: Windows tray uses assets/favicon/favicon.ico exact 32px frame")
