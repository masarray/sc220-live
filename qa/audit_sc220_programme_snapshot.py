from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
snapshot = (ROOT / "live/ProgrammeSpectrumSnapshot.h").read_text(encoding="utf-8")
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
    "snapshot owns exactly two preallocated 512-sample stereo banks",
    all(token in snapshot for token in (
        "frameSize = 512",
        "std::array<float, frameSize> left",
        "std::array<float, frameSize> right",
        "std::array<Bank, 2> banks",
    )),
)
check(
    "producer publishes only complete banks",
    all(token in snapshot for token in (
        "if (writeOffset == frameSize)",
        "bank.sequence.fetch_add (1",
        "publishedBank.store",
        "publishedGeneration.fetch_add",
        "writeBank ^= 1",
    )),
)
check(
    "consumer rejects partial or changed banks",
    all(token in snapshot for token in (
        "sequenceBefore",
        "sequenceAfter",
        "sequenceBefore != sequenceAfter",
        "generation == consumerGeneration",
    )),
)
check(
    "snapshot producer cannot wait allocate or lock",
    "std::memcpy" in snapshot
    and "mutex" not in snapshot.lower()
    and "condition_variable" not in snapshot
    and "new " not in snapshot
    and "sleep" not in snapshot.lower(),
)
check(
    "old direct-capture visual generator is no longer chained",
    "GenerateSC220DirectPcCaptureVisualizer.cmake" not in state,
)
check(
    "snapshot generator is the final visual authority",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and "post-limiter snapshot" in state.lower(),
)
check(
    "generated engine includes and resets snapshot",
    all(token in engine for token in (
        '#include "ProgrammeSpectrumSnapshot.h"',
        "programmeSpectrumSnapshot.reset()",
    )),
)
check(
    "generated engine publishes the exact post-limiter buffer",
    all(token in engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeSpectrumSnapshot.push (voiceBlock.getReadPointer (0)",
        "voiceBlock.getReadPointer (1), numSamples",
    ))
    and engine.find("programmeSpectrumSnapshot.push")
    > engine.find("masterLimiter.process (masterContext);"),
)
check(
    "production engine no longer publishes ScopeTap samples",
    "programmeAnalyzer.push" not in engine,
)
check(
    "generated UI consumes snapshot directly",
    all(token in ui for token in (
        '#include "ProgrammeSpectrumSnapshot.h"',
        "programmeSpectrumSnapshot.consumeLatest",
        "updateFromSnapshot",
        "POST-LIMITER SNAPSHOT",
    )),
)
check(
    "generated UI has no queue fallback sources",
    all(token not in ui for token in (
        "updateFromTaps",
        "programmeTap.consumeLatest",
        "musicTap.consumeLatest",
        "voiceTap.consumeLatest",
        "PcAudioLoopbackCapture::consumeLatestVisual",
        "MUSIC RECOVERY",
        "VOICE RECOVERY",
        "PC CAPTURE",
        "NO FRAMES",
    )),
)
check(
    "FFT work remains entirely in UI source",
    "performFrequencyOnlyForwardTransform" in ui
    and "performFrequencyOnlyForwardTransform" not in engine
    and "performFrequencyOnlyForwardTransform" not in snapshot,
)
check(
    "generated final sources exist",
    engine_path.exists() and ui_path.exists(),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live deterministic post-limiter programme snapshot audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
