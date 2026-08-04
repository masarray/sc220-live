from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
telemetry = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")
integrator = (ROOT / "live/ProgrammeLoudnessIntegrator.h").read_text(encoding="utf-8")
live_loudness = (ROOT / "live/ProgrammeLiveLoudnessTelemetry.h").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
generator = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")
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
    "telemetry owns exactly fourteen fixed logarithmic bands",
    all(token in telemetry for token in (
        "bandCount = 14", "std::array<Coefficients, bandCount>",
        "std::array<std::array<State, bandCount>, 2>",
        "50.0f, 80.0f, 125.0f", "12000.0f, 16000.0f",
        "lowerBoundary", "upperBoundary", "nominal / bandwidth",
    )),
)
check(
    "each band is a real proportional-bandwidth second-order filter",
    all(token in telemetry for token in (
        "alpha / a0", "-alpha / a0", "-2.0 * std::cos",
        "processSample", "state.x2", "state.y2",
        "std::clamp (nominal / bandwidth",
    )),
)
check(
    "pink calibration integrates the actual digital response in log frequency",
    all(token in telemetry for token in (
        "calculatePinkNoiseCalibration", "integrationPoints = 4096",
        "Equal steps in ln(f)", "responsePower", "pinkCalibrationDb",
        "referenceDb - responseDb[band]",
    )),
)
check(
    "producer publishes frequency-dependent rhythmic atomic power telemetry",
    all(token in telemetry for token in (
        "std::array<std::atomic<float>, bandCount>",
        "envelopePower", "attackSeconds", "releaseSeconds",
        "envelopeSeconds", "coefficient", "publishedDb[band].store",
        "publishedGeneration.fetch_add",
    )),
)
check(
    "steady pink calibration remains the only per-band level correction",
    "pinkCalibrationDb[band]" in telemetry
    and "transientDb" not in telemetry
    and "transientEmphasis" not in telemetry
    and "slowPower" not in telemetry,
)
check(
    "telemetry has no concrete queue lock allocation or FFT implementation",
    all(token not in telemetry for token in (
        "ScopeTap", "ProgrammeSpectrumSnapshot", "std::queue", "std::deque",
        "std::vector", "juce::AbstractFifo", "std::mutex",
        "juce::CriticalSection", "condition_variable", "new ",
        "juce::dsp::FFT", "performFrequencyOnlyForwardTransform", "std::memcpy",
    )),
)
check(
    "atomic analyzer and final calibration passes are chained in order",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/CalibrateSC220Meters.cmake")' in state
    and state.find("RebuildSC220ImpactVisualizer.cmake")
    < state.find("CalibrateSC220Meters.cmake")
    and "GenerateSC220DirectPcCaptureVisualizer.cmake" not in state,
)
check(
    "generated engine prepares telemetry at device start",
    all(token in engine for token in (
        '#include "ProgrammeBandTelemetry.h"',
        "programmeBandTelemetry.prepare (sampleRate)",
    )),
)
check(
    "generated engine measures exact protected post-limiter buffer",
    all(token in engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeBandTelemetry.process (voiceBlock.getReadPointer (0)",
        "voiceBlock.getReadPointer (1), numSamples",
    ))
    and engine.find("programmeBandTelemetry.process")
    > engine.find("masterLimiter.process (masterContext);"),
)
check("production engine no longer publishes analyzer sample queues", "programmeAnalyzer.push" not in engine)
check(
    "generated UI reads calibrated atomic bands without cosmetic EQ contour",
    all(token in ui for token in (
        '#include "ProgrammeBandTelemetry.h"',
        "programmeBandTelemetry.read", "updateFromTelemetry",
        "PINK-FLAT BANDS", "PROGRAM ANALYZER",
        "const float displayDb = rawDb[band]",
    ))
    and all(token not in ui for token in (
        "displayDb += 4.0f", "displayDb += 2.0f", "displayDb -= 1.5f",
    )),
)
check(
    "generated UI expands movement equally across bands and adds transient caps",
    all(token in ui for token in (
        "expandedDb = -38.0f", "* 1.32f", "(expandedDb + 64.0f) / 52.0f",
        "positiveRise", "pulses[band]", "segmentCount = 16",
        "peakHoldFrames[band] = 14", "releaseMix",
    )),
)
check(
    "generated UI has no FFT snapshot or fallback source",
    all(token not in ui + engine for token in (
        "performFrequencyOnlyForwardTransform", "multiplyWithWindowingTable",
        "ProgrammeSpectrumSnapshot", "programmeSpectrumSnapshot",
        "updateFromSnapshot", "updateFromTaps", "programmeTap.consumeLatest",
        "musicTap.consumeLatest", "voiceTap.consumeLatest",
        "PcAudioLoopbackCapture::consumeLatestVisual", "MUSIC RECOVERY",
        "VOICE RECOVERY", "PC CAPTURE", "NO FRAMES",
    )),
)
check(
    "integrator exposes a real 400 ms momentary loudness value",
    all(token in integrator for token in (
        "momentaryLufs()", "momentary = lufsFromEnergy (blockMeanEnergy)",
        "Four adjacent 100 ms segments", "momentaryReady()",
    )),
)
check(
    "live loudness bridge is atomic and generated engine publishes it",
    all(token in live_loudness for token in (
        "std::atomic<float>", "publishedGeneration", "publish (float value)",
    ))
    and "programmeLiveLoudnessTelemetry.publish (loudnessIntegrator.momentaryLufs())" in engine,
)
check(
    "loudness dial uses LUFS-M for movement and LUFS-I for the main number",
    all(token in ui for token in (
        "programmeLiveLoudnessTelemetry.read", "nextMomentary",
        "needleLufs", "displayLufs = needleInitialised",
        '"LUFS-I"', '"M " + juce::String',
    )),
)
check(
    "calibration generator owns final engine and UI output",
    all(token in calibrator for token in (
        'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
        'file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")',
        "remove cosmetic analyzer contour", "drive loudness needle from LUFS-M",
    )),
)
check("generated final sources exist", engine_path.exists() and ui_path.exists())

failed = [name for name, ok in checks if not ok]
print("SC220 Live rhythmic pink-flat atomic bands and moving loudness audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
