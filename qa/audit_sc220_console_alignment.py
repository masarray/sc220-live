from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
monitor = (ROOT / "cmake/GenerateSC220MonitorUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
main_cpp = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "CMake generates one reviewed UI authority",
    all(
        token in cmake
        for token in (
            "SC220_UI_GENERATED_SOURCE",
            "GenerateSC220AlignedUi.cmake",
            'set(SC220_UI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/SC220MainComponent.cpp")',
        )
    ),
)
check(
    "aligned monitor and final generators are chained",
    "GenerateSC220MonitorUi.cmake" in aligned
    and "SC220_UI_MONITOR_GENERATED_SOURCE" in aligned
    and 'include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")' in state,
)
check(
    "final polish owns both UI and engine output",
    all(
        token in polish
        for token in (
            'file(READ "${SC220_UI_MONITOR_GENERATED_SOURCE}" _ui)',
            'file(READ "${SC220_ENGINE_GENERATED_SOURCE}" _engine)',
            'file(WRITE "${SC220_UI_MONITOR_GENERATED_SOURCE}" "${_ui}")',
            'file(WRITE "${SC220_ENGINE_GENERATED_SOURCE}" "${_engine}")',
        )
    ),
)
check(
    "one shared grid owns SC220 PC and Stream faders",
    all(
        token in aligned + polish
        for token in (
            "sharedFaderTop",
            "sharedFaderBottom",
            "sharedFaderHeight",
            "sharedMeterWidth",
            "sharedFaderWidth",
            "sc220Meter->setBounds",
            "pcMeter->setBounds",
            "streamMeter->setBounds",
            "sc220Fader.setBounds",
            "pcFader.setBounds",
            "streamFader.setBounds",
        )
    ),
)
check(
    "vertical console grid scales from 720 to 900 pixels",
    all(
        token in polish
        for token in (
            "juce::jlimit (245, 288",
            "static_cast<float> (inputArea.getHeight()) * 0.37f",
            "sharedFaderTop = sourceSelectorY + 82",
            "inputArea.getBottom() - 76",
            "juce::jmax (176",
        )
    ),
)
check(
    "Input Trim and Auto Duck share the pre-fader row",
    "sourceSelectorY - 94, 76, 84" in polish
    and "AUTO DUCK  /  SC220 KEY" in polish
    and "duckAmount.getRight() + 20" not in polish,
)
check(
    "Master receives one quarter of the desktop",
    "0.310f" in polish and "0.440f" in polish,
)
check(
    "Stream and Monitor selectors use separated responsive columns",
    all(
        token in polish
        for token in (
            "masterColumnGap = 14",
            "masterColumnWidth",
            "selectorWidth",
            "juce::jmin (158",
            "outputBox.setBounds",
            "monitorOutputBox.setBounds",
            "programMonitorButton.setBounds",
            "musicCueButton.setBounds",
        )
    ),
)
check(
    "routing labels are explicit and non-ambiguous",
    all(
        token in polish
        for token in (
            '"STREAM OUTPUT"',
            '"MONITOR OUTPUT"',
            '"VB-CABLE / OBS"',
            '"HEADPHONES / SPEAKERS"',
        )
    ),
)
check(
    "Stream selector only lists virtual-cable endpoints",
    "juce::StringArray streamOutputs" in polish
    and 'name.containsIgnoreCase ("CABLE")' in polish
    and 'outputBox.addItemList (streamOutputs, 1)' in polish,
)
check(
    "legacy physical Stream selection is recovered to VB-CABLE",
    "savedDeviceState == nullptr || ! outputCable.load()" in polish,
)
check(
    "post-master protected programme publishes analyzer samples",
    all(
        token in polish
        for token in (
            "masterLimiter.process (masterContext);",
            "programmeSpectrumTap = voiceEnhancerBridge.analysisTap()",
            "programmeSpectrumTap.push",
            "voiceBlock.getReadPointer (0)",
            "voiceBlock.getReadPointer (1)",
        )
    )
    and polish.find("programmeSpectrumTap.push")
    > polish.find("masterLimiter.process (masterContext);"),
)
check(
    "per-lane visual publication stays disabled",
    polish.count("setVisualTapEnabled (false)") >= 4
    and "setVisualTapEnabled (active)" not in polish,
)
check(
    "UI consumes the one programme tap",
    "auto& programmeTap = audioEngine.getVoiceEnhancerBridge().analysisTap()" in polish
    and "programmeTap.setEnabled (uiActive)" in polish
    and "post-master programme spectrum" in polish,
)
check(
    "spectrum work remains lightweight",
    "spectrumDivider >= 2" in polish
    and "fftOrder = 10" in polish
    and "std::array<float, 72> smoothed" in polish,
)
check(
    "FFT implementation remains real audio analysis",
    all(
        token in main_cpp
        for token in (
            "consumeLatest",
            "multiplyWithWindowingTable",
            "performFrequencyOnlyForwardTransform",
            "gainToDecibels",
        )
    ),
)
check(
    "physical Monitor controls remain part of final UI",
    all(
        token in monitor
        for token in (
            "monitorOutputBox",
            "programMonitorButton",
            "musicCueButton",
            "selectMonitorOutputDevice",
        )
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live final console and post-master spectrum audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
