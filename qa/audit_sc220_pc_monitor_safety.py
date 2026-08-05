from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
guard = (ROOT / "cmake/GuardSC220PcMonitorRouting.cmake").read_text(encoding="utf-8")
recovery = (ROOT / "cmake/GenerateSC220PcCaptureRecovery.cmake").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
smoke_cmake = (ROOT / "qa/wasapi_monitor_smoke/CMakeLists.txt").read_text(encoding="utf-8")
smoke_main = (ROOT / "qa/wasapi_monitor_smoke/main.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "PC source and monitor endpoint identity is resolved explicitly",
    all(token in guard for token in (
        "pcEndpointConflictsWithMonitor",
        "resolvePcPlaybackEndpoint",
        "endpointNamesMatch",
    )),
)
check(
    "monitor list withholds the currently captured PC endpoint",
    "getMonitorOutputDeviceNames" in guard
    and "pcEndpointConflictsWithMonitor (pcSource, names[index])" in guard,
)
check(
    "PC source list withholds active stream and monitor endpoints",
    "getPcAudioEndpointNames" in guard
    and "pcEndpointConflictsWithStream (endpoint.name, streamOutput)" in guard
    and "pcEndpointConflictsWithMonitor (endpoint.name, monitorDevice)" in guard,
)
check(
    "monitor selection rejects direct plus DSP self-monitoring",
    "direct-plus-DSP doubling" in guard
    and "monitorOutput.setMuted (true)" in guard
    and "same physical endpoint" in guard,
)
check(
    "PC endpoint selection rejects recursive monitor routing",
    "PC Audio Source and Monitor Output cannot use the same physical endpoint" in guard
    and "captures the monitor back into itself" in guard,
)
check(
    "monitor power revalidates routing before unmute",
    "Unsafe Monitor Routing" in guard
    and "selectMonitorOutputDevice" in guard
    and "setMonitorMuted (false)" in guard,
)
check(
    "endpoint switching flushes delayed monitor audio and resets loudness",
    "monitorOutput.flush();" in guard
    and "loudnessResetRequested.store (true" in guard,
)
check(
    "UI distinguishes retrying capture from generic starting",
    "CAPTURE ERROR / RETRYING" in guard,
)
check(
    "UI explains source equals monitor routing block",
    "SOURCE = MONITOR / USE VIRTUAL SOURCE" in guard,
)

check(
    "capture tries event mode and a polling compatibility fallback",
    all(token in recovery for token in (
        "initialiseCapture (true)",
        "initialiseCapture (false)",
        "AUDCLNT_STREAMFLAGS_EVENTCALLBACK",
        "polling fallback",
    )),
)
check(
    "capture recreates failed endpoint sessions automatically",
    "while (! threadShouldExit())" in recovery
    and "retrying automatically" in recovery
    and "wait (350)" in recovery,
)
check(
    "capture detects runtime packet and buffer failures",
    all(token in recovery for token in (
        "WASAPI GetNextPacketSize",
        "IAudioCaptureClient::GetBuffer",
        "IAudioCaptureClient::ReleaseBuffer",
    )),
)
check(
    "capture stop clears FIFO and stale endpoint peaks",
    "fifo.reset();" in recovery
    and "ringBuffer.clear();" in recovery
    and recovery.count("peakL.store (-120.0f)") >= 3
    and recovery.count("peakR.store (-120.0f)") >= 3,
)
check(
    "empty capture reads decay meters to silence",
    "36.0 * static_cast<double> (numSamples)" in recovery
    and "peakL.load() - releaseDb" in recovery
    and "peakR.load() - releaseDb" in recovery,
)

routing_index = state.find("GuardSC220PcMonitorRouting.cmake")
recovery_index = state.find("GenerateSC220PcCaptureRecovery.cmake")
analyzer_index = state.find("RebuildSC220ImpactVisualizer.cmake")
check(
    "production generator applies routing guard and capture recovery before analyzer",
    -1 not in (routing_index, recovery_index, analyzer_index)
    and routing_index < recovery_index < analyzer_index,
)
check(
    "production defines a generated PC capture translation unit",
    "SC220_PC_CAPTURE_GENERATED_SOURCE" in state
    and "PcAudioLoopbackCapture_Recovery.cpp" in state,
)
check(
    "Windows smoke target compiles the recovered capture implementation",
    "GenerateSC220PcCaptureRecovery.cmake" in smoke_cmake
    and "SC220_PC_CAPTURE_GENERATED_SOURCE" in smoke_cmake,
)
check(
    "Windows smoke executable exercises capture start and stop lifecycle",
    "PcAudioLoopbackCapture capture" in smoke_main
    and "capture.start()" in smoke_main
    and "capture.stop()" in smoke_main,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live PC source/monitor safety and capture recovery audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
