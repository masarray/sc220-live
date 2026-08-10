from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
engine_h = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
engine_cpp = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")
monitor_h = (ROOT / "live/MonitorOutputClient.h").read_text(encoding="utf-8")
monitor_cpp = (ROOT / "live/MonitorOutputClient.cpp").read_text(encoding="utf-8")
ui_h = (ROOT / "live/SC220MainComponent.h").read_text(encoding="utf-8")
engine_gen = (ROOT / "cmake/GenerateSC220MonitorEngine.cmake").read_text(encoding="utf-8")
ui_gen = (ROOT / "cmake/GenerateSC220MonitorUi.cmake").read_text(encoding="utf-8")
state_gen = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
aligned_gen = (ROOT / "cmake/GenerateSC220AlignedUi.cmake").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


def apply_generator(source: str, generator: str) -> str:
    pairs = re.findall(
        r"set\((_[A-Za-z0-9_]+)_anchor \[=\[\n(.*?)\n\]=\]\)\s*"
        r"set\(\1_replacement \[=\[\n(.*?)\n\]=\]\)",
        generator,
        flags=re.S,
    )
    result = source
    for name, anchor, replacement in pairs:
        count = result.count(anchor)
        if count != 1:
            raise AssertionError(f"{name}: expected one anchor, found {count}")
        result = result.replace(anchor, replacement, 1)
    return result


try:
    generated_engine = apply_generator(engine_cpp, engine_gen)
except AssertionError as exc:
    generated_engine = ""
    checks.append((f"engine generator applies exactly once: {exc}", False))
else:
    check("engine generator applies exactly once", True)

check(
    "monitor uses independent native WASAPI output instead of a second JUCE device manager",
    "juce::AudioDeviceManager" not in monitor_h + monitor_cpp
    and all(
        token in monitor_cpp
        for token in (
            "IAudioClient",
            "IAudioRenderClient",
            "WASAPI SHARED / OBS-STYLE",
            "TryAcquireSRWLockExclusive",
        )
    ),
)
check(
    "monitor transport is bounded and prefilled",
    all(
        token in monitor_cpp
        for token in (
            "juce::AbstractFifo fifo",
            "fifoCapacitySamples",
            "primeTargetSamples",
            "fifo.prepareToWrite",
            "fifo.prepareToRead",
            "primed.store",
        )
    ),
)
check(
    "monitor defaults muted and rejects virtual cable",
    "std::atomic<bool> muted { true }" in monitor_h
    and all(
        token in monitor_cpp
        for token in ("VB-Audio", "VB-CABLE", "CABLE Input", "CABLE Output")
    ),
)
check(
    "monitor push enqueues one selected source without a second programme mix",
    "impl->enqueueLocked (source, numSamples);" in monitor_cpp
    and "impl->renderAvailableLocked (getGainDb(), isMuted());" in monitor_cpp,
)

check(
    "engine exposes Program and Music Cue monitor modes",
    all(
        token in engine_h
        for token in (
            "enum class MonitorSource",
            "program = 0",
            "musicCue = 1",
            "selectMonitorOutputDevice",
            "setMonitorGainDb",
            "setMonitorMuted",
            "musicCueBuffer",
            "MonitorOutputClient monitorOutput",
        )
    ),
)
check(
    "engine status exposes monitor health telemetry",
    all(
        token in engine_h
        for token in (
            "monitorRunning",
            "monitorPrimed",
            "monitorBufferedSamples",
            "monitorUnderflows",
            "monitorOverflows",
            "monitorDevice",
            "monitorError",
        )
    ),
)

if generated_engine:
    music_process = generated_engine.find("musicEnhancerBridge.process (musicBlock, musicMidi);")
    duck_gain = generated_engine.find("musicBlock.applyGainRamp", music_process)
    cue_copy = generated_engine.find("musicCueBlock.copyFrom", duck_gain)
    cue_gain = generated_engine.find("musicCueBlock.applyGainRamp")
    detector_control = generated_engine.find("const float targetDuck")
    limiter = generated_engine.find("masterLimiter.process (masterContext);")
    monitor_publish = generated_engine.find("monitorOutput.push")
    stream_gate = generated_engine.find("if (! outputMuted.load())", monitor_publish)

    check(
        "PC monitor copies the already-ducked enhanced music lane",
        -1 not in (music_process, detector_control, duck_gain, cue_copy)
        and music_process < detector_control < duck_gain < cue_copy,
    )
    check(
        "Music Cue has no second duck or fader gain stage",
        cue_gain == -1
        and generated_engine.count("musicBlock.applyGainRamp") == 1,
    )
    check(
        "post-duck monitor copy is plain sample copy only",
        "Structural anti-double-duck guarantee" in generated_engine
        and "musicCueBlock.copyFrom" in generated_engine,
    )
    check(
        "monitor publishes exactly one selected source per callback",
        generated_engine.count("monitorOutput.push (musicCueBlock, numSamples);") == 1
        and generated_engine.count("monitorOutput.push (voiceBlock, numSamples);") == 1
        and "if (monitorSource.load() == MonitorSource::musicCue)" in generated_engine,
    )
    check(
        "Program monitor publishes the protected master independently of GO LIVE",
        -1 not in (limiter, monitor_publish, stream_gate)
        and limiter < monitor_publish < stream_gate,
    )
    check(
        "monitor FIFO is prepared and flushed with stream lifecycle",
        all(
            token in generated_engine
            for token in (
                "monitorOutput.initialise();",
                "monitorOutput.prepareSource (sampleRate, blockSize);",
                "monitorOutput.flush();",
                "monitorOutput.shutdown();",
            )
        ),
    )

check(
    "UI declares physical output, power and Program/Cue controls",
    all(
        token in ui_h
        for token in (
            "monitorPowerButton",
            "monitorOutputBox",
            "programMonitorButton",
            "musicCueButton",
            "refreshMonitorDevices",
        )
    ),
)
check(
    "UI generator wires physical monitor controls",
    all(
        token in ui_gen
        for token in (
            "selectMonitorOutputDevice",
            "setMonitorSource",
            "setMonitorMuted",
            "setMonitorGainDb",
            "SELECT MONITOR",
            "PHYSICAL OUTPUT ACTIVE",
        )
    ),
)
check(
    "monitor and stream selectors share the console source row",
    ui_gen.count("sourceSelectorY, 164, 34") >= 2
    and "monitorOutputBox.setBounds" in ui_gen
    and "outputBox.setBounds" in ui_gen,
)
check(
    "monitor and Music Signature state persists",
    all(
        token in state_gen
        for token in (
            '"monitorDevice"',
            '"monitorGainDb"',
            '"monitorMuted"',
            '"monitorSource"',
            '"musicPresetIndex"',
            '"musicEnhance"',
            '"musicStereoMagic"',
        )
    ),
)
check(
    "normal build substitutes generated engine UI and application sources",
    all(
        token in aligned_gen
        for token in (
            "GenerateSC220MonitorEngine.cmake",
            "GenerateSC220MonitorUi.cmake",
            "GenerateSC220BroadcastState.cmake",
            "HEADER_FILE_ONLY TRUE",
            "MonitorOutputClient.cpp",
            "SC220_APP_GENERATED_SOURCE",
        )
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live independent Monitor Bus contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
