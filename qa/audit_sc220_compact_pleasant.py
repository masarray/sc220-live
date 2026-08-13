from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

header = (ROOT / "live/SmartPleasantPresence.h").read_text(encoding="utf-8")
source = (ROOT / "live/SmartPleasantPresence.cpp").read_text(encoding="utf-8")
engine_h = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
engine_cpp = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
compact = (ROOT / "cmake/SC220P7CompactPleasant.cmake").read_text(encoding="utf-8")

generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
generated_app_path = generated_root / "LiveApplication_BroadcastState.cpp"
generated_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else ""
generated_app = generated_app_path.read_text(encoding="utf-8") if generated_app_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "pleasant engine uses separate mid-high and de-ess bands",
    all(token in header + source for token in (
        "midHighBands", "sibilanceBands", "3900.0f", "7350.0f",
        "midHighReductionDb", "deEssReductionDb",
    )),
)
check(
    "detectors are stereo-linked and zero-latency block processors",
    all(token in source for token in (
        "broadPeak = juce::jmax", "midHighPeak = juce::jmax",
        "sibilancePeak = juce::jmax", "writePointers[index][sample]",
    ))
    and ".setSize (" not in source
    and "new " not in source,
)
check(
    "reduction stays bounded to preserve standout articulation",
    "juce::jlimit (0.0f, 1.70f" in source
    and "juce::jlimit (0.0f, 4.25f" in source
    and "softGate" in source,
)
check(
    "voice lane applies pleasant control after ASK-P enhancement",
    "voiceEnhancerBridge.process (voiceBlock, voiceMidi);" in engine_cpp
    and "pleasantPresence.process (voiceBlock);" in engine_cpp
    and engine_cpp.index("voiceEnhancerBridge.process (voiceBlock, voiceMidi);")
        < engine_cpp.index("pleasantPresence.process (voiceBlock);"),
)
check(
    "protection authority follows Enhance and Smart Treble",
    all(token in engine_cpp for token in (
        "voiceEnhancerBridge.enhance()", "voiceEnhancerBridge.smartTreble()",
        "pleasantPresence.setStrength", "0.58f", "1.0f",
    )),
)
check(
    "engine exposes independent reduction telemetry",
    all(token in engine_h + engine_cpp for token in (
        "pleasantMidHighReductionDb", "deEssReductionDb",
        "pleasantPresence.snapshot()",
    )),
)
check(
    "compiled regression audit covers body mid-high sibilance and stereo",
    all(token in cmake for token in (
        "SC220PleasantPresenceAudit", "qa/SmartPleasantPresenceAudit.cpp",
        "live/SmartPleasantPresence.cpp",
    )),
)
check(
    "compact pass precedes the final premium alignment authority",
    "SC220P7CompactPleasant.cmake" in state
    and "SC220P8PremiumAlignment.cmake" in state
    and state.rfind("SC220P8PremiumAlignment.cmake")
        > state.rfind("SC220P7CompactPleasant.cmake"),
)
check(
    "compact layout specifies smaller default and safe resize limits",
    all(token in compact for token in (
        "centreWithSize (1280, 720)", "setResizeLimits (1080, 680, 1760, 1080)",
        "headerArea = area.removeFromTop (58)",
        "footerArea = area.removeFromBottom (46)",
        "sharedFaderWidth = 60", "juce::jmax (158",
    )),
)
check(
    "generated console contains compact final geometry",
    generated_ui_path.exists() and generated_app_path.exists()
    and all(token in generated_ui + generated_app for token in (
        "centreWithSize (1280, 720)", "setResizeLimits (1080, 680, 1760, 1080)",
        "headerArea = area.removeFromTop (58)", "masterColumnGap = 14",
        "spectrumDisplay->getBottom() + 42", "sharedFaderWidth = 60",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 compact UX and smart pleasant-presence contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
