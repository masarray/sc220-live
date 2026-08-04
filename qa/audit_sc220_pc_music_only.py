from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
generated_root = ROOT / "generated" / "sc220-monitor"
engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
stabilizer_path = ROOT / "cmake" / "StabilizeSC220PcMusicMode.cmake"
spectralizer_path = ROOT / "cmake" / "RebuildSC220ImpactVisualizer.cmake"
state_path = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"

engine = engine_path.read_text(encoding="utf-8") if engine_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""
stabilizer = stabilizer_path.read_text(encoding="utf-8")
spectralizer = spectralizer_path.read_text(encoding="utf-8")
stabilizer_words = " ".join(stabilizer.lower().replace("#", " ").split())
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
    "spectralizer rebuild runs after PC Music stabilizer",
    'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and state.find("StabilizeSC220PcMusicMode.cmake")
    < state.find("RebuildSC220ImpactVisualizer.cmake"),
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
    "programme ScopeTap is permanently armed and feeds the visible spectrum",
    "programmeAnalyzer.setEnabled (true)" in engine
    and "audioEngine.programmeAnalyzerTap()" in ui
    and "spectrumDisplay->updateFromTap" in ui
    and "audioEngine.setProgrammeAnalyzerEnabled (false);" not in ui
    and "audioEngine.setProgrammeAnalyzerEnabled (active);" not in ui,
)
check(
    "PC-only spectralizer analyses protected post-master stereo samples",
    all(token in ui for token in (
        "programmeTap.consumeLatest",
        "leftFftData",
        "rightFftData",
        "stereoPower += 0.5f",
        "PROGRAM SPECTRUM",
    )),
)
check(
    "frequency bars remain fixed from bass to treble",
    all(token in ui for token in (
        "lowerFrequency = 35.0f",
        "bandCount = 24",
        '"40"',
        '"100"',
        '"1k"',
        '"12k"',
    )),
)
check(
    "spectralizer is lightweight and UI-thread only",
    "fftOrder = 9" in ui
    and "performFrequencyOnlyForwardTransform" in ui
    and "performFrequencyOnlyForwardTransform" not in engine
    and "spectrumDivider" not in ui,
)
check(
    "spectralizer has fast attack smooth release and per-band peak hold",
    all(token in ui for token in (
        "* 0.82f",
        "levels[band] * 0.88f",
        "peakHoldFrames[band] = 15",
        "500 ms HOLD",
    )),
)
check(
    "obsolete left-to-right history meter is removed",
    "PROGRAM IMPACT" not in ui
    and "PC INPUT IMPACT" not in ui
    and '"OLDER"' not in ui
    and '"NOW"' not in ui
    and "historySize = 32" not in ui,
)
check(
    "generated final sources exist",
    engine_path.exists() and ui_path.exists(),
)
check(
    "stabilizer documents safe virtual source and PC-only intent",
    "different cable" in stabilizer_words
    and "without an sc220 input device" in stabilizer_words,
)
check(
    "spectralizer generator documents audio-thread isolation",
    "512-point stereo fft" in spectralizer.lower()
    and "message thread" in spectralizer.lower()
    and "never in the audio callback" in spectralizer.lower(),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live PC Music-only routing and lightweight OBS spectralizer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
