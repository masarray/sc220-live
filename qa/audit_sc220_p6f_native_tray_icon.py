from hashlib import sha1
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6F = (ROOT / "cmake" / "SC220P6FNativeTrayIcon.cmake").read_text(encoding="utf-8")
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

for path in (CANONICAL_96, CANONICAL_512, ALIAS_96, ALIAS_512):
    if not path.exists():
        raise AssertionError(f"SC220 app icon file is missing: {path}")

if git_blob_sha(CANONICAL_96) != git_blob_sha(ALIAS_96):
    raise AssertionError("96px build alias is not the assets/favicon SC220 icon")
if git_blob_sha(CANONICAL_512) != git_blob_sha(ALIAS_512):
    raise AssertionError("512px build alias is not the assets/favicon SC220 icon")

# Lock the exact user-uploaded favicon bundle currently reviewed on hardware.
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

for needle, label in [
    ('assets/favicon/favicon-96x96.png', "canonical 96px folder"),
    ('assets/favicon/web-app-manifest-512x512.png', "canonical 512px folder"),
    ('_canonical_96_sha STREQUAL _alias_96_sha', "96px alias parity guard"),
    ('_canonical_512_sha STREQUAL _alias_512_sha', "512px alias parity guard"),
    ('BinaryData::getNamedResource (\\"favicon_96x96_png\\"', "runtime 96px resource"),
    ('BinaryData::getNamedResource (\\"favicon_512x512_png\\"', "runtime 512px fallback"),
    ('setIconImage (trayImage, trayImage)', "tray publication"),
]:
    require(P6F, needle, label)

for forbidden in [
    "sc220-download",
    "askp_app_logo_png",
    "SC220TrayIconData",
    "favicon_ico",
    "MusicalNoiseClean",
    "noiseClean.",
    "expander.",
]:
    if forbidden in P6F:
        raise AssertionError(f"P6F must remain canonical favicon branding only: {forbidden}")

print("SC220 P6F assets/favicon authority audit passed")
