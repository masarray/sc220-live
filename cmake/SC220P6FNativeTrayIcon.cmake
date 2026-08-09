# P6F Windows tray/app-icon authority.
#
# Hardware retest showed the tray was no longer black, but it still displayed
# the old ArSonKuPik artwork. The renderer was not the remaining problem: the
# local assets/ui favicon PNG/ICO set itself was stale. The current SC220 brand
# authority lives in masarray/sc220-download and uses the dark SC220 tile with
# the orange audio-wave mark.
#
# P6F therefore keeps P6E's proven Windows tray registration/retry path and
# locks it to the corrected SC220 PNG mirrors now synced into assets/ui.
# CMake's ICON_BIG / ICON_SMALL use the same corrected files, so tray, EXE and
# taskbar branding share one SC220 asset authority.
#
# This pass is application chrome only. Audio, DSP, routing, entitlement and
# payment behaviour are untouched.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6F requires the generated application source")
endif()

get_filename_component(_p6f_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_sc220_icon_96 "${_p6f_root}/assets/ui/favicon-96x96.png")
set(_sc220_icon_512 "${_p6f_root}/assets/ui/favicon-512x512.png")

foreach(_icon IN ITEMS "${_sc220_icon_96}" "${_sc220_icon_512}")
    if(NOT EXISTS "${_icon}")
        message(FATAL_ERROR "SC220 P6F canonical app icon is missing: ${_icon}")
    endif()
endforeach()

# Size guards intentionally match the reviewed SC220 icon mirrors synced from
# masarray/sc220-download. The stronger content-addressed guard lives in
# qa/audit_sc220_p6f_native_tray_icon.py.
file(SIZE "${_sc220_icon_96}" _icon96_size)
file(SIZE "${_sc220_icon_512}" _icon512_size)
if(NOT _icon96_size EQUAL 13165)
    message(FATAL_ERROR "SC220 P6F 96px icon is not the reviewed SC220 asset")
endif()
if(NOT _icon512_size EQUAL 182135)
    message(FATAL_ERROR "SC220 P6F 512px icon is not the reviewed SC220 asset")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

# P6E is intentionally retained as the final runtime implementation: it keeps a
# persistent 32x32 ARGB tray image and retries registration after Explorer is
# ready. With the corrected assets, that path now renders the SC220 app mark.
foreach(_required IN ITEMS
        "BinaryData::getNamedResource (\"favicon_96x96_png\""
        "BinaryData::getNamedResource (\"favicon_512x512_png\""
        "trayImage = juce::Image (juce::Image::ARGB, 32, 32, true)"
        "setIconImage (trayImage, trayImage)"
        "SC220 Live - running in background")
    string(FIND "${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F required tray path missing: ${_required}")
    endif()
endforeach()

# The stale ICO is deliberately not a runtime authority anymore. It was the
# exact reason P6F initially reintroduced the ArSonKuPik mark.
foreach(_forbidden IN ITEMS
        "SC220TrayIconData"
        "favicon_ico"
        "askp_app_logo_png")
    string(FIND "${_app}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F stale tray authority remains: ${_forbidden}")
    endif()
endforeach()

message(STATUS "SC220 P6F: tray/EXE icon authority = canonical SC220 96/512 PNG assets")
