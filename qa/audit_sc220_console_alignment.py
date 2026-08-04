from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
impact = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")

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
    "CMake generates one reviewed UI authority",
    all(token in cmake for token in (
        "SC220_UI_GENERATED_SOURCE",
        "GenerateSC220AlignedUi.cmake",
        'set(SC220_UI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/SC220MainComponent.cpp")',
    )),
)
check(
    "aligned monitor final and impact generators are chained",
    "GenerateSC220MonitorUi.cmake" in aligned
    and "SC220_UI_MONITOR_GENERATED_SOURCE" in aligned
    and 'include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/StabilizeSC220PcMusicMode.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and state.find("StabilizeSC220PcMusicMode.cmake")
    < state.find("RebuildSC220ImpactVisualizer.cmake"),
)
check(
    "final generated sources exist for audit",
    generated_ui_path.exists() and generated_engine_path.exists(),
)
check(
    "final polish owns both UI and engine output",
    all(token in polish for token in (
        'file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)',
        'file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)',
        'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
        'file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")',
    )),
)
check(
    "impact generator owns the final visualizer output",
    all(token in impact for token in (
        "sc220_impact_replace_range",
        "SC220_UI_MONITOR_GENERATED_SOURCE",
        "PROGRAM IMPACT",
        "updateFromLevels",
        'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
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
    "stereo meters symmetrically flank all three faders",
    "sc220StripArea.getWidth() - 36" in generated_ui
    and "pcStripArea.getWidth() - 36" in generated_ui
    and "streamColumn.getWidth() - 10" in generated_ui
    and "setInterceptsMouseClicks (false, false)" in generated_ui,
)
check(
    "vertical console grid scales from 720 to 900 pixels",
    all(token in generated_ui for token in (
        "juce::jlimit (245, 288",
        "static_cast<float> (inputArea.getHeight()) * 0.37f",
        "sharedFaderTop = sourceSelectorY + 82",
        "inputArea.getBottom() - 76",
        "juce::jmax (176",
    )),
)
check(
    "Input Trim and Auto Duck share the pre-fader row",
    "duckAmount.setBounds (pcStripArea.getCentreX() - 38" in generated_ui
    and "sourceSelectorY - 94, 76, 84" in generated_ui.replace("\n", " ")
    and "AUTO DUCK  /  SC220 KEY" in generated_ui
    and "duckAmount.getRight() + 20" not in generated_ui,
)
check(
    "Master receives one quarter of the desktop",
    "0.310f" in generated_ui and "0.440f" in generated_ui,
)
check(
    "Stream and Monitor selectors use separated responsive columns",
    all(token in generated_ui for token in (
        "masterColumnGap = 14",
        "masterColumnWidth",
        "selectorWidth",
        "juce::jmin (158",
        "outputBox.setBounds",
        "monitorOutputBox.setBounds",
        "programMonitorButton.setBounds",
        "musicCueButton.setBounds",
    )),
)
check(
    "routing labels are explicit and non-ambiguous",
    all(token in generated_ui for token in (
        '"STREAM OUTPUT"',
        '"MONITOR OUTPUT"',
        '"VB-CABLE / OBS"',
        '"HEADPHONES / SPEAKERS"',
    )),
)
check(
    "Stream selector only lists virtual-cable endpoints",
    "juce::StringArray streamOutputs" in generated_ui
    and 'name.containsIgnoreCase ("CABLE")' in generated_ui
    and 'outputBox.addItem (i == 0 ? "VB-CABLE"' in generated_ui
    and "audioEngine.selectOutputDevice (streamOutputs[selectedIndex])" in generated_ui,
)
check(
    "legacy physical Stream selection is recovered to VB-CABLE",
    "savedDeviceState == nullptr || ! outputCable.load()" in generated_engine,
)
check(
    "post-master protected programme still publishes analyzer diagnostics",
    all(token in generated_engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeAnalyzer.push",
        "voiceBlock.getReadPointer (0)",
        "voiceBlock.getReadPointer (1)",
    ))
    and generated_engine.find("programmeAnalyzer.push")
    > generated_engine.find("masterLimiter.process (masterContext);"),
)
check(
    "per-lane visual publication stays disabled in final UI",
    generated_ui.count("setVisualTapEnabled (false)") >= 4
    and "setVisualTapEnabled (active)" not in generated_ui,
)
check(
    "UI impact meter uses live output telemetry with PC input fallback",
    all(token in generated_ui for token in (
        "spectrumDisplay->updateFromLevels",
        "lastStatus.outputPeakDbL",
        "lastStatus.outputPeakDbR",
        "lastStatus.pcPeakDbL",
        "lastStatus.pcPeakDbR",
        "lastStatus.pcLoopbackRunning",
    ))
    and "updateFromTap (audioEngine.programmeAnalyzerTap()" not in generated_ui,
)
check(
    "impact meter remains lightweight and has deterministic peak hold",
    all(token in generated_ui for token in (
        "historySize = 32",
        "peakHoldFrames = 15",
        "500 ms PEAK HOLD",
        "envelope += (target - envelope) * 0.82f",
        "heldPeak - 0.025f",
    )),
)
check(
    "obsolete production FFT display is removed",
    "performFrequencyOnlyForwardTransform" not in generated_ui
    and "32 BAND  /  15 FPS" not in generated_ui
    and "NO PROGRAM AUDIO" not in generated_ui
    and "PROGRAM IMPACT" in generated_ui,
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
print("SC220 Live generated final console and OBS-style impact visualizer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
