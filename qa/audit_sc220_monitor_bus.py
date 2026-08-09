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


def contains_all(text: str, *tokens: str) -> bool:
    return all(token in text for token in tokens)


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

# Documentation may mention the retired architecture. Ban actual declarations,
# inheritance and callbacks rather than the English class name in a comment.
check(
    "monitor no longer depends on a second JUCE AudioDeviceManager",
    "juce::AudioDeviceManager deviceManager" not in monitor_h + monitor_cpp
    and "private juce::AudioIODeviceCallback" not in monitor_h
    and "audioDeviceIOCallbackWithContext" not in monitor_cpp
    and "deviceManager.addAudioCallback" not in monitor_cpp,
)
check(
    "monitor enumerates active Windows render endpoints and stable IDs",
    contains_all(
        monitor_cpp,
        "IMMDeviceEnumerator",
        "EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE",
        "PKEY_Device_FriendlyName",
        "device->GetId",
        "GetDefaultAudioEndpoint (eRender, eConsole",
        "SYSTEM DEFAULT",
    ),
)
check(
    "monitor opens an OBS-style shared-mode render client",
    contains_all(
        monitor_cpp,
        "device->Activate (__uuidof (IAudioClient)",
        "nextClient->GetMixFormat",
        "AUDCLNT_SHAREMODE_SHARED",
        "requestedBufferDuration100ns = 10000000",
        "nextClient->GetBufferSize",
        "__uuidof (IAudioRenderClient)",
        "audioClient->GetCurrentPadding",
        "renderClient->GetBuffer",
        "renderClient->ReleaseBuffer",
        "audioClient->Start",
    ),
)
check(
    "monitor uses the endpoint native mix format rather than stream rate",
    contains_all(
        monitor_cpp,
        "mixFormat->nSamplesPerSec",
        "configureNativeFormat",
        "WAVE_FORMAT_IEEE_FLOAT",
        "WAVE_FORMAT_PCM",
        "outputEncoding",
        "WASAPI shared-mode monitor initialization failed",
    )
    and "must support the stream sample rate" not in monitor_cpp,
)
check(
    "monitor transport and resampling memory are fixed before playback",
    contains_all(
        monitor_cpp,
        "fifoCapacitySamples = 384000",
        "resampleScratchCapacity = 65536",
        "juce::AbstractFifo fifo",
        "ringBuffer.setSize",
        "resampleInput.setSize",
        "resampleOutput.setSize",
        "primeTargetSamples",
        "fifo.prepareToWrite",
        "fifo.prepareToRead",
    ),
)
check(
    "monitor performs rate conversion and bounded clock-drift control",
    contains_all(
        monitor_cpp,
        "baseRatio = sourceSampleRate / monitorSampleRate",
        "fillError",
        "correctionPpm",
        "adaptiveRatio",
        "leftInterpolator.process",
        "rightInterpolator.process",
        "currentDriftCorrectionPpm.store",
    ),
)
check(
    "monitor supports float and common PCM endpoint formats",
    contains_all(
        monitor_cpp,
        "SampleEncoding::float32",
        "SampleEncoding::pcm16",
        "SampleEncoding::pcm24",
        "SampleEncoding::pcm32",
        "writeOneSample",
        "writeInterleaved",
        "AUDCLNT_BUFFERFLAGS_SILENT",
    ),
)
check(
    "monitor submission cannot wait on endpoint selection or reset",
    contains_all(
        monitor_cpp,
        "TryAcquireSRWLockExclusive",
        "ReleaseSRWLockExclusive",
        "push (const juce::AudioBuffer<float>& source",
    ),
)

