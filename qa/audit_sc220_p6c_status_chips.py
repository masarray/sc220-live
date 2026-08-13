from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6C = (ROOT / "cmake" / "SC220P6CStatusChips.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6C authority missing {label}: {needle}")


# P6C must be the final UI authority after P6B.
p6b = PIPELINE.find("SC220P6BNoiseCleanup.cmake")
p6c = PIPELINE.find("SC220P6CStatusChips.cmake")
if p6b < 0 or p6c < 0 or p6c <= p6b:
    raise AssertionError("P6C must run after P6B in GenerateSC220BroadcastState.cmake")

# P6C is UI-only and must not alter runtime DSP, routing, denoise or ducking.
for forbidden in [
    "SC220_ENGINE_GENERATED_SOURCE",
    "MusicalNoiseClean",
    "expander.",
    "noiseClean.",
    "duck",
    "selectPcAudioEndpoint",
    "selectMonitorOutputDevice",
]:
    if forbidden in P6C:
        raise AssertionError(f"P6C must remain UI-only; runtime mutation found: {forbidden}")

for needle, label in [
    ("const auto dspChipColour = active ? palette::green : palette::amber", "state-driven DSP colour"),
    ("const auto drawIlluminatedPad", "shared hardware pad lens renderer"),
    ("dspPadHeight = static_cast<float> (enhancerPowerButton.getHeight())", "DSP pad matches power height"),
    ("active ? \"DSP ON\" : \"DSP BYPASS\"", "explicit DSP state copy"),
    ("drawIlluminatedPad (dspChip", "DSP illuminated pad face"),
    ("const auto protectChip = juce::Rectangle<float>", "Smart Protect chip geometry"),
    ("protectPadHeight = static_cast<float> (enhancerPowerButton.getHeight())", "Smart Protect pad matches power height"),
    ("SMART PROTECT   ON", "Smart Protect status copy"),
    ("drawIlluminatedPad (protectChip", "Smart Protect illuminated pad face"),
    ("lampColour.brighter (0.38f)", "lit pad lens highlight"),
]:
    require(P6C, needle, label)

for stale in [
    "SMART PROTECT  /  ON",
    "g.fillEllipse (static_cast<float> (enhancerPowerButton.getX() - 96)",
]:
    if stale not in P6C:
        raise AssertionError(f"P6C must explicitly replace the legacy status anchor: {stale}")

print("SC220 P6C status-chip audit passed")
