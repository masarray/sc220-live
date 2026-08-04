from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
spectralizer = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")
snapshot = (ROOT / "live/ProgrammeSpectrumSnapshot.h").read_text(encoding="utf-8")

generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"
if not generated_root.exists():
    generated_root = ROOT / "generated" / "sc220-monitor"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
generated_engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
generated_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else ""
generated_engine = generated_engine_path.read_text(encoding="utf-8") if generated_engine_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "normal CMake build compiles generated engine and final UI",
    all(token in aligned for token in (
        'set_source_files_properties("${SC220_ENGINE_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)',
        'set_source_files_properties("${SC220_UI_GENERATED_SOURCE}" PROPERTIES HEADER_FILE_ONLY TRUE)',
        '"${SC220_ENGINE_GENERATED_SOURCE}"',
        '"${SC220_UI_MONITOR_GENERATED_SOURCE}"',
    )),
)
check(
    "snapshot spectralizer is the last visual generator",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and "GenerateSC220DirectPcCaptureVisualizer.cmake" not in state,
)
check(
    "final generated sources exist",
    generated_ui_path.exists() and generated_engine_path.exists(),
)
check(
    "final polish still owns responsive console geometry",
    all(token in polish for token in (
        "sharedFaderTop",
        "sharedFaderBottom",
        "masterColumnGap",
        "STREAM OUTPUT",
        "MONITOR OUTPUT",
    )),
)
check(
    "one shared grid owns SC220 PC and Stream faders",
    all(token in generated_ui for token in (
        "sharedFaderTop",
        "sharedFaderBottom",
        "sharedFaderHeight",
        "sharedFaderWidth",
        "sc220Meter->setBounds",
        "pcMeter->setBounds",
        "streamMeter->setBounds",
        "sc220Fader.setBounds",
        "pcFader.setBounds",
        "streamFader.setBounds",
    )),
)
check(
    "Stream and Monitor selectors remain separated",
    all(token in generated_ui for token in (
        "masterColumnGap = 14",
        "outputBox.setBounds",
        "monitorOutputBox.setBounds",
        "programMonitorButton.setBounds",
        "musicCueButton.setBounds",
    )),
)
check(
    "snapshot is a complete-frame double bank",
    all(token in snapshot for token in (
        "frameSize = 512",
        "std::array<Bank, 2>",
        "writeBank ^= 1",
        "publishedBank.store",
        "publishedGeneration.fetch_add",
        "sequenceBefore",
        "sequenceAfter",
    )),
)
check(
    "audio producer never waits or allocates",
    "std::memcpy" in snapshot
    and "mutex" not in snapshot.lower()
    and "condition_variable" not in snapshot
    and "new " not in snapshot,
)
check(
    "post-limiter buffer is the only production visual source",
    all(token in generated_engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeSpectrumSnapshot.push",
        "voiceBlock.getReadPointer (0)",
        "voiceBlock.getReadPointer (1)",
    ))
    and generated_engine.find("programmeSpectrumSnapshot.push")
    > generated_engine.find("masterLimiter.process (masterContext);"),
)
check(
    "UI reads only complete post-limiter snapshots",
    all(token in generated_ui for token in (
        "updateFromSnapshot",
        "programmeSpectrumSnapshot.consumeLatest",
        "POST-LIMITER SNAPSHOT",
        "WAITING FOR POST-LIMITER AUDIO",
    )),
)
check(
    "all previous production tap fallbacks are gone",
    all(token not in generated_ui for token in (
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
    "FFT remains lightweight and outside the engine",
    all(token in generated_ui for token in (
        "fftOrder = 9",
        "bandCount = 24",
        "segmentCount = 15",
        "performFrequencyOnlyForwardTransform",
        "stereoPower += 0.5f",
        "peakHoldFrames[band] = 15",
    ))
    and "performFrequencyOnlyForwardTransform" not in generated_engine,
)
check(
    "obsolete time-history visualizer is absent",
    all(token not in generated_ui for token in (
        "PROGRAM IMPACT",
        "PC INPUT IMPACT",
        '"OLDER"',
        '"NOW"',
        "historySize = 32",
    )),
)
check(
    "physical Monitor controls remain part of final UI",
    all(token in generated_ui for token in (
        "monitorOutputBox",
        "programMonitorButton",
        "musicCueButton",
        "selectMonitorOutputDevice",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live final console and deterministic post-limiter snapshot audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
