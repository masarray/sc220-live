from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6A = (ROOT / "cmake" / "SC220P6AUiPolish.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6A authority missing {label}: {needle}")


# P6A must be the final UI authority after P5 and must remain UI-only.
p5 = PIPELINE.find("SC220P5ConsolePolish.cmake")
p6a = PIPELINE.find("SC220P6AUiPolish.cmake")
if p5 < 0 or p6a < 0 or p6a <= p5:
    raise AssertionError("P6A must run after P5 in GenerateSC220BroadcastState.cmake")

for forbidden in ["MusicalNoiseClean", "expander.set", "setThresholdOffsetDb"]:
    if forbidden in P6A:
        raise AssertionError(f"P6A must remain UI-only; audio mutation found: {forbidden}")

for needle, label in [
    ("centreX - 16.0f", "optically centred fader thumb"),
    ("kSc220PcAudioHeadphonesSvg", "supplied PC Audio SVG runtime"),
    ("pcStripArea.getY() + 37", "lower PC Audio heading"),
    ("pcStripArea.getY() + 88", "lower PC Audio status LED"),
    ("pcStripArea.getY() + 81", "lower PC Audio status text"),
    ("muteActive", "active MUTE state"),
    ("mutePulse", "MUTE pulse animation"),
    ("sc220Mute.repaint()", "SC220 MUTE animation repaint"),
    ("streamMute.repaint()", "Stream MUTE animation repaint"),
]:
    require(P6A, needle, label)

icon = ROOT / "assets" / "ui" / "pc_audio_headphones.svg"
if not icon.exists():
    raise AssertionError("P6A PC Audio headphones SVG asset is missing")
icon_text = icon.read_text(encoding="utf-8")
if "<svg" not in icon_text or "viewBox=" not in icon_text:
    raise AssertionError("P6A PC Audio icon is not valid scalable SVG artwork")

print("SC220 P6A UI polish audit passed")
