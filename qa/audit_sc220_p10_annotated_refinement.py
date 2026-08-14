from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
ICONS_H = (ROOT / "live/LiveIcons.h").read_text(encoding="utf-8")
ICONS_CPP = (ROOT / "live/LiveIcons.cpp").read_text(encoding="utf-8")
P10 = (ROOT / "cmake/SC220P10AnnotatedConsoleRefinement.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
GENERATED_PATH = ROOT / "build-sc220/generated/sc220-monitor/SC220MainComponent_Monitor.cpp"
GENERATED = GENERATED_PATH.read_text(encoding="utf-8") if GENERATED_PATH.exists() else ""
ALPHA_ASSET = ROOT / "assets/ui/sc220_mkii_device_alpha.png"

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


def png_alpha_evidence(path: Path) -> tuple[int, int, int, int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    position = 8
    idat = bytearray()
    width = height = colour_type = bit_depth = -1
    while position < len(data):
        length = struct.unpack(">I", data[position:position + 4])[0]
        chunk_type = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        position += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, colour_type, _, _, interlace = struct.unpack(">IIBBBBB", payload)
            if interlace != 0:
                raise ValueError("interlaced PNG is outside this deterministic audit")
        elif chunk_type == b"IDAT":
            idat.extend(payload)
        elif chunk_type == b"IEND":
            break
    if bit_depth != 8 or colour_type != 6:
        raise ValueError(f"expected 8-bit RGBA PNG, got depth={bit_depth} type={colour_type}")

    raw = zlib.decompress(bytes(idat))
    bpp = 4
    stride = width * bpp
    previous = bytearray(stride)
    offset = 0
    alphas: list[int] = []
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        scanline = bytearray(raw[offset:offset + stride])
        offset += stride
        for index in range(stride):
            left = scanline[index - bpp] if index >= bpp else 0
            up = previous[index]
            up_left = previous[index - bpp] if index >= bpp else 0
            if filter_type == 1:
                scanline[index] = (scanline[index] + left) & 0xFF
            elif filter_type == 2:
                scanline[index] = (scanline[index] + up) & 0xFF
            elif filter_type == 3:
                scanline[index] = (scanline[index] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                predictor = left + up - up_left
                pa, pb, pc = abs(predictor - left), abs(predictor - up), abs(predictor - up_left)
                choice = left if pa <= pb and pa <= pc else up if pb <= pc else up_left
                scanline[index] = (scanline[index] + choice) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unknown PNG filter {filter_type}")
        alphas.extend(scanline[3::4])
        previous = scanline
    return width, height, min(alphas), max(alphas), sum(alpha == 0 for alpha in alphas)


try:
    alpha_width, alpha_height, alpha_min, alpha_max, transparent_pixels = png_alpha_evidence(ALPHA_ASSET)
    alpha_ok = (
        alpha_width == 1098
        and alpha_height == 518
        and alpha_min == 0
        and alpha_max == 255
        and transparent_pixels > (alpha_width * alpha_height) // 5
    )
except (OSError, ValueError, zlib.error, struct.error):
    alpha_ok = False

check(
    "1 transparent SC220 cutout is real RGBA and is the only embedded device asset",
    alpha_ok
    and "assets/ui/sc220_mkii_device_alpha.png" in CMAKE
    and "assets/ui/sc220_mkii_device.png" not in CMAKE
    and "sc220_mkii_device_alpha_png" in GENERATED
    and "g.drawImageWithin (sc220Image" in GENERATED,
)
check(
    "2 Smart Protect visual indicator is removed by latest operator direction",
    "SMART PROTECT" not in GENERATED,
)
check(
    "3 DSP state and power icon are one 128 by 28 clickable component",
    'setEmbeddedLabels ("DSP ON", "DSP BYPASS"' in GENERATED
    and "enhancerCardTop + 48, 128, 28" in GENERATED
    and "const auto dspGroupBounds" not in GENERATED
    and "setEmbeddedLabels" in ICONS_H
    and "hasEmbeddedLabel" in ICONS_CPP,
)
check(
    "4 redundant INPUT MIX caption is absent",
    '"INPUT MIX"' not in GENERATED,
)
check(
    "5 input gain and auto-duck knobs are enlarged consistently",
    GENERATED.count("sourceSelectorY - 84, 78, 78") == 2,
)
check(
    "6 top-bar status and controls share the header optical centre",
    "const int headerControlY = headerArea.getCentreY() - 16" in GENERATED
    and "headerArea.getCentreY() - 20, 108, 40" in GENERATED
    and "headerArea.getCentreY() - 4" in GENERATED,
)
check(
    "7 Program and Music Cue share height with a deliberate gutter",
    "constexpr int monitorModeGap = 6" in GENERATED
    and "monitorOutputBox.getBottom() + 18, 26, 26" in GENERATED
    and "monitorPowerButton.getBottom() + 26, 100, 124" in GENERATED
    and "const int modeY = monitorKnob.getBottom() + 28" in GENERATED
    and "modeY, modeWidth, 28" in GENERATED
    and "modeY, selectorWidth - modeWidth - monitorModeGap, 28" in GENERATED,
)
check(
    "8 four input mode controls share one height and spacing token",
    "constexpr int inputButtonGap = 6" in GENERATED
    and "constexpr int inputButtonHeight = 26" in GENERATED
    and GENERATED.count("inputButtonWidth, inputButtonHeight") == 4,
)
check(
    "9 duplicate fader labels and grey profile rail are absent",
    'g.drawText ("SC220", sc220Fader' not in GENERATED
    and 'g.drawText ("PC AUDIO", pcFader' not in GENERATED
    and "auto progress = juce::Rectangle<float>" not in GENERATED,
)
check(
    "10 SC220 and Stream MUTE controls are raised to one baseline",
    GENERATED.count("getBottom() - 50, 68, 28") == 2,
)
check(
    "11 source and output dropdowns derive from one aligned selector row",
    "const int sourceSelectorY = inputArea.getY() + 214" in GENERATED
    and "pcSourceBox.setBounds" in GENERATED
    and "outputBox.setBounds" in GENERATED
    and "monitorOutputBox.setBounds" in GENERATED,
)
check(
    "12 preset and DSP control live inside the enhancer card header row",
    "presetBox.setBounds (signatureArea.getX() + 28, enhancerCardTop + 48, 206, 28)" in GENERATED
    and "enhancerPowerButton.setBounds (signatureArea.getRight() - 142" in GENERATED,
)
check(
    "13 analyzer is above one unified enhancer and macro control card",
    "signatureArea.getY() + 12" in GENERATED
    and "const int enhancerCardTop = spectrumDisplay->getBottom() + 12" in GENERATED
    and "const auto enhancerControlBay" in GENERATED
    and "const auto macroConsoleBay" in GENERATED,
)
check(
    "14 fader cap uses soft ambient and contact shadows on the dark surface",
    "juce::DropShadow (juce::Colours::black.withAlpha (0.34f * alpha)" in SOURCE
    and "10, { 0, 5 }).drawForPath (g, capShadowPath)" in SOURCE
    and "cap.translated (0.0f, 2.0f)" in SOURCE
    and "cap.expanded (2.0f, 1.0f).translated" not in SOURCE,
)
check(
    "P10 is final presentation authority and integrated label uses embedded Inter",
    PIPELINE.rfind("SC220P10AnnotatedConsoleRefinement.cmake")
    > PIPELINE.rfind("SC220P9DigitalConsoleStyle.cmake")
    and "interFont (9.0f, true)" in GENERATED
    and "inter_regular.ttf" in CMAKE
    and "inter_semibold.ttf" in CMAKE,
)

failed = [name for name, ok in checks if not ok]
print("SC220 P10 annotated premium-console refinement audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
