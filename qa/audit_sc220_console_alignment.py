from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
spectralizer = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")

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
    "aligned monitor final and spectralizer generators are chained",
    "GenerateSC220MonitorUi.cmake" in aligned
    and "SC220_UI_MONITOR_GENERATED_SOURCE" in aligned
    and 'include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/StabilizeSC220PcMusicMode.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and state.find("StabilizeSC220PcMusicMode.cmake")
    < state.find("RebuildSC220ImpactVisualizer.cmake"),
)
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
    "spectralizer generator owns final UI and engine publication output",
    all(token in spectralizer for token in (
        "sc220_spectrum_replace_range",
        "SC220_UI_MONITOR_GENERATED_SOURCE",
        "SC220_ENGINE_GENERATED_SOURCE",
        "PROGRAM SPECTRUM",
        "updateFromTaps",
        'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
        'file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")',
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
    "post-master protected programme publishes after limiting",
    all(token in generated_engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeAnalyzer.setEnabled (true);",
        "programmeAnalyzer.push",
        "voiceBlock.getReadPointer (0)",
        "voiceBlock.getReadPointer (1)",
    ))
    and generated_engine.find("programmeAnalyzer.setEnabled (true);",
                              generated_engine.find("masterLimiter.process (masterContext);"))
    < generated_engine.find("programmeAnalyzer.push",
                            generated_engine.find("masterLimiter.process (masterContext);")),
)
check(
    "voice and music recovery taps remain armed across UI lifecycle",
    generated_ui.count("getVoiceEnhancerBridge().setVisualTapEnabled (true)") >= 3
    and generated_ui.count("getMusicEnhancerBridge().setVisualTapEnabled (true)") >= 3
    and "setVisualTapEnabled (active)" not in generated_ui,
)
check(
    "UI consumes post-master then Music then Voice taps",
    all(token in generated_ui for token in (
        "updateFromTaps",
        "programmeTap.consumeLatest",
        "musicTap.consumeLatest",
        "voiceTap.consumeLatest",
        "audioEngine.programmeAnalyzerTap()",
        "getMusicEnhancerBridge().analysisTap()",
        "getVoiceEnhancerBridge().analysisTap()",
        "MUSIC RECOVERY",
        "VOICE RECOVERY",
    )),
)
check(
    "runtime timer re-arms all visual publication paths",
    all(token in generated_ui for token in (
        "audioEngine.setProgrammeAnalyzerEnabled (true);",
        "audioEngine.getMusicEnhancerBridge().setVisualTapEnabled (true);",
        "audioEngine.getVoiceEnhancerBridge().setVisualTapEnabled (true);",
        "WAITING FOR ANALYZER FRAMES",
    )),
)
check(
    "spectralizer uses lightweight stereo FFT outside the audio callback",
    all(token in generated_ui for token in (
        "fftOrder = 9",
        "bandCount = 24",
        "leftFftData",
        "rightFftData",
        "performFrequencyOnlyForwardTransform",
        "stereoPower += 0.5f",
    ))
    and "performFrequencyOnlyForwardTransform" not in generated_engine,
)
check(
    "spectral bars are fixed frequency positions with segmented rendering",
    all(token in generated_ui for token in (
        "lowerFrequency = 35.0f",
        "upperFrequency",
        "segmentCount = 15",
        "PROGRAM SPECTRUM",
        '"40"',
        '"1k"',
        '"12k"',
    )),
)
check(
    "per-band ballistics and peak hold are deterministic",
    all(token in generated_ui for token in (
        "levels[band] += (target - levels[band]) * 0.82f",
        "levels[band] * 0.88f + target * 0.12f",
        "peakHoldFrames[band] = 15",
        "heldPeaks[band] - 0.026f",
    )),
)
check(
    "obsolete one-tap and time-history visualizers are removed",
    "spectrumDisplay->updateFromTap" not in generated_ui
    and "PROGRAM IMPACT" not in generated_ui
    and "PC INPUT IMPACT" not in generated_ui
    and '"OLDER"' not in generated_ui
    and '"NOW"' not in generated_ui
    and "historySize = 32" not in generated_ui,
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
print("SC220 Live final console and redundant OBS spectralizer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
