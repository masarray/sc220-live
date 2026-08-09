from hashlib import sha1
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6F = (ROOT / "cmake" / "SC220P6FNativeTrayIcon.cmake").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
ICON96 = ROOT / "assets" / "ui" / "favicon-96x96.png"
ICON512 = ROOT / "assets" / "ui" / "favicon-512x512.png"

EXPECTED_GIT_BLOBS = {
    ICON96: "d13bbdaf85c3dde039a85599317ced17fb5b8373",
    ICON512: "a23b0ddb8491b2554ef7742640f5391b90088e45",
}


def git_blob_sha(path: Path) -> str:
    data = path.read_bytes()
    header = f"blob {len(data)}\0".encode("ascii")
    return sha1(header + data).hexdigest()


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6F authority missing {label}: {needle}")


if 'include("${CMAKE_CURRENT_LIST_DIR}/SC220P6FNativeTrayIcon.cmake")' not in P6D:
    raise AssertionError("P6F must be chained after P6E/P6D")

for path, expected in EXPECTED_GIT_BLOBS.items():
    if not path.exists():
        raise AssertionError(f"canonical SC220 app icon is missing: {path}")
    actual = git_blob_sha(path)
    if actual != expected:
        raise AssertionError(
            f"{path.name} is not the reviewed SC220 app icon: expected {expected}, got {actual}"
        )

for needle, label in [
    ('ICON_BIG assets/ui/favicon-512x512.png', "Windows large app icon"),
    ('ICON_SMALL assets/ui/favicon-96x96.png', "Windows small app icon"),
]:
    require(CMAKE, needle, label)

for needle, label in [
    ('BinaryData::getNamedResource (\\"favicon_96x96_png\\"', "tray 96px SC220 asset"),
    ('BinaryData::getNamedResource (\\"favicon_512x512_png\\"', "tray 512px fallback"),
    ('trayImage = juce::Image (juce::Image::ARGB, 32, 32, true)', "persistent native tray surface"),
    ('setIconImage (trayImage, {})', "tray icon publication"),
    ('_icon96_size EQUAL 13165', "reviewed 96px size guard"),
    ('_icon512_size EQUAL 182135', "reviewed 512px size guard"),
]:
    require(P6F, needle, label)

# These signatures belonged to the failed P6F ICO-detour implementation. The
# runtime guard may mention their names as forbidden generated-source tokens,
# but no dedicated ICO BinaryData target/decoder may remain in P6F itself.
for forbidden in [
    'HEADER_NAME SC220TrayIconData.h',
    'NAMESPACE SC220TrayIconData',
    'juce_add_binary_data(SC220TrayIconData',
    'SOURCES "${_p6f_root}/assets/ui/favicon.ico"',
    'juce::Colour::fromRGBA (px[2], px[1], px[0], alpha)',
    'const int score = width == 32 ? 0',
    'MusicalNoiseClean',
    'noiseClean.',
    'expander.',
    'selectPcAudioEndpoint',
    'selectMonitorOutputDevice',
]:
    if forbidden in P6F:
        raise AssertionError(f"P6F must remain SC220 app/tray branding only: {forbidden}")

print("SC220 P6F canonical app/tray icon audit passed")
