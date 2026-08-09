# P6F canonical SC220 app/tray icon authority.
#
# The user-provided branding bundle lives at assets/favicon. That folder is the
# only branding authority. The existing JUCE target still names two legacy build
# aliases under assets/ui, so those aliases are required to be byte-identical to
# the corresponding files in assets/favicon. This keeps the runtime resource
# names stable while guaranteeing that tray, EXE, taskbar and compact-controller
# artwork comes from the uploaded SC220 favicon bundle rather than ASK-P assets.
#
# This pass is application chrome only. Audio, DSP, routing, entitlement and
# payment behaviour are untouched.

if(NOT DEFINED SC220_APP_GENERATED_SOURCE)
    message(FATAL_ERROR "SC220 P6F requires the generated application source")
endif()

get_filename_component(_p6f_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_canonical_icon_96
    "${_p6f_root}/assets/favicon/favicon-96x96.png")
set(_canonical_icon_512
    "${_p6f_root}/assets/favicon/web-app-manifest-512x512.png")
set(_juce_alias_icon_96
    "${_p6f_root}/assets/ui/favicon-96x96.png")
set(_juce_alias_icon_512
    "${_p6f_root}/assets/ui/favicon-512x512.png")

foreach(_icon IN ITEMS
        "${_canonical_icon_96}"
        "${_canonical_icon_512}"
        "${_juce_alias_icon_96}"
        "${_juce_alias_icon_512}")
    if(NOT EXISTS "${_icon}")
        message(FATAL_ERROR "SC220 P6F app icon is missing: ${_icon}")
    endif()
endforeach()

file(SHA256 "${_canonical_icon_96}" _canonical_96_sha)
file(SHA256 "${_canonical_icon_512}" _canonical_512_sha)
file(SHA256 "${_juce_alias_icon_96}" _alias_96_sha)
file(SHA256 "${_juce_alias_icon_512}" _alias_512_sha)

if(NOT _canonical_96_sha STREQUAL _alias_96_sha)
    message(FATAL_ERROR
        "SC220 P6F 96px JUCE alias drifted from assets/favicon authority")
endif()
if(NOT _canonical_512_sha STREQUAL _alias_512_sha)
    message(FATAL_ERROR
        "SC220 P6F 512px JUCE alias drifted from assets/favicon authority")
endif()

file(READ "${SC220_APP_GENERATED_SOURCE}" _app)

# P6E owns the proven long-lived Windows notification-area object and Explorer
# registration retries. P6F only locks its image resources to the canonical
# favicon bundle. Resource names remain stable because the JUCE aliases retain
# their historic filenames while sharing the exact canonical Git blobs.
#
# Deliberately check only the setIconImage(trayImage...) call prefix here. P6E
# currently publishes the Windows tray image as setIconImage(trayImage, {}), and
# the optional second JUCE image argument is not part of the branding contract.
# Hard-coding that argument previously made this configure guard reject the
# already-generated, valid P6E tray path before C++ compilation could begin.
foreach(_required IN ITEMS
        "BinaryData::getNamedResource (\"favicon_96x96_png\""
        "BinaryData::getNamedResource (\"favicon_512x512_png\""
        "trayImage = juce::Image (juce::Image::ARGB, 32, 32, true)"
        "setIconImage (trayImage"
        "SC220 Live - running in background")
    string(FIND "${_app}" "${_required}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F required tray path missing: ${_required}")
    endif()
endforeach()

foreach(_forbidden IN ITEMS
        "SC220TrayIconData"
        "favicon_ico"
        "askp_app_logo_png")
    string(FIND "${_app}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
        message(FATAL_ERROR "SC220 P6F stale tray authority remains: ${_forbidden}")
    endif()
endforeach()

message(STATUS
    "SC220 P6F: canonical app/tray icon authority = assets/favicon")