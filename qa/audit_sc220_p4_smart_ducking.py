from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
chain = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
p4 = (ROOT / "cmake/SC220P4SmartDucking.cmake").read_text(encoding="utf-8")
detector = (ROOT / "live/SmartVoiceDuckDetector.h").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "P4 runs after P3 final authority",
    "FinalizeSC220LiveUxP3.cmake" in chain
    and "SC220P4SmartDucking.cmake" in chain
    and chain.index("SC220P4SmartDucking.cmake")
        > chain.index("FinalizeSC220LiveUxP3.cmake"),
)
check(
    "speech detector is allocation-free control sidechain",
    all(token in detector for token in (
        "class SmartVoiceDuckDetector final",
        "120.0 / sampleRate",
        "3800.0 / sampleRate",
        "sampleRate * 0.045",
        "sampleRate * 0.300",
        "candidateSamples = 0",
        "crestDb > 18.0f",
        "speechBandRatioDb >= -10.0f",
    )),
)
check(
    "frozen Learn profile is threshold reference only",
    all(token in detector for token in (
        "hasFrozenNoiseProfile",
        "learnedNoiseFloorDb + 16.0f",
        "std::clamp",
    ))
    and "startLearning" not in detector
    and "FFT" in detector,  # documentation explicitly says no FFT workload
)
check(
    "P4 replaces raw peak trigger with sustained detector",
    "smartVoiceDuckDetector.process" in p4
    and "dbFromPeak (keyPeak) > -36.0f" in p4
    and "forbidden legacy behaviour" in p4,
)
check(
    "natural duck ballistics are guarded",
    "targetDuck < duckGainLinear ? 0.055 : 0.650" in p4
    and "45 ms" in p4
    and "300 ms" in p4,
)
check(
    "LUFS target caption is removed and forbidden",
    "YT / TIKTOK  -14  |  TP <= -1" in p4
    and "LUFS target-caption anchor changed" in p4
    and "forbidden legacy behaviour" in p4,
)

# If the configure-time generated sources are available, verify the final result,
# not only the transformer contract.
for generated_root in (
    ROOT / "build-sc220/generated/sc220-monitor",
    ROOT / "generated/sc220-monitor",
):
    engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
    ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
    if engine_path.exists() and ui_path.exists():
        engine = engine_path.read_text(encoding="utf-8")
        ui = ui_path.read_text(encoding="utf-8")
        check(
            "generated engine uses SmartVoiceDuckDetector",
            "smartVoiceDuckDetector.process" in engine
            and "dbFromPeak (keyPeak) > -36.0f" not in engine,
        )
        check(
            "generated LUFS dial has no fixed social target caption",
            "YT / TIKTOK  -14  |  TP <= -1" not in ui,
        )
        break

failed = [name for name, ok in checks if not ok]
print("SC220 P4 smart ducking / clean LUFS audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
