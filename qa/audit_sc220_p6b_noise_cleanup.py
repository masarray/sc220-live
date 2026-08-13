from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6B = (ROOT / "cmake" / "SC220P6BNoiseCleanup.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
GENERATED_PATH = ROOT / "build-sc220/generated/sc220-monitor/LiveAudioEngine_Monitor.cpp"
GENERATED = GENERATED_PATH.read_text(encoding="utf-8") if GENERATED_PATH.exists() else ""


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6B authority missing {label}: {needle}")


p6a = PIPELINE.find("SC220P6AUiPolish.cmake")
p6b = PIPELINE.find("SC220P6BNoiseCleanup.cmake")
if p6a < 0 or p6b < 0 or p6b <= p6a:
    raise AssertionError("P6B must run after P6A")

# Frozen-profile, zero-latency programme cleanup contract.
for needle, label in [
    ("noiseClean.hasProfile() && noiseClean.isEnabled()", "learned profile gate"),
    ("currentSampleRate.load()", "runtime rate"),
    ("4800.0f", "upper-band split"),
    ("maxHissReductionDb = 6.0f", "bounded hiss reduction"),
    ("hissReferenceDb = frozenFloorDb - 3.0f", "frozen floor reference"),
    ("fastTransient", "transient protection"),
    ("programmeProtect", "programme protection"),
    ("bodyL + highL * p6bHissGain", "sample-synchronous recombination"),
    ("p6bHissGain = 1.0f", "true bypass state reset"),
]:
    require(P6B, needle, label)

# No continuous FFT or adaptive profile learning is allowed in P6B.
for forbidden in [
    "startLearning",
    "profileAccumulator",
    "updateNoiseProfile",
    "FixedFft",
]:
    if forbidden in GENERATED:
        raise AssertionError(f"P6B introduced forbidden adaptive/FFT runtime path: {forbidden}")

if (GENERATED.count("noiseClean.process (voiceBlock);") != 1
        or "if (noiseClean.shouldCaptureProfile())\n        noiseClean.process (voiceBlock);" not in GENERATED):
    raise AssertionError("Final engine must call the FFT profiler only during explicit Learn capture")

# MUTE must remain one component silhouette; no drawing outside button bounds.
for needle, label in [
    ("const auto muteInner = area.reduced (1.4f)", "inner mute warning rim"),
    ("g.drawRoundedRectangle (muteInner", "inner warning border"),
    ("g.fillEllipse (muteInner.getX() + 6.0f", "internal warning LED"),
]:
    require(P6B, needle, label)

if "area.expanded (1.4f)" not in P6B:
    raise AssertionError("P6B must explicitly replace the clipped P6A outer glow anchor")

print("SC220 P6B frozen hiss cleanup and single-shape MUTE audit passed")
