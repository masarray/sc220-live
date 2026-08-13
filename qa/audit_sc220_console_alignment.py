from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
visualizer = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")
telemetry = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")

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
    "natural live policy runs before restored responsive visualizer",
    "FinalizeSC220NaturalLivePath.cmake" in state
    and "RebuildSC220ImpactVisualizer.cmake" in state
    and state.index("FinalizeSC220NaturalLivePath.cmake")
        < state.index("RebuildSC220ImpactVisualizer.cmake"),
)
check("final generated sources exist", generated_ui_path.exists() and generated_engine_path.exists())
check(
    "responsive console geometry remains intact",
    all(token in polish for token in (
        "sharedFaderTop", "sharedFaderBottom", "masterColumnGap",
        "STREAM OUTPUT", "MONITOR OUTPUT",
    )),
)
check(
    "one shared grid owns SC220 PC and Stream faders",
    all(token in generated_ui for token in (
        "sharedFaderTop", "sharedFaderBottom", "sharedFaderHeight",
        "sharedFaderWidth", "sc220Meter->setBounds", "pcMeter->setBounds",
        "streamMeter->setBounds", "sc220Fader.setBounds",
        "pcFader.setBounds", "streamFader.setBounds",
    )),
)
check(
    "Stream and Monitor selectors remain separated",
    all(token in generated_ui for token in (
        "masterColumnGap = 12", "outputBox.setBounds", "monitorOutputBox.setBounds",
        "programMonitorButton.setBounds", "musicCueButton.setBounds",
    )),
)
check(
    "atomic analyzer uses fourteen preallocated stereo band filters",
    all(token in telemetry for token in (
        "bandCount = 14", "centreFrequencies", "processSample",
        "std::array<std::array<State, bandCount>, 2>",
        "publishedGeneration.fetch_add",
    )),
)
check(
    "exact post-limiter programme buffer feeds atomic band telemetry",
    all(token in generated_engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeBandTelemetry.process",
        "voiceBlock.getReadPointer (0)", "voiceBlock.getReadPointer (1)",
    ))
    and generated_engine.find("programmeBandTelemetry.process")
        > generated_engine.find("masterLimiter.process (masterContext);"),
)
check(
    "UI reads atomic levels with fast transient ballistics",
    all(token in generated_ui for token in (
        "updateFromTelemetry", "programmeBandTelemetry.read",
        "programmeBandTelemetry.readActivity", "activity[band]",
        "constexpr int segments = 16", "const float rise",
        "pulses[band]", "14 BAND  /  30 FPS",
    )),
)
check(
    "old queued FFT visualizer is absent from generated programme path",
    all(token not in generated_ui + generated_engine for token in (
        "programmeTap.consumeLatest", "performFrequencyOnlyForwardTransform",
        "multiplyWithWindowingTable", "updateFromTap (audioEngine.programmeAnalyzerTap()",
    )),
)
check(
    "physical Monitor controls remain part of final UI",
    all(token in generated_ui for token in (
        "monitorOutputBox", "programMonitorButton", "musicCueButton",
        "selectMonitorOutputDevice",
    )),
)
check(
    "truthful Stream endpoint UI remains intact",
    "SELECT STREAM DEVICE" in generated_ui
    and "VB-CABLE 2" not in generated_ui
    and "VB-CABLE 3" not in generated_ui,
)
check(
    "restored visualizer source itself is fail-closed against old FFT route",
    "obsolete queued FFT visualizer remains" in visualizer
    and "programmeBandTelemetry.process" in visualizer,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live generated console and responsive atomic analyzer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
