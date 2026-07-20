from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
engine_h = (root / 'live/LiveAudioEngine.h').read_text(encoding='utf-8')
engine_cpp = (root / 'live/LiveAudioEngine.cpp').read_text(encoding='utf-8')
ui_h = (root / 'live/LiveMainComponent.h').read_text(encoding='utf-8')
ui_cpp = (root / 'live/LiveMainComponent.cpp').read_text(encoding='utf-8')
widgets = (root / 'live/LiveWidgets.cpp').read_text(encoding='utf-8')
noise_h = (root / 'live/MusicalNoiseClean.h').read_text(encoding='utf-8')
noise_cpp = (root / 'live/MusicalNoiseClean.cpp').read_text(encoding='utf-8')
theme_h = (root / 'live/LiveTheme.h').read_text(encoding='utf-8')
app = (root / 'live/LiveApplication.cpp').read_text(encoding='utf-8')

check('Version 0.11.0', 'VERSION 0.11.0' in cmake and 'return "0.11.0"' in app)
check('Async stability recovery', 'private juce::AsyncUpdater' in engine_h and 'handleAsyncUpdate()' in engine_cpp)
check('Native/estimated xrun telemetry', 'deviceManager.getXRunCount()' in engine_cpp and 'driverXruns' in engine_h)
check('Deadline telemetry', 'deadlineMissCount' in engine_cpp and 'elapsedNs >= budgetNs' in engine_cpp)
check('Callback gap telemetry', 'timingGlitchCount' in engine_cpp and '2.35' in engine_cpp)
check('Bounded instability window', 'timingWindowBlocks >= 128' in engine_cpp)
check('Automatic buffer recovery', 'chooseNextStableBufferSize' in engine_cpp and 'setAudioDeviceSetup (setup, true)' in engine_cpp)
check('Recovery cooldown', '30000.0' in engine_cpp)
check('Cross-device safe floor', 'applyCrossDeviceSafeFloorIfNeeded' in engine_cpp and 'setup.bufferSize >= 128' in engine_cpp)
check('Settings guard switch', 'Auto Stability Guard' in ui_cpp and 'setStabilityGuardEnabled' in ui_cpp)
check('Stability footer state', 'MiniStat trayStat { "Stability"' in ui_h and 'status.stabilityState' in ui_cpp)
check('Narrower side panels', 'sidePanelWidth = 220' in theme_h and 'workspace.getWidth() * 0.158f' in ui_cpp)
check('Rack two-line hierarchy', '"ADD SLOT"' in widgets and 'statusArea = juce::Rectangle<int> (34, 27' in widgets)
check('Compact enhancer card', 'enhancerHeight' in ui_cpp and 'enhancerPresetSurface = enhancerInner.removeFromTop (48)' in ui_cpp)
check('Higher small-label contrast', 'textMuted  { 0xff93a2b5 }' in theme_h)
check('Wider expander meter', 'withSizeKeepingCentre (72' in ui_cpp)
check('Cached expander coefficients', 'detectorAttackCoefficient' in noise_h and 'gainCloseCoefficient' in noise_cpp)
check('Pointer-based expander processing', 'targetLeft[sample] *= gain' in noise_cpp and 'detectorLeft[sample]' in noise_cpp)
check('Vectorised output peak scan', 'getMagnitude (0, 0, samples)' in engine_cpp)
check('No allocation in callback', 'new ' not in engine_cpp[engine_cpp.find('audioDeviceIOCallbackWithContext'):engine_cpp.find('audioDeviceAboutToStart')])
check('No mutex in callback', 'ScopedLock' not in engine_cpp[engine_cpp.find('audioDeviceIOCallbackWithContext'):engine_cpp.find('audioDeviceAboutToStart')])

for path in [root/'live/LiveAudioEngine.cpp', root/'live/LiveAudioEngine.h',
             root/'live/LiveMainComponent.cpp', root/'live/LiveWidgets.cpp',
             root/'live/MusicalNoiseClean.cpp']:
    text = path.read_text(encoding='utf-8')
    check(f'Brace balance {path.name}', text.count('{') == text.count('}'))

passed = sum(ok for _, ok in checks)
out = ['v0.11.0 P0 source audit', f'Checks : {len(checks)}', f'Passed : {passed}', f'Failed : {len(checks)-passed}', '']
out += [f'[ {"PASS" if ok else "FAIL"} ] {name}' for name, ok in checks]
report = '\n'.join(out) + '\n'
(root/'qa/v0110_p0_audit.txt').write_text(report, encoding='utf-8')
print(report)
raise SystemExit(0 if passed == len(checks) else 1)
