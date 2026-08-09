from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MONITOR_H = (ROOT / "live" / "MonitorOutputClient.h").read_text(encoding="utf-8")
MONITOR_CPP = (ROOT / "live" / "MonitorOutputClient.cpp").read_text(encoding="utf-8")
ANALYZER_PASS = (ROOT / "cmake" / "RestoreSC220ProgrammeAnalyzerRoute.cmake").read_text(encoding="utf-8")
CHAIN = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
ENGINE = (ROOT / "live" / "LiveAudioEngine.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "monitor uses native WASAPI implementation rather than a second JUCE AudioDeviceManager",
    "std::unique_ptr<Impl> impl" in MONITOR_H
    and "juce::AudioDeviceManager deviceManager" not in MONITOR_H,
)
check(
    "monitor opens endpoint native mix format in shared mode",
    "IAudioRenderClient" in MONITOR_CPP
    and "GetMixFormat" in MONITOR_CPP
    and "AUDCLNT_SHAREMODE_SHARED" in MONITOR_CPP,
)
check(
    "monitor supports source-to-endpoint sample-rate conversion",
    "resampling" in MONITOR_H
    and "rateConversionActive" in MONITOR_CPP
    and "driftCorrection" in MONITOR_CPP,
)
check(
    "obsolete equal-sample-rate rejection is absent",
    "Monitor endpoint must support the stream sample rate" not in MONITOR_CPP,
)
check(
    "Windows native monitor dependencies remain linked",
    "target_link_libraries(SC220Live PRIVATE ole32 avrt)" in CMAKE,
)
check(
    "engine publishes the exact post-limiter programme bus",
    "masterLimiter.process" in ENGINE
    and "programmeAnalyzer.push (voiceBlock.getReadPointer (0)" in ENGINE,
)
check(
    "generated UI is forced onto programmeAnalyzerTap",
    "audioEngine.programmeAnalyzerTap()" in ANALYZER_PASS
    and "getVoiceEnhancerBridge().analysisTap()" in ANALYZER_PASS
    and "generated UI still reads the disabled Voice Enhancer visual tap" in ANALYZER_PASS,
)
check(
    "analyzer recovery pass is final in the generated UI chain",
    'include("${CMAKE_CURRENT_LIST_DIR}/RestoreSC220ProgrammeAnalyzerRoute.cmake")' in CHAIN
    and CHAIN.rfind("RestoreSC220ProgrammeAnalyzerRoute.cmake")
        > CHAIN.rfind("IntegrateSC220StaticNoiseClean.cmake"),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live runtime regression guard")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
