from hashlib import sha1
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6F = (ROOT / "cmake" / "SC220P6FNativeTrayIcon.cmake").read_text(encoding="utf-8")
CANONICAL_ICO = ROOT / "assets" / "favicon" / "favicon.ico"
CANONICAL_96 = ROOT / "assets" / "favicon" / "favicon-96x96.png"
CANONICAL_512 = ROOT / "assets" / "favicon" / "web-app-manifest-512x512.png"
ALIAS_96 = ROOT / "assets" / "ui" / "favicon-96x96.png"
ALIAS_512 = ROOT / "assets" / "ui" / "favicon-512x512.png"


def git_blob_sha(path: Path) -> str:
    data = path.read_bytes()
    header = f"blob {len(data)}\0".encode("ascii")
    return sha1(header + data).hexdigest()


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6F authority missing {label}: {needle}")


if 'include("${CMAKE_CURRENT_LIST_DIR}/SC220P6FNativeTrayIcon.cmake")' not in P6D:
    raise AssertionError("P6F must be chained after P6E/P6D")

for path in (CANONICAL_ICO, CANONICAL_96, CANONICAL_512, ALIAS_96, ALIAS_512):
    if not path.exists():
        raise AssertionError(f"SC220 app icon file is missing: {path}")

if git_blob_sha(CANONICAL_96) != git_blob_sha(ALIAS_96):
    raise AssertionError("96px build alias is not the assets/favicon SC220 icon")
if git_blob_sha(CANONICAL_512) != git_blob_sha(ALIAS_512):
    raise AssertionError("512px build alias is not the assets/favicon SC220 icon")

# Lock the reviewed uploaded PNG mirrors used elsewhere in the app.
expected = {
    CANONICAL_96: "d13d9c4028440933d1872cd446ee2d3f92abc496",
    CANONICAL_512: "128f13b13f9c53971d79e343b868a99e3df569a9",
}
for path, expected_sha in expected.items():
    actual = git_blob_sha(path)
    if actual != expected_sha:
        raise AssertionError(
            f"{path.name} changed from reviewed assets/favicon authority: "
            f"expected {expected_sha}, got {actual}"
        )

# The tray uses the purpose-built Windows ICO directly. Verify the ICO directory
# contains the authored 48/32/16 32-bit frames and especially an exact 32px frame.
raw = CANONICAL_ICO.read_bytes()
if len(raw) < 54 or raw[:4] != b"\x00\x00\x01\x00":
    raise AssertionError("assets/favicon/favicon.ico is not a valid Windows ICO")
count = int.from_bytes(raw[4:6], "little")
frames = []
for i in range(count):
    entry = 6 + i * 16
    if entry + 16 > len(raw):
        raise AssertionError("favicon.ico directory is truncated")
    width = raw[entry] or 256
    height = raw[entry + 1] or 256
    bit_depth = int.from_bytes(raw[entry + 6:entry + 8], "little")
    frame_bytes = int.from_bytes(raw[entry + 8:entry + 12], "little")
    frame_offset = int.from_bytes(raw[entry + 12:entry + 16], "little")
    if frame_offset + frame_bytes > len(raw):
        raise AssertionError("favicon.ico frame extends past file end")
    frames.append((width, height, bit_depth))

if (32, 32, 32) not in frames:
    raise AssertionError(f"favicon.ico needs an exact 32x32 32-bit tray frame, got {frames}")
if not {(48, 48, 32), (32, 32, 32), (16, 16, 32)}.issubset(set(frames)):
    raise AssertionError(f"favicon.ico lost its reviewed 48/32/16 frame set: {frames}")

for needle, label in [
    ('assets/favicon/favicon.ico', "canonical Windows ICO source"),
    ('HEADER_NAME SC220CanonicalTrayIconData.h', "dedicated ICO BinaryData header"),
    ('NAMESPACE SC220CanonicalTrayIconData', "dedicated ICO namespace"),
    ('SC220CanonicalTrayIconData::favicon_ico', "embedded canonical ICO bytes"),
    ('width == 32 && height == 32 && bitDepth == 32', "exact 32px frame selection"),
    ('bool hasAlpha = false', "ICO alpha audit"),
    ('maskRow[x / 8]', "legacy AND-mask fallback"),
    ('juce::Colour::fromRGBA (px[2], px[1], px[0], alpha)', "BGRA alpha-preserving decode"),
    ('setIconImage (trayImage, {})', "JUCE tray publication"),
]:
    require(P6F, needle, label)

for forbidden in [
    "sc220-download",
    "askp_app_logo_png",
    "MusicalNoiseClean",
    "noiseClean.",
    "expander.",
]:
    if forbidden in P6F:
        raise AssertionError(f"P6F must remain canonical favicon branding only: {forbidden}")

print("SC220 P6F canonical Windows ICO tray audit passed")
