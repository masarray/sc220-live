from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INTEGRATION = ROOT / "cmake" / "IntegrateSC220StaticNoiseClean.cmake"
CHAIN = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"
NOISE_H = ROOT / "live" / "MusicalNoiseClean.h"
NOISE_CPP = ROOT / "live" / "MusicalNoiseClean.cpp"

integration = INTEGRATION.read_text(encoding="utf-8")
chain = CHAIN.read_text(encoding="utf-8")
noise_h = NOISE_H.read_text(encoding="utf-8")
noise_cpp = NOISE_CPP.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("Sonkupik MusicalNoiseClean source is present", "class MusicalNoiseClean" in noise_h)
check("stereo-linked expander source is present", "class SmoothDownwardExpander" in noise_h)
check("profile learning exists", "startLearning" in noise_h and "profileValid.store (true)" in noise_cpp)
check("spectral denoise is profile gated", "if (! profileValid.load())" in noise_cpp)
check("static-noise integration pass exists", INTEGRATION.exists())
check(
    "integration runs after truthful Stream Output pass",
    'include("${CMAKE_CURRENT_LIST_DIR}/UseSC220RealStreamEndpoints.cmake")' in chain
    and 'include("${CMAKE_CURRENT_LIST_DIR}/IntegrateSC220StaticNoiseClean.cmake")' in chain
    and chain.index("UseSC220RealStreamEndpoints.cmake")
    < chain.index("IntegrateSC220StaticNoiseClean.cmake"),
)
check(
    "SC220 voice path applies enhancer before expander",
    "voiceEnhancerBridge.process (voiceBlock, voiceMidi);" in integration
    and "expander.process (voiceBlock, detectorBlock);" in integration
    and integration.index("voiceEnhancerBridge.process (voiceBlock, voiceMidi);")
    < integration.index("expander.process (voiceBlock, detectorBlock);"),
)
check(
    "expander threshold follows learned noise floor",
    "noiseClean.hasProfile()" in integration
    and "noiseClean.getEstimatedNoiseFloorDb()" in integration,
)
check(
    "gear menu exposes enable and two-second learn workflow",
    all(token in integration for token in (
        '"Noise Clean"',
        '"Learn Static Noise (2 sec)"',
        "noise.startLearning (2.0)",
        "noise.setEnabled (enable)",
        "expander.setEnabled (enable)",
    )),
)
check(
    "learned profile status is user-visible",
    all(token in integration for token in (
        "NOISE PROFILE: LEARNING",
        "NOISE PROFILE: READY",
        "NOISE PROFILE: NOT LEARNED",
    )),
)
check(
    "learning instruction explicitly requires silent SC220 input",
    "Keep the SC220 input silent for 2 seconds" in integration,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live Sonkupik static-noise clean audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
