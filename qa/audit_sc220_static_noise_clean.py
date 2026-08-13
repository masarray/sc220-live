from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = (ROOT / "cmake" / "IntegrateSC220StaticNoiseClean.cmake").read_text(encoding="utf-8")
FINAL = (ROOT / "cmake" / "FinalizeSC220NaturalLivePath.cmake").read_text(encoding="utf-8")
CHAIN = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
NOISE_H = (ROOT / "live" / "MusicalNoiseClean.h").read_text(encoding="utf-8")
NOISE_CPP = (ROOT / "live" / "MusicalNoiseClean.cpp").read_text(encoding="utf-8")
GENERATED_PATH = ROOT / "build-sc220/generated/sc220-monitor/LiveAudioEngine_Monitor.cpp"
GENERATED = GENERATED_PATH.read_text(encoding="utf-8") if GENERATED_PATH.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("learned static-noise profiler exists", "class MusicalNoiseClean" in NOISE_H)
check("stereo-linked expander exists", "class SmoothDownwardExpander" in NOISE_H)
check("explicit two-second learning still exists", "startLearning" in NOISE_H and "profileValid.store (true)" in NOISE_CPP)
check(
    "normal programme path reports zero profiler latency",
    "int getLatencySamples() const noexcept { return 0; }" in NOISE_H,
)
check(
    "engine calls FFT workspace only while Learn is requested or active",
    "noiseClean.shouldCaptureProfile()" in FINAL
    and "if (noiseClean.shouldCaptureProfile())" in FINAL,
)
check(
    "frozen learned floor is mandatory before expander processing",
    "noiseClean.hasProfile() && noiseClean.isEnabled() && expander.isEnabled()" in GENERATED
    and "noiseClean.getEstimatedNoiseFloorDb()" in GENERATED
    and ": -72.0f" not in GENERATED,
)
check(
    "Noise Clean OFF is true runtime bypass",
    "noiseClean.isEnabled() && expander.isEnabled()" in FINAL,
)
check(
    "user sees frozen-profile semantics",
    "NOISE PROFILE: FROZEN / READY" in FINAL
    and "only learning phase" in FINAL
    and "profile is frozen afterwards" in FINAL,
)
check(
    "voice enhancer remains before linked expander",
    "voiceEnhancerBridge.process (voiceBlock, voiceMidi);" in GENERATED
    and "expander.process (voiceBlock, detectorBlock);" in GENERATED
    and GENERATED.index("voiceEnhancerBridge.process (voiceBlock, voiceMidi);")
        < GENERATED.index("expander.process (voiceBlock, detectorBlock);"),
)
check(
    "final natural policy runs after original Sonkupik integration",
    "IntegrateSC220StaticNoiseClean.cmake" in CHAIN
    and "FinalizeSC220NaturalLivePath.cmake" in CHAIN
    and CHAIN.index("IntegrateSC220StaticNoiseClean.cmake")
        < CHAIN.index("FinalizeSC220NaturalLivePath.cmake"),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live frozen-profile natural cleanup audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
