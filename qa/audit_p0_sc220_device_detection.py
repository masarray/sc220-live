from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
engine = (ROOT / "live" / "LiveAudioEngine.cpp").read_text(encoding="utf-8")
ui = (ROOT / "live" / "SC220MainComponent.cpp").read_text(encoding="utf-8")
ui_header = (ROOT / "live" / "SC220MainComponent.h").read_text(encoding="utf-8")
watcher = (ROOT / "live" / "SC220DeviceWatcher.h").read_text(encoding="utf-8")

checks = {
    "engine recognizes field MIXER-01 endpoint": 'normalised.contains ("MIXER-01")' in engine,
    "MIXER-01 has highest device preference": 'return 100;' in engine and 'MIXER-01' in engine,
    "startup scans recording inputs": 'const auto inputs = getInputDeviceNames();' in engine,
    "startup selects preferred SC220 input": 'selectInputDevice (preferredSc220Input)' in engine,
    "stale/default non-SC220 input is replaced": 'if (! looksLikeSc220InputName (activeInput))' in engine,
    "auto-selection is not limited to first-run state": engine.find('const auto activeInput = deviceManager.getAudioDeviceSetup().inputDeviceName;')
        > engine.find('if (savedDeviceState == nullptr)'),
    "UI recognizes Microphone (MIXER-01)": 'normalised.contains ("MIXER-01")' in ui,
    "runtime watcher is owned for console lifetime": 'SC220DeviceWatcher sc220DeviceWatcher { audioEngine };' in ui_header,
    "runtime watcher periodically rescans real inputs": 'audioEngine.getInputDeviceNames()' in watcher and 'startTimerHz (2)' in watcher,
    "removed SC220 is detached from active JUCE input": 'setup.inputDeviceName.clear();' in watcher,
    "removed SC220 disables input channels only": 'setup.useDefaultInputChannels = false;' in watcher and 'setup.inputChannels.clear();' in watcher,
    "stream output is preserved during SC220 removal": 'setup.outputDeviceName' not in watcher.split('if (! presentNow)', 1)[1].split('return;', 1)[0],
    "returning SC220 is automatically selected": 'audioEngine.selectInputDevice (preferred)' in watcher,
    "USB settling failures are retried without tight loop": 'retryTicks = 3;' in watcher,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")

if failed:
    print("\nSC220 device lifecycle audit failed:")
    for name in failed:
        print(f" - {name}")
    sys.exit(1)

print("\nSC220 startup + runtime device lifecycle audit passed.")
