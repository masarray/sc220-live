from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
generated_root = ROOT / "generated" / "sc220-monitor"
engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
stabilizer_path = ROOT / "cmake" / "StabilizeSC220PcMusicMode.cmake"
snapshot_path = ROOT / "live" / "ProgrammeSpectrumSnapshot.h"
state_path = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"

engine = engine_path.read_text(encoding="utf-8") if engine_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""
stabilizer = stabilizer_path.read_text(encoding="utf-8")
snapshot = snapshot_path.read_text(encoding="utf-8")
state = state_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "PC Music stabilizer runs before snapshot visualizer",
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
        "looksLikeSc220Endpoint",
        "for (const auto& name : getInputDeviceNames())",
        "selectInputDevice (name)",
    )),
)
check(
    "zero-input callback still processes PC Music",
    all(token in engine for token in (
        "if (numInputChannels > 0",
        "voiceBlock.clear",
        "pcLoopback.read (musicBlock, numSamples)",
        "musicEnhancerBridge.process (musicBlock, musicMidi)",
        "masterLimiter.process (masterContext)",
    )),
)
check(
    "safe VB-CABLE families remain distinguishable",
    all(token in engine for token in (
        'return "VBCABLEA"',
        'return "VBCABLEB"',
        'return "VBCABLEC"',
        'return "VBCABLED"',
        'return "VBCABLE"',
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
    "post-limiter snapshot publication follows PC Music processing",
    engine.find("musicEnhancerBridge.process (musicBlock, musicMidi)")
    < engine.find("masterLimiter.process (masterContext)")
    < engine.find("programmeSpectrumSnapshot.push"),
)
check(
    "visible spectrum consumes only the post-limiter snapshot",
    all(token in ui for token in (
        "updateFromSnapshot",
        "programmeSpectrumSnapshot.consumeLatest",
        "POST-LIMITER SNAPSHOT",
        "PROGRAM SPECTRUM",
    )),
)
check(
    "snapshot is complete-frame and allocation-free",
    all(token in snapshot for token in (
        "frameSize = 512",
        "std::array<Bank, 2>",
        "std::memcpy",
        "publishedGeneration",
        "sequenceBefore",
        "sequenceAfter",
    ))
    and "new " not in snapshot
    and "mutex" not in snapshot.lower(),
)
check(
    "old analyzer sources cannot mask PC-only operation",
    all(token not in ui for token in (
        "programmeTap.consumeLatest",
        "musicTap.consumeLatest",
        "voiceTap.consumeLatest",
        "PcAudioLoopbackCapture::consumeLatestVisual",
        "MUSIC RECOVERY",
        "VOICE RECOVERY",
        "PC CAPTURE",
        "NO FRAMES",
    )),
)
check(
    "spectralizer remains lightweight and UI-thread only",
    "fftOrder = 9" in ui
    and "bandCount = 24" in ui
    and "performFrequencyOnlyForwardTransform" in ui
    and "performFrequencyOnlyForwardTransform" not in engine,
)
check(
    "generated final sources exist",
    engine_path.exists() and ui_path.exists() and snapshot_path.exists(),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live PC Music-only and post-limiter snapshot audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
