from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
generated_root = ROOT / "generated" / "sc220-monitor"
engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
stabilizer_path = ROOT / "cmake" / "StabilizeSC220PcMusicMode.cmake"
state_path = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"

engine = engine_path.read_text(encoding="utf-8") if engine_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""
stabilizer = stabilizer_path.read_text(encoding="utf-8")
state = state_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "PC Music stabilizer runs after final UI polish",
    'include("${CMAKE_CURRENT_LIST_DIR}/StabilizeSC220PcMusicMode.cmake")' in state
    and state.find("PolishSC220FinalUi.cmake")
    < state.find("StabilizeSC220PcMusicMode.cmake"),
)
check(
    "programme graph opens output-first and does not require SC220 input",
    "deviceManager.initialise (0, 2, savedDeviceState, true)" in engine
    and "deviceManager.initialise (2, 2, savedDeviceState, true)" not in engine,
)
check(
    "output changes do not invent a default hardware input",
    all(token in engine for token in (
        "if (inputName != nullptr)",
        "else if (requested.inputDeviceName.isEmpty())",
        "requested.useDefaultInputChannels = false",
    )),
)
check(
    "SC220 is still auto-selected when MIXER becomes available",
    all(token in engine for token in (
        "looksLikeSc220Endpoint",
        "for (const auto& name : getInputDeviceNames())",
        "selectInputDevice (name)",
    )),
)
check(
    "zero-input callback clears Voice but continues into PC Music processing",
    all(token in engine for token in (
        "if (numInputChannels > 0",
        "voiceBlock.clear",
        "pcLoopback.read (musicBlock, numSamples)",
        "musicEnhancerBridge.process (musicBlock, musicMidi)",
        "programmeAnalyzer.push",
    )),
)
check(
    "different VB-CABLE families are distinguishable",
    all(token in engine for token in (
        'return "VBCABLEA"',
        'return "VBCABLEB"',
        'return "VBCABLEC"',
        'return "VBCABLED"',
        'return "VBCABLE"',
    )),
)
check(
    "PC Audio list allows virtual sources except the exact Stream cable",
    "if (pcEndpointConflictsWithStream (endpoint.name, streamOutput))" in engine
    and "isUnsafeVirtualEndpoint (endpoint.name)" not in engine[
        engine.find("juce::StringArray LiveAudioEngine::getPcAudioEndpointNames"):
        engine.find("juce::String LiveAudioEngine::selectPcAudioEndpoint")
    ],
)
check(
    "same endpoint or same virtual cable family is blocked",
    all(token in engine for token in (
        "PcAudioLoopbackCapture::endpointNamesMatch (source, streamOutput)",
        "sourceCable.isNotEmpty() && sourceCable == streamCable",
        "PC Audio cannot capture the same cable used by Stream Output",
    )),
)
check(
    "programme analyzer remains enabled independent of window lifecycle",
    "programmeAnalyzer.setEnabled (true)" in engine
    and "audioEngine.setProgrammeAnalyzerEnabled (false);" not in ui
    and "audioEngine.setProgrammeAnalyzerEnabled (active);" not in ui
    and "audioEngine.setProgrammeAnalyzerEnabled (uiActive);" not in ui,
)
check(
    "UI still consumes protected post-master programme audio",
    "audioEngine.programmeAnalyzerTap()" in ui
    and "spectrumDisplay->updateFromTap" in ui,
)
check(
    "generated final sources exist",
    engine_path.exists() and ui_path.exists(),
)
check(
    "stabilizer documents safe virtual source and PC-only intent",
    "different cable" in stabilizer.lower()
    and "without an SC220 input device" in stabilizer,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live PC Music-only analyzer and safe cable-source audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
