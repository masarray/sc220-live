from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P5 = (ROOT / "cmake" / "SC220P5ConsolePolish.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P5 authority missing {label}: {needle}")


# P5 must remain the final generated-source authority after P4.
p4 = PIPELINE.find("SC220P4SmartDucking.cmake")
p5 = PIPELINE.find("SC220P5ConsolePolish.cmake")
if p4 < 0 or p5 < 0 or p5 <= p4:
    raise AssertionError("P5 must run after P4 in GenerateSC220BroadcastState.cmake")

# User-facing console contract.
for needle, label in [
    ("Digital Sound Processor ~ Mas Ari Signature", "DSP subtitle"),
    ("DSP ON", "DSP ON label"),
    ("DSP BYPASS", "DSP BYPASS label"),
    ("volume_2_svg", "Lucide speaker"),
    ("MIXER-01\", \"SC220", "friendly SC220 endpoint alias"),
    ("inputArea.getBottom() - 44", "SC220 mute baseline"),
    ("kSc220FaderThumbSvg", "SVG fader runtime"),
]:
    require(P5, needle, label)

# Silence cleanup must be materially faster/deeper than the previous P3 values.
for needle, label in [
    ("expander.setAmount (0.86f)", "stronger P5 expander amount"),
    ("sampleRate * 0.14", "fast expander close"),
    ("0.0f, 42.0f", "deeper expander range"),
    ("0.070f, 0.040f", "short silence hold"),
]:
    require(P5, needle, label)

# The approved supplied fader artwork must exist as SVG, not a raster substitute.
fader = ROOT / "assets" / "ui" / "sc220_fader_thumb.svg"
if not fader.exists():
    raise AssertionError("SC220 SVG fader asset is missing")
fader_text = fader.read_text(encoding="utf-8")
if "<svg" not in fader_text or "viewBox=" not in fader_text:
    raise AssertionError("SC220 fader asset is not a valid scalable SVG surface")

# App branding uses the canonical favicon package. CMake already consumes the
# ui/favicon paths, so those files are intentionally byte-identical mirrors.
for filename, canonical in [
    ("favicon-96x96.png", "favicon-96x96.png"),
    ("favicon-512x512.png", "web-app-manifest-512x512.png"),
]:
    app_icon = ROOT / "assets" / "ui" / filename
    web_icon = ROOT / "assets" / "favicon" / canonical
    if not app_icon.exists() or not web_icon.exists():
        raise AssertionError(f"Missing app/favicon branding asset: {filename}")
    if app_icon.read_bytes() != web_icon.read_bytes():
        raise AssertionError(f"App icon {filename} does not mirror canonical favicon asset {canonical}")

print("SC220 P5 console polish audit passed")
