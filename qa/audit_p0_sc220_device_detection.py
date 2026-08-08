from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
engine = (ROOT / "live" / "LiveAudioEngine.cpp").read_text(encoding="utf-8")
ui = (ROOT / "live" / "SC220MainComponent.cpp").read_text(encoding="utf-8")

checks = {
    "engine recognizes field MIXER-01 endpoint": 'normalised.contains ("MIXER-01")' in engine,
    "MIXER-01 has highest device preference": 'return 100;' in engine and 'MIXER-01' in engine,
    "startup scans recording inputs": 'const auto inputs = getInputDeviceNames();' in engine,
    "startup selects preferred SC220 input": 'selectInputDevice (preferredSc220Input)' in engine,
    "stale/default non-SC220 input is replaced": 'if (! looksLikeSc220InputName (activeInput))' in engine,
    "auto-selection is not limited to first-run state": engine.find('const auto activeInput = deviceManager.getAudioDeviceSetup().inputDeviceName;')
        > engine.find('if (savedDeviceState == nullptr)'),
    "UI recognizes Microphone (MIXER-01)": 'normalised.contains ("MIXER-01")' in ui,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")

if failed:
    print("\nP0 SC220 device detection audit failed:")
    for name in failed:
        print(f" - {name}")
    sys.exit(1)

print("\nP0 SC220 device detection audit passed.")
