from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
bands = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")
integrator = (ROOT / "live/ProgrammeLoudnessIntegrator.h").read_text(encoding="utf-8")
live_loudness = (ROOT / "live/ProgrammeLiveLoudnessTelemetry.h").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
calibrator = (ROOT / "cmake/CalibrateSC220Meters.cmake").read_text(encoding="utf-8")

generated_root = ROOT / "generated" / "sc220-monitor"
if not generated_root.exists():
    generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"
engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
engine = engine_path.read_text(encoding="utf-8") if engine_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "band filters use proportional logarithmic bandwidth",
    all(token in bands for token in (
        "lowerBoundary", "upperBoundary", "nominal / bandwidth",
        "std::sqrt", "std::clamp (nominal / bandwidth",
    )),
)
check(
    "pink calibration integrates the actual digital response in log frequency",
    all(token in bands for token in (
        "calculatePinkNoiseCalibration", "integrationPoints = 4096",
        "Equal steps in ln(f)", "responsePower", "pinkCalibrationDb",
        "referenceDb - responseDb[band]",
    )),
)
check(
    "band power has sample-rate-independent smoothing",
    all(token in bands for token in (
        "powerSmoothingSeconds = 0.180", "smoothedPower",
        "std::exp", "blockPower", "10.0 * std::log10",
    )),
)
check(
    "momentary loudness is a real 400 ms BS.1770 block",
    all(token in integrator for token in (
        "momentaryLufs()", "momentary = lufsFromEnergy (blockMeanEnergy)",
        "Four adjacent 100 ms segments", "momentaryReady()",
    )),
)
check(
    "live loudness bridge is atomic and lock free",
    all(token in live_loudness for token in (
        "std::atomic<float>", "publishedGeneration", "publish (float value)",
        "memory_order_release", "memory_order_acquire",
    )),
)
check(
    "meter calibration runs after analyzer generation",
    'include("${CMAKE_CURRENT_LIST_DIR}/CalibrateSC220Meters.cmake")' in state
    and state.find("RebuildSC220ImpactVisualizer.cmake")
    < state.find("CalibrateSC220Meters.cmake"),
)
check(
    "final UI has no cosmetic frequency contour",
    "const float displayDb = rawDb[band]" in ui
    and all(token not in ui for token in (
        "displayDb += 4.0f", "displayDb += 2.0f", "displayDb -= 1.5f",
    )),
)
check(
    "final analyzer identifies pink calibration",
    "PINK-FLAT BANDS" in ui,
)
check(
    "engine publishes live momentary loudness after weighted frames",
    "programmeLiveLoudnessTelemetry.publish (loudnessIntegrator.momentaryLufs())" in engine
    and engine.find("programmeLiveLoudnessTelemetry.publish")
    > engine.find("loudnessIntegrator.processFrame"),
)
check(
    "loudness dial reads LUFS-M and keeps LUFS-I numeric",
    all(token in ui for token in (
        "programmeLiveLoudnessTelemetry.read", "nextMomentary",
        "needleLufs", "displayLufs = needleInitialised",
        '"LUFS-I"', '"M " + juce::String',
    )),
)
check(
    "generated final sources exist",
    engine_path.exists() and ui_path.exists(),
)
check(
    "calibration generator owns final engine and UI output",
    all(token in calibrator for token in (
        'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
        'file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")',
        "remove cosmetic analyzer contour", "drive loudness needle from LUFS-M",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live pink-flat analyzer and moving loudness audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
