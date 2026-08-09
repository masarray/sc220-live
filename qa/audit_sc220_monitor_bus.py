from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
engine_h = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
engine_cpp = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")
monitor_h = (ROOT / "live/MonitorOutputClient.h").read_text(encoding="utf-8")
monitor_cpp = (ROOT / "live/MonitorOutputClient.cpp").read_text(encoding="utf-8")
ui_h = (ROOT / "live/SC220MainComponent.h").read_text(encoding="utf-8")
ui_cpp = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
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

# UI monitor generator consumes the already aligned source. Its specific anchors
# are audited directly here; the aligned generator has a separate exact audit.
check(
    "monitor client owns a second output-only AudioDeviceManager",
    "juce::AudioDeviceManager deviceManager" in monitor_h
    and "deviceManager.initialise (0, 0" in monitor_cpp
    and "deviceManager.addAudioCallback (this)" in monitor_cpp,
)
check(
    "monitor transport is bounded and prefilled",
    all(
        token in monitor_h + monitor_cpp
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
    "monitor endpoint enforces nominal stream sample rate",
    all(
        token in monitor_cpp
        for token in (
            "setup.sampleRate = preferredSampleRate",
            "std::abs (actualRate - expectedRate) > 1.0",
            "must support the stream sample rate",
        )
    ),
)

callback_match = re.search(
    r"void MonitorOutputClient::audioDeviceIOCallbackWithContext.*?\n}\n\n"
    r"void MonitorOutputClient::audioDeviceAboutToStart",
    monitor_cpp,
    flags=re.S,
)
callback = callback_match.group(0) if callback_match else ""
check(
    "monitor callback is lock-free and allocation-free",
    bool(callback)
    and "ScopedLock" not in callback
    and "setSize" not in callback
    and "new " not in callback
    and "std::vector" not in callback,
)
check(
    "monitor callback uses a deterministic smoothed gain ramp",
    all(token in callback for token in ("gainLinear", "gainStep", "output[sample] *= gain")),
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
    cue_copy = generated_engine.find("musicCueBlock.copyFrom")
    cue_gain = generated_engine.find("musicCueBlock.applyGainRamp")
    duck_gain = generated_engine.find("musicBlock.applyGainRamp", cue_gain)
    limiter = generated_engine.find("masterLimiter.process (masterContext);")
    monitor_publish = generated_engine.find("monitorOutput.push")
    stream_gate = generated_engine.find("if (! outputMuted.load())", monitor_publish)
    check(
        "Music Cue is enhanced and fader-controlled before ducking",
        -1 not in (music_process, cue_copy, cue_gain, duck_gain)
        and music_process < cue_copy < cue_gain < duck_gain,
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
