from pathlib import Path
import hashlib

root = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

h = (root/'live/LiveTheme.h').read_text()
t = (root/'live/LiveTheme.cpp').read_text()
i = (root/'live/LiveIcons.cpp').read_text()
w = (root/'live/LiveWidgets.cpp').read_text()
m = (root/'live/LiveMainComponent.cpp').read_text()
cm = (root/'CMakeLists.txt').read_text()

check('Version 0.10.3', 'VERSION 0.10.3' in cm)
check('Tooltip bounds override', 'getTooltipBounds' in h and 'LiveLookAndFeel::getTooltipBounds' in t)
check('Structured tooltip microcopy', "containsChar ('|')" in t and 'fromFirstOccurrenceOf' in t)
check('Tooltip uses constrained bounds', 'constrainedWithin' in t)
check('No oversized callout path', 'callout.startNewSubPath' not in t)
check('Quiet momentary icon buttons', '(toggleButton && active)' in i)
check('Animated icon hover', 'hoverMix' in i and 'startTimerHz (60)' in i)
check('Animated rack hover', 'CompactRackRow::timerCallback' in w and 'hoverMix' in w)
check('Rack icon bay removed', 'iconBay' not in w)
check('Nested control bay removed', 'layout.noiseControlSurface' not in m[m.find('void LiveMainComponent::paint'):])
check('Only response visual bay remains', m.count('drawInsetBay (g, layout.') == 1)
check('Macro control bay label removed', 'MACRO CONTROL BAY' not in m)
check('Active preset simplified', '"PRESET"' in m and 'ACTIVE PRESET' not in m)
check('Primary knob tooltips', 'Noise Reduction|' in m and 'Stereo Magic|' in m)
check('Tooltip delay increased', 'tooltip { this, 850 }' in (root/'live/LiveMainComponent.h').read_text())
check('Package version updated', '0.10.3' in (root/'scripts/package-live-windows.ps1').read_text())
check('Single click build version updated', '0.10.3' in (root/'build-live-windows.bat').read_text())

# Audio sources must be unchanged from v0.10.2 if adjacent baseline exists.
baseline = root.parent/'Sonkupik-Live-v0.10.2-source'
if baseline.exists():
    for rel in ['live/LiveAudioEngine.cpp', 'live/MusicalNoiseClean.cpp']:
        a = hashlib.sha256((root/rel).read_bytes()).hexdigest()
        b = hashlib.sha256((baseline/rel).read_bytes()).hexdigest()
        check(f'Audio source unchanged: {rel}', a == b)

passed = sum(v for _, v in checks)
lines = [f'P3 source audit v0.10.3', f'Checks : {len(checks)}', f'Passed : {passed}', f'Failed : {len(checks)-passed}', '']
for name, ok in checks:
    lines.append(f'[ {"PASS" if ok else "FAIL"} ] {name}')
result = '\n'.join(lines) + '\n'
(root/'qa/v0103_p3_audit.txt').write_text(result)
print(result)
raise SystemExit(0 if passed == len(checks) else 1)
