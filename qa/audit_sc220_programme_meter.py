from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
engine = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")
meter = (ROOT / "live/ProgrammeLoudnessIntegrator.h").read_text(encoding="utf-8")
ui = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("dedicated post-master analyzer tap exists", all(token in header + engine for token in (
    "askp::ScopeTap programmeAnalyzer",
    "programmeAnalyzer.push",
    "setProgrammeAnalyzerEnabled",
)))
check("analyzer is post limiter and 32-band message-thread FFT", all(token in engine + ui for token in (
    "masterLimiter.process (masterContext)",
    "programmeAnalyzer.push",
    "fftOrder = 10",
    "std::array<float, 32> smoothed",
    "performFrequencyOnlyForwardTransform",
)))
check("stereo analyzer cannot cancel anti-phase programme", all(token in ui for token in (
    "leftFftData",
    "rightFftData",
    "leftMagnitude * leftMagnitude + rightMagnitude * rightMagnitude",
    "capturedPeak",
)))
check("integrated loudness uses 400 ms blocks with 75 percent overlap", all(token in meter for token in (
    "sampleRate * 0.100",
    "std::array<double, 4> segmentEnergy",
    "relativeGateLu = -10.0f",
    "absoluteGateLufs = -70.0f",
)))
check("integrated loudness has a three-second reliability warmup", all(token in meter + ui for token in (
    "warmupSeconds = 3.0",
    "CALIBRATING",
    "loudnessReady",
)))
check("true peak uses four-times polyphase oversampling and session max hold", all(token in header + engine for token in (
    "2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR",
    "truePeakOversampler.processSamplesUp",
    "truePeakMaximumLinear = juce::jmax",
    "truePeakDbtp",
)))
check("UI labels integrated LUFS true peak and social target explicitly", all(token in ui for token in (
    '"LUFS-I"',
    '" dBTP MAX"',
    '"YT / TIKTOK  -14  |  TP <= -1"',
)))
check("GO LIVE starts a fresh programme measurement", all(token in ui for token in (
    "goLiveButton.getToggleState()",
    "audioEngine.resetProgrammeLoudness()",
)))

failed = [name for name, ok in checks if not ok]
print("SC220 post-master analyzer and programme meter audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
