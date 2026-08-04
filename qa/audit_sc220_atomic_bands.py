from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
telemetry = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
generator = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")

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
    "telemetry owns exactly fourteen fixed bands",
    all(token in telemetry for token in (
        "bandCount = 14", "std::array<Coefficients, bandCount>",
        "std::array<std::array<State, bandCount>, 2>",
        "50.0f, 80.0f, 125.0f", "12000.0f, 16000.0f",
    )),
)
check(
    "each band is a real second-order band-pass filter",
    all(token in telemetry for token in (
        "alpha / a0", "-alpha / a0", "-2.0 * std::cos",
        "processSample", "state.x2", "state.y2",
    )),
)
check(
    "producer publishes atomic dB telemetry per callback",
    all(token in telemetry for token in (
        "std::array<std::atomic<float>, bandCount>",
        "20.0f * std::log10", "publishedDb[band].store",
        "publishedGeneration.fetch_add",
    )),
)
check(
    "telemetry cannot queue wait lock allocate or run FFT",
    all(token not in telemetry for token in (
        "ScopeTap", "ProgrammeSpectrumSnapshot", "queue", "mutex",
        "condition_variable", "std::vector", "new ", "FFT", "memcpy",
    )),
)
check(
    "atomic band generator is the final visual authority",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and "atomic" in generator.lower()
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
    "generated UI reads only atomic band values",
    all(token in ui for token in (
        '#include "ProgrammeBandTelemetry.h"',
        "programmeBandTelemetry.read", "updateFromTelemetry",
        "POST-LIMITER BANDS", "PROGRAM ANALYZER",
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
check("generated final sources exist", engine_path.exists() and ui_path.exists())

failed = [name for name, ok in checks if not ok]
print("SC220 Live atomic post-limiter band telemetry audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
