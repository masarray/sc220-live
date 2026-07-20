from pathlib import Path
import hashlib
root = Path(__file__).resolve().parents[1]
checks=[]
def check(name, ok): checks.append((name,bool(ok)))

theme=(root/'live/LiveTheme.cpp').read_text()
hdr=(root/'live/LiveMainComponent.h').read_text()
main=(root/'live/LiveMainComponent.cpp').read_text()
icons=(root/'live/LiveIcons.cpp').read_text()
cm=(root/'CMakeLists.txt').read_text()
app=(root/'live/LiveApplication.cpp').read_text()

check('Version 0.10.4', 'VERSION 0.10.4' in cm and 'return "0.10.4"' in app)
check('Opaque premium tooltip wrapper', 'class PremiumTooltipWindow' in hdr and 'setOpaque (true)' in hdr)
check('Tooltip paints every pixel', 'g.fillAll (colours::panelLo)' in theme)
check('Fallback gets full bounds', 'drawFallbackIcon (g, bounds.transformedBy' in icons)
check('No double-reduced fallback call', 'drawFallbackIcon (g, iconBounds.transformedBy' not in icons)
check('CPU display uses wall-clock cadence', 'nextCpuUiUpdateMs' in hdr and 'nowMs + 1000u' in main)
check('CPU display smoothed', 'displayedCpuPercent +=' in main)
check('Peak display decays', 'displayedPeakPercent * 0.90f' in main)
check('Package version', "Version = '0.10.4'" in (root/'scripts/package-live-windows.ps1').read_text())
check('BAT version', '-Version 0.10.4' in (root/'build-live-windows.bat').read_text())

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
check('Audio engine unchanged', sha(root/'live/LiveAudioEngine.cpp') == '2e06beadd9f04737f2ce8d66eef3cf77a6672c6f6daba9a6d3ec478272c51ada')
check('Noise clean engine unchanged', sha(root/'live/MusicalNoiseClean.cpp') == '0a039218536db3790ac6c91c7b0a546f0373159b4335614707ce6206ee9a9553')

passed=sum(ok for _,ok in checks)
out=[f'v0.10.4 source audit',f'Checks : {len(checks)}',f'Passed : {passed}',f'Failed : {len(checks)-passed}','']
out += [f'[ {"PASS" if ok else "FAIL"} ] {name}' for name,ok in checks]
(root/'qa/v0104_fix_audit.txt').write_text('\n'.join(out)+'\n')
print('\n'.join(out))
raise SystemExit(0 if passed==len(checks) else 1)
