from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
source = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("voice Signature bridge exists", "EnhancerEngineBridge voiceEnhancerBridge" in header)
check("music Signature bridge exists", "EnhancerEngineBridge musicEnhancerBridge" in header)
check("master safety limiter exists", "juce::dsp::Limiter<float> masterLimiter" in header)
check("music preset API exists", all(token in header for token in (
    "setMusicPresetIndex", "getMusicPresetIndex", "getMusicPresetNames")))
check("music macro API exists", all(token in header for token in (
    "setMusicEnhance", "setMusicSmartBass", "setMusicSmartTreble",
    "setMusicVocalBody", "setMusicStereoMagic")))
check("SC220 trim is lane-local", "const float inputTargetDb = inputTrimBypassed.load() ? 0.0f : inputTrimDb.load();" in source)
check("old shared input trim split removed", "inputTrimDb.load() + 12.0f" not in source)
check("dual mono no longer discards channel 2", "dualMonoInput ? 0" not in source)
check("voice is enhanced before voice fader", source.find("voiceEnhancerBridge.process") < source.find("sc220MixGainLinear.setTargetValue"))
check("music is enhanced before ducking", source.find("musicEnhancerBridge.process") < source.find("const float targetDuck"))
check("music fader is after music enhancement", source.find("musicEnhancerBridge.process") < source.find("pcGainLinear.setTargetValue"))
check("lanes meet after independent processing", source.find("voiceBlock.addFrom") > source.find("musicEnhancerBridge.process"))
check("master trim is after lane summing", source.find("const float outputTargetDb") > source.find("voiceBlock.addFrom"))
check("master limiter is after master trim", source.find("masterLimiter.process") > source.find("const float outputTargetDb"))
check("separate gain-reduction telemetry exists", all(token in header + source for token in (
    "voiceGainReductionDb", "musicGainReductionDb", "masterGainReductionDb")))
check("separate DSP latency telemetry exists", all(token in header + source for token in (
    "voiceLatencySamples", "musicDspLatencySamples")))

failed = [name for name, ok in checks if not ok]
print("SC220 Live dual-source broadcast bus audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
