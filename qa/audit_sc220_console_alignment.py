from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
aligned = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
monitor = (ROOT / "cmake/GenerateSC220MonitorUi.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
polish = (ROOT / "cmake/PolishSC220FinalUi.cmake").read_text(encoding="utf-8")
bridge_h = (ROOT / "live/EnhancerEngineBridge.h").read_text(encoding="utf-8")
bridge_cpp = (ROOT / "live/EnhancerEngineBridge.cpp").read_text(encoding="utf-8")
main_cpp = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "CMake generates the reviewed UI authority",
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
    "aligned and monitor generators are chained",
    "GenerateSC220MonitorUi.cmake" in aligned
    and "SC220_UI_MONITOR_GENERATED_SOURCE" in aligned,
)
check(
    "final polish runs after monitor generation",
    'include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")' in state,
)
check(
    "one shared grid still owns SC220 PC and Stream geometry",
    all(
        token in aligned
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
    "lower empty space is used by extended faders",
    "inputArea.getBottom() - 96" in polish
    and "juce::jmax (248, sharedFaderBottom - sharedFaderTop)" in polish,
)
check(
    "master panel receives balanced desktop width",
    "0.310f" in polish and "0.450f" in polish,
)
check(
    "Stream and Monitor selectors use responsive separated columns",
    all(
        token in polish
        for token in (
            "masterColumnGap = 12",
            "masterColumnWidth",
            "selectorWidth",
            "outputBox.setBounds",
            "monitorOutputBox.setBounds",
            "programMonitorButton.setBounds",
            "musicCueButton.setBounds",
        )
    ),
)
check(
    "fixed colliding 164px monitor layout is replaced",
    "responsive stream and monitor columns" in polish
    and "juce::jmin (154" in polish,
)
check(
    "Stream selector stays on the shared source row",
    "outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2" in polish
    and "sourceSelectorY, selectorWidth, 34" in polish,
)
check(
    "visual taps follow UI lifecycle",
    all(
        token in aligned
        for token in (
            "getVoiceEnhancerBridge().setVisualTapEnabled (active)",
            "getMusicEnhancerBridge().setVisualTapEnabled (active)",
        )
    ),
)
check(
    "bridge owns a dedicated analyzer tap",
    all(
        token in bridge_h
        for token in (
            "askp::ScopeTap analyzerTap_",
            "analysisTap()",
            "analyzerTap_.setEnabled (enabled)",
            "std::atomic<bool> visualTapEnabled_",
        )
    ),
)
check(
    "embedded ASK-P tap remains disabled",
    "processor_.scopeTap.setEnabled (false);" in bridge_cpp
    and "processor_.scopeTap.push" not in bridge_cpp,
)
check(
    "post-Signature audio publishes to the dedicated tap",
    all(
        token in bridge_cpp
        for token in (
            "processor_.processBlock (audio, midi);",
            "visualTapEnabled_.load",
            "analyzerTap_.push (left, right, audio.getNumSamples());",
        )
    )
    and bridge_cpp.find("analyzerTap_.push")
    > bridge_cpp.find("processor_.processBlock"),
)
check(
    "UI reads both bridge-owned analyzer taps",
    polish.count(".analysisTap()") == 2,
)
check(
    "spectrum work is throttled to about 15 fps",
    "spectrumDivider >= 2" in polish,
)
check(
    "lightweight 1024-point FFT and 72 display bins are generated",
    "fftOrder = 10" in polish
    and "std::array<float, 72> smoothed" in polish,
)
check(
    "spectrum implementation still consumes Voice and Music",
    all(
        token in main_cpp
        for token in (
            "voiceTap.consumeLatest",
            "musicTap.consumeLatest",
            "performFrequencyOnlyForwardTransform",
        )
    ),
)
check(
    "monitor source controls remain part of final UI",
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
print("SC220 Live responsive console and lightweight analyzer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