push_match = re.search(
    r"void MonitorOutputClient::push.*?\n}\n\nMonitorOutputClient::Status",
    monitor_cpp,
    flags=re.S,
)
render_match = re.search(
    r"void renderAvailableLocked.*?\n    }\n\n    SRWLOCK playbackLock",
    monitor_cpp,
    flags=re.S,
)
push_path = push_match.group(0) if push_match else ""
render_path = render_match.group(0) if render_match else ""
realtime_path = push_path + render_path
check(
    "monitor steady-state push and render allocate no buffers",
    bool(push_path and render_path)
    and all(token not in realtime_path for token in (
        "setSize", "std::vector", "std::make_unique", "new ",
        "CoTaskMemAlloc", "malloc", "calloc",
    )),
)
check(
    "monitor pre-fills audio before starting the Windows endpoint",
    contains_all(
        render_path,
        "if (! endpointStarted && ready < primeTarget)",
        "ReleaseBuffer",
        "audioClient->Start",
        "endpointStartedFlag.store (true",
        "primed.store (true",
    ),
)
check(
    "monitor automatically recreates an invalidated render client",
    contains_all(
        monitor_cpp,
        "markForReconnectLocked",
        "monitor will reconnect automatically",
        "audioClient == nullptr && impl->selectedDisplayName.isNotEmpty()",
        "now - impl->lastReconnectAttemptMs >= 1000.0",
        "openSelectedEndpointLocked (getGainDb(), true)",
        "reconnectCount.fetch_add",
    ),
)
check(
    "monitor defaults muted and rejects VB-CABLE render endpoints",
    "std::atomic<bool> muted { true }" in monitor_h
    and contains_all(monitor_cpp, "VB-Audio", "VB-CABLE", "CABLE Input", "CABLE Output"),
)
check(
    "monitor status exposes native endpoint and reconnect health",
    contains_all(
        monitor_h + monitor_cpp,
        "endpointStarted",
        "endpointPaddingFrames",
        "sourceSampleRate",
        "resampling",
        "driftCorrectionPpm",
        "reconnects",
        "WASAPI SHARED / OBS-STYLE",
    ),
)
check(
    "engine exposes Program and Music Cue monitor modes",
    contains_all(
        engine_h,
        "enum class MonitorSource",
        "program = 0",
        "musicCue = 1",
        "selectMonitorOutputDevice",
        "setMonitorGainDb",
        "setMonitorMuted",
        "musicCueBuffer",
        "MonitorOutputClient monitorOutput",
    ),
)
check(
    "engine status exposes monitor health",
    contains_all(
        engine_h,
        "monitorRunning",
        "monitorPrimed",
        "monitorBufferedSamples",
        "monitorUnderflows",
        "monitorOverflows",
        "monitorDevice",
        "monitorError",
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
        "Program monitor receives protected master independently of GO LIVE",
        -1 not in (limiter, monitor_publish, stream_gate)
        and limiter < monitor_publish < stream_gate,
    )
    check(
        "monitor follows stream lifecycle without sharing its endpoint",
        contains_all(
            generated_engine,
            "monitorOutput.initialise();",
            "monitorOutput.prepareSource (sampleRate, blockSize);",
            "monitorOutput.flush();",
            "monitorOutput.shutdown();",
        ),
    )

check(
    "UI declares endpoint power and Program/Cue controls",
    contains_all(
        ui_h,
        "monitorPowerButton",
        "monitorOutputBox",
        "programMonitorButton",
        "musicCueButton",
        "refreshMonitorDevices",
    ),
)
check(
    "UI generator wires physical monitoring controls",
    contains_all(
        ui_gen,
        "selectMonitorOutputDevice",
        "setMonitorSource",
        "setMonitorMuted",
        "setMonitorGainDb",
        "SELECT MONITOR",
        "PHYSICAL OUTPUT ACTIVE",
    ),
)
check(
    "monitor and stream selectors remain separate",
    ui_gen.count("sourceSelectorY, 164, 34") >= 2
    and "monitorOutputBox.setBounds" in ui_gen
    and "outputBox.setBounds" in ui_gen,
)
check(
    "monitor selection gain mute source and Music Signature persist",
    contains_all(
        state_gen,
        '"monitorDevice"',
        '"monitorGainDb"',
        '"monitorMuted"',
        '"monitorSource"',
        '"musicPresetIndex"',
        '"musicEnhance"',
        '"musicStereoMagic"',
    ),
)
check(
    "normal build substitutes monitor-enabled generated sources",
    contains_all(
        aligned_gen,
        "GenerateSC220MonitorEngine.cmake",
        "GenerateSC220MonitorUi.cmake",
        "GenerateSC220BroadcastState.cmake",
        "HEADER_FILE_ONLY TRUE",
        "MonitorOutputClient.cpp",
        "SC220_APP_GENERATED_SOURCE",
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live OBS-style native WASAPI Monitor Bus audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
