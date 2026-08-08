from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
clock_fix = (ROOT / "cmake" / "FixSC220RuntimeClockStatus.cmake").read_text(encoding="utf-8")
chain = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
ui = (ROOT / "live" / "SC220MainComponent.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "actual sample rate is published on audio-device start",
    "currentSampleRate.store (sampleRate)" in clock_fix
    and "device->getCurrentSampleRate()" in clock_fix,
)
check(
    "actual buffer size is published on audio-device start",
    "currentBufferSize.store (blockSize)" in clock_fix
    and "device->getCurrentBufferSizeSamples()" in clock_fix,
)
check(
    "status refresh follows the actually open device",
    "deviceManager.getCurrentAudioDevice()" in clock_fix
    and "currentSampleRate.store (device->getCurrentSampleRate())" in clock_fix
    and "currentBufferSize.store" in clock_fix,
)
check(
    "runtime clock fix runs before final analyzer authority",
    'include("${CMAKE_CURRENT_LIST_DIR}/FixSC220RuntimeClockStatus.cmake")' in chain
    and chain.find("FixSC220RuntimeClockStatus.cmake")
        < chain.find("RestoreSC220ProgrammeAnalyzerRoute.cmake"),
)
check(
    "FFT visualizer rejects invalid zero-Hz status so clock publication is required",
    "if (count <= 0 || sampleRate <= 0.0)" in ui,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live runtime clock/status audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
