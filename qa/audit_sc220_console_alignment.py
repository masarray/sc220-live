from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
visualizer = (ROOT / "cmake/RebuildSC220ImpactVisualizer.cmake").read_text(encoding="utf-8")
calibrator = (ROOT / "cmake/CalibrateSC220Meters.cmake").read_text(encoding="utf-8")
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
    "atomic analyzer and pink calibration are the final visual passes",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/CalibrateSC220Meters.cmake")' in state
    and state.find("RebuildSC220ImpactVisualizer.cmake")
    < state.find("CalibrateSC220Meters.cmake")
    and "GenerateSC220DirectPcCaptureVisualizer.cmake" not in state,
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
        "masterColumnGap = 14", "outputBox.setBounds", "monitorOutputBox.setBounds",
        "programMonitorButton.setBounds", "musicCueButton.setBounds",
    )),
)
check(
    "telemetry uses fourteen preallocated proportional band-pass filters",
    all(token in telemetry for token in (
        "bandCount = 14", "centreFrequencies", "Coefficients", "State",
        "processSample", "std::array<std::array<State, bandCount>, 2>",
        "lowerBoundary", "upperBoundary", "nominal / bandwidth",
    )),
)
check(
    "telemetry publishes calibrated levels and separate rhythm activity atomically",
    all(token in telemetry for token in (
        "std::array<std::atomic<float>, bandCount> publishedDb",
        "std::array<std::atomic<float>, bandCount> publishedActivity",
        "publishedGeneration.fetch_add", "publishedDb[band].store",
        "publishedActivity[band].store", "readActivity",
    )),
)
check(
    "telemetry keeps a stable 180 ms baseline plus fast per-frequency activity",
    all(token in telemetry for token in (
        "calibratedPower", "fastPower", "calibratedSmoothingSeconds = 0.180",
        "fastAttackSeconds", "fastReleaseSeconds", "activityThresholdDb",
        "activityRangeDb",
    )),
)
check(
    "telemetry has no queue allocation lock or FFT implementation",
    all(token not in telemetry for token in (
        "std::vector", "std::deque", "std::queue", "juce::AbstractFifo",
        "std::mutex", "juce::CriticalSection", "condition_variable",
        "juce::dsp::FFT", "performFrequencyOnlyForwardTransform", "new ",
    )),
)
check(
    "exact post-limiter programme buffer feeds the band telemetry",
    all(token in generated_engine for token in (
        "masterLimiter.process (masterContext);",
        "programmeBandTelemetry.process",
        "voiceBlock.getReadPointer (0)", "voiceBlock.getReadPointer (1)",
    ))
    and generated_engine.find("programmeBandTelemetry.process")
    > generated_engine.find("masterLimiter.process (masterContext);"),
)
check(
    "UI reads pink-calibrated levels and separate activity without sample transport",
    all(token in generated_ui for token in (
        "updateFromTelemetry", "programmeBandTelemetry.read",
        "programmeBandTelemetry.readActivity", "rawActivity", "rhythm * 0.20f",
        "PINK-FLAT BANDS", "WAITING FOR ENGINE BANDS",
        "PROGRAM ANALYZER", "const float displayDb = rawDb[band]",
    )),
)
check(
    "deleted visualizer paths are absent",
    all(token not in generated_ui + generated_engine for token in (
        "ProgrammeSpectrumSnapshot", "programmeSpectrumSnapshot",
        "performFrequencyOnlyForwardTransform", "updateFromSnapshot",
        "updateFromTaps", "programmeTap.consumeLatest", "musicTap.consumeLatest",
        "voiceTap.consumeLatest", "PcAudioLoopbackCapture::consumeLatestVisual",
        "MUSIC RECOVERY", "VOICE RECOVERY", "PC CAPTURE", "NO FRAMES",
    )),
)
check(
    "rhythmic visual lift pulse and peak hold remain deterministic",
    all(token in generated_ui for token in (
        "segmentCount = 16", "peakHoldFrames[band] = 14",
        "expandedDb = -38.0f", "* 1.32f", "positiveRise",
        "pulses[band]", "releaseMix", "heldPeaks[band] - 0.032f",
        "baseTarget + rhythm * 0.20f",
    )),
)
check(
    "calibration pass removes the old visual EQ contour",
    all(token in calibrator for token in (
        "remove cosmetic analyzer contour", "PINK-FLAT BANDS",
    ))
    and all(token not in generated_ui for token in (
        "displayDb += 4.0f", "displayDb += 2.0f", "displayDb -= 1.5f",
    )),
)
check(
    "obsolete time-history visualizer is absent",
    all(token not in generated_ui for token in (
        "PROGRAM IMPACT", "PC INPUT IMPACT", '"OLDER"', '"NOW"', "historySize = 32",
    )),
)
check(
    "physical Monitor controls remain part of final UI",
    all(token in generated_ui for token in (
        "monitorOutputBox", "programMonitorButton", "musicCueButton",
        "selectMonitorOutputDevice",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live final console and rhythmic pink-flat analyzer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
