from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/PcAudioLoopbackCapture.h").read_text(encoding="utf-8")
generator = (ROOT / "cmake/GenerateSC220DirectPcCaptureVisualizer.cmake").read_text(
    encoding="utf-8"
)
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")

generated_root = ROOT / "generated" / "sc220-monitor"
if not generated_root.exists():
    generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"

capture_path = generated_root / "PcAudioLoopbackCapture_Visual.cpp"
ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
capture = capture_path.read_text(encoding="utf-8") if capture_path.exists() else ""
ui = ui_path.read_text(encoding="utf-8") if ui_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "direct capture visual API is public and independent of LiveAudioEngine",
    all(token in header for token in (
        "static int consumeLatestVisual",
        "static std::uint64_t capturedVisualSamples",
        "Direct WASAPI visual authority",
    )),
)
check(
    "direct capture generator runs after programme spectralizer generation",
    'include("${CMAKE_CURRENT_LIST_DIR}/GenerateSC220DirectPcCaptureVisualizer.cmake")'
    in state
    and state.find("RebuildSC220ImpactVisualizer.cmake")
    < state.find("GenerateSC220DirectPcCaptureVisualizer.cmake"),
)
check(
    "normal build replaces base capture source with generated authority",
    all(token in generator for token in (
        'set_source_files_properties("${SC220_PC_CAPTURE_SOURCE}"',
        "PROPERTIES HEADER_FILE_ONLY TRUE",
        'target_sources(SC220Live PRIVATE "${SC220_PC_CAPTURE_GENERATED_SOURCE}")',
    )),
)
check(
    "generated direct capture and final UI sources exist",
    capture_path.exists() and ui_path.exists(),
)
check(
    "visual tap is a bounded SPSC queue",
    all(token in capture for token in (
        "class DirectPcCaptureVisualTap final",
        "queueSize = 16384",
        "producerWrite",
        "consumerRead",
        "writePublished",
        "readPublished",
        "std::memory_order_release",
        "std::memory_order_acquire",
    )),
)
check(
    "capture producer never waits or allocates for visualization",
    "visual data drops; the capture thread never waits" in capture
    and "directPcCaptureVisualTap.push (left, right, numSamples);" in capture,
)
publish_position = capture.find("directPcCaptureVisualTap.push (left, right, numSamples);")
fifo_position = capture.find("const int writable = juce::jmin (numSamples, fifo.getFreeSpace());")
check(
    "WASAPI samples publish before the DSP FIFO can reject them",
    publish_position >= 0 and fifo_position >= 0 and publish_position < fifo_position,
)
check(
    "UI consumes the newest bounded tail",
    all(token in capture for token in (
        "if (available > maximumSamples)",
        "read = (published - maximumSamples) & queueMask",
        "consumerRead = published",
        "readPublished.store (published",
    )),
)
check(
    "source priority is post-master then Music then direct PC then Voice",
    ui.find("programmeTap.consumeLatest")
    < ui.find("musicTap.consumeLatest")
    < ui.find("PcAudioLoopbackCapture::consumeLatestVisual")
    < ui.find("voiceTap.consumeLatest"),
)
check(
    "direct PC capture is visible as a real spectralizer source",
    all(token in ui for token in (
        "CaptureSource::pcCapture",
        'sourceText = "PC CAPTURE"',
        "PROGRAM SPECTRUM",
        "performFrequencyOnlyForwardTransform",
    )),
)
check(
    "WASAPI startup failures are exposed instead of permanent STARTING",
    all(token in ui for token in (
        '"CAPTURE ERROR"',
        "lastStatus.pcAudioError.isNotEmpty()",
        "g.drawFittedText (lastStatus.pcAudioError",
    )),
)
check(
    "visual FFT remains outside the capture and audio engine sources",
    "performFrequencyOnlyForwardTransform" not in capture
    and "multiplyWithWindowingTable" not in capture,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live direct WASAPI capture spectralizer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
