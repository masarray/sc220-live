from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
generated_root = ROOT / "generated" / "sc220-monitor"
engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
stabilizer_path = ROOT / "cmake" / "StabilizeSC220PcMusicMode.cmake"
telemetry_path = ROOT / "live" / "ProgrammeBandTelemetry.h"
state_path = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"

engine = engine_path.read_text(encoding="utf-8") if engine_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""
stabilizer = stabilizer_path.read_text(encoding="utf-8")
telemetry = telemetry_path.read_text(encoding="utf-8")
state = state_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "PC Music stabilizer runs before atomic band visualizer",
    'include("${CMAKE_CURRENT_LIST_DIR}/StabilizeSC220PcMusicMode.cmake")' in state
    and 'include("${CMAKE_CURRENT_LIST_DIR}/RebuildSC220ImpactVisualizer.cmake")' in state
    and state.find("StabilizeSC220PcMusicMode.cmake")
    < state.find("RebuildSC220ImpactVisualizer.cmake"),
)
check(
    "programme graph opens output-first without SC220",
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
    "SC220 remains optional and auto-selectable",
    all(token in engine for token in (
        "looksLikeSc220Endpoint", "for (const auto& name : getInputDeviceNames())",
        "selectInputDevice (name)",
    )),
)
check(
    "zero-input callback still processes PC Music",
    all(token in engine for token in (
        "if (numInputChannels > 0", "voiceBlock.clear",
        "pcLoopback.read (musicBlock, numSamples)",
        "musicEnhancerBridge.process (musicBlock, musicMidi)",
        "masterLimiter.process (masterContext)",
    )),
)
check(
    "safe VB-CABLE families remain distinguishable",
    all(token in engine for token in (
        'return "VBCABLEA"', 'return "VBCABLEB"', 'return "VBCABLEC"',
        'return "VBCABLED"', 'return "VBCABLE"',
    )),
)
check(
    "same endpoint or cable family remains blocked",
    all(token in engine for token in (
        "PcAudioLoopbackCapture::endpointNamesMatch (source, streamOutput)",
        "sourceCable.isNotEmpty() && sourceCable == streamCable",
        "PC Audio cannot capture the same cable used by Stream Output",
    )),
)
check(
    "post-limiter band measurement follows PC Music processing",
    engine.find("musicEnhancerBridge.process (musicBlock, musicMidi)")
    < engine.find("masterLimiter.process (masterContext)")
    < engine.find("programmeBandTelemetry.process"),
)
check(
    "visible analyzer consumes atomic post-limiter bands",
    all(token in ui for token in (
        "updateFromTelemetry", "programmeBandTelemetry.read",
        "POST-LIMITER BANDS", "PROGRAM ANALYZER",
    )),
)
check(
    "band telemetry is fixed-size and allocation-free",
    all(token in telemetry for token in (
        "bandCount = 14", "std::array<Coefficients, bandCount>",
        "std::array<std::array<State, bandCount>, 2>",
        "std::array<std::atomic<float>, bandCount>",
        "publishedGeneration.fetch_add",
    ))
    and "std::vector" not in telemetry
    and "new " not in telemetry
    and "mutex" not in telemetry.lower(),
)
check(
    "old sample transport sources cannot mask PC-only operation",
    all(token not in ui + engine for token in (
        "ProgrammeSpectrumSnapshot", "programmeSpectrumSnapshot",
        "programmeTap.consumeLatest", "musicTap.consumeLatest",
        "voiceTap.consumeLatest", "PcAudioLoopbackCapture::consumeLatestVisual",
        "MUSIC RECOVERY", "VOICE RECOVERY", "PC CAPTURE", "NO FRAMES",
    )),
)
check(
    "visualizer has no FFT or sample queue",
    "performFrequencyOnlyForwardTransform" not in ui + engine + telemetry
    and "multiplyWithWindowingTable" not in ui + engine + telemetry
    and "consumeLatest" not in ui,
)
check(
    "generated final sources and telemetry exist",
    engine_path.exists() and ui_path.exists() and telemetry_path.exists(),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live PC Music-only and atomic post-limiter band audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
