from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6F = (ROOT / "cmake" / "SC220P6FNativeTrayIcon.cmake").read_text(encoding="utf-8")
ICO = ROOT / "assets" / "ui" / "favicon.ico"


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6F authority missing {label}: {needle}")


if 'include("${CMAKE_CURRENT_LIST_DIR}/SC220P6FNativeTrayIcon.cmake")' not in P6D:
    raise AssertionError("P6F must be chained after P6E/P6D")

if not ICO.exists() or ICO.stat().st_size < 1024:
    raise AssertionError("canonical assets/ui/favicon.ico is missing or unexpectedly small")

raw = ICO.read_bytes()
if raw[:4] != b"\x00\x00\x01\x00":
    raise AssertionError("favicon.ico is not a Windows ICO resource")
if len(raw) < 6:
    raise AssertionError("favicon.ico header is incomplete")
count = int.from_bytes(raw[4:6], "little")
if count < 3:
    raise AssertionError("favicon.ico should keep the multi-size Windows icon set")

for needle, label in [
    ('HEADER_NAME SC220TrayIconData.h', "dedicated BinaryData header"),
    ('NAMESPACE SC220TrayIconData', "dedicated BinaryData namespace"),
    ('assets/ui/favicon.ico', "canonical ICO source"),
    ('SC220TrayIconData::favicon_ico', "embedded ICO bytes"),
    ('SC220TrayIconData::favicon_icoSize', "embedded ICO size"),
    ('bitDepth != 32', "32-bit ICO frame validation"),
    ('const int score = width >= 32 ? width - 32', "32 px frame preference"),
    ('juce::Colour::fromRGBA (px[2], px[1], px[0], alpha)', "BGRA-to-JUCE alpha decode"),
    ('setIconImage (trayImage, trayImage)', "exact decoded tray image publication"),
]:
    require(P6F, needle, label)

for forbidden in [
    'askp_app_logo_png',
    'MusicalNoiseClean',
    'noiseClean.',
    'expander.',
    'selectPcAudioEndpoint',
    'selectMonitorOutputDevice',
]:
    if forbidden in P6F:
        raise AssertionError(f"P6F must remain tray-artwork only: {forbidden}")

print("SC220 P6F native ICO tray audit passed")
