from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
generator = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")
bridge_h = (ROOT / "live/EnhancerEngineBridge.h").read_text(encoding="utf-8")
bridge_cpp = (ROOT / "live/EnhancerEngineBridge.cpp").read_text(encoding="utf-8")
main_cpp = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "CMake generates aligned production UI source",
    all(
        token in cmake
        for token in (
            "SC220_UI_GENERATED_SOURCE",
            "GenerateSC220AlignedUi.cmake",
            '"${SC220_UI_GENERATED_SOURCE}"',
        )
    ),
)
check(
    "original UI is an explicit generator input",
    'set(SC220_UI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/live/SC220MainComponent.cpp")'
    in cmake,
)
check(
    "one console grid owns all fader dimensions",
    all(
        token in generator
        for token in (
            "sharedFaderTop",
            "sharedFaderBottom",
            "sharedFaderHeight",
            "sharedMeterWidth",
            "sharedFaderWidth",
        )
    ),
)
check(
    "SC220 PC and Stream meters share geometry",
    all(
        token in generator
        for token in (
            "sc220Meter->setBounds",
            "pcMeter->setBounds",
            "streamMeter->setBounds",
            "sharedMeterWidth, sharedFaderHeight",
        )
    ),
)
check(
    "SC220 PC and Stream faders share geometry",
    all(
        token in generator
        for token in (
            "sc220Fader.setBounds",
            "pcFader.setBounds",
            "streamFader.setBounds",
            "sharedFaderWidth, sharedFaderHeight",
        )
    ),
)
check(
    "System Mix and Stream Output use same source row",
    all(
        token in generator
        for token in (
            "pcSourceBox.setBounds",
            "outputBox.setBounds",
            "sourceSelectorY",
        )
    )
    and generator.count("sourceSelectorY, 164, 34") >= 2,
)
check(
    "Stream title follows output selector",
    'g.drawText ("STREAM", outputBox.getX(), outputBox.getY() - 47' in generator,
)
check(
    "visual taps follow UI lifecycle",
    all(
        token in generator
        for token in (
            "getVoiceEnhancerBridge().setVisualTapEnabled (active)",
            "getMusicEnhancerBridge().setVisualTapEnabled (active)",
        )
    ),
)
check(
    "bridge owns analyzer enable state",
    "std::atomic<bool> visualTapEnabled_" in bridge_h
    and "visualTapEnabled_.store" in bridge_h,
)
check(
    "embedded publisher is disabled before DSP",
    bridge_cpp.find("processor_.scopeTap.setEnabled (false);")
    < bridge_cpp.find("processor_.processBlock (audio, midi);"),
)
check(
    "post-Signature block is published explicitly",
    all(
        token in bridge_cpp
        for token in (
            "processor_.processBlock (audio, midi);",
            "visualTapEnabled_.load",
            "processor_.scopeTap.push (left, right, audio.getNumSamples());",
        )
    )
    and bridge_cpp.find("processor_.scopeTap.push")
    > bridge_cpp.find("processor_.processBlock"),
)
check(
    "spectrum still consumes both live engine taps",
    all(
        token in main_cpp
        for token in (
            "voiceTap.consumeLatest",
            "musicTap.consumeLatest",
            "performFrequencyOnlyForwardTransform",
        )
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live aligned console and analyzer bridge audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
