from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))

cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
app = (ROOT / 'live/LiveApplication.cpp').read_text(encoding='utf-8')
main = (ROOT / 'live/LiveMainComponent.cpp').read_text(encoding='utf-8')
widgets_h = (ROOT / 'live/LiveWidgets.h').read_text(encoding='utf-8')
widgets = (ROOT / 'live/LiveWidgets.cpp').read_text(encoding='utf-8')
theme = (ROOT / 'live/LiveTheme.cpp').read_text(encoding='utf-8')
gitignore = (ROOT / '.gitignore').read_text(encoding='utf-8')
bat = (ROOT / 'build-live-windows.bat').read_text(encoding='utf-8')
package = (ROOT / 'scripts/package-live-windows.ps1').read_text(encoding='utf-8')
local_qa = (ROOT / 'scripts/run-sc220-local-qa.ps1').read_text(encoding='utf-8')
live_target = cmake.split('target_sources(SonkupikLive PRIVATE', 1)[1].split('target_include_directories(SonkupikLive', 1)[0]

check('CMake version 0.10.0', 'VERSION 0.10.0' in cmake)
check('Application version 0.10.0', 'return "0.10.0"' in app)
check('Single-click BAT exists', (ROOT / 'build-live-windows.bat').exists())
check('BAT builds and packages Release', 'package-live-windows.ps1' in bat and '-Configuration Release' in bat)
check('BAT supports clean rebuild', 'build-live-windows.bat clean' in bat and '-Clean' in bat)
check('Package script default version updated', "[string]$Version = '0.10.0'" in package)
check('Package script forwards clean build', '[switch]$Clean' in package and '$buildScriptArgs.Clean = $true' in package)
check('Private repository has no GitHub workflow definitions', not list((ROOT / '.github/workflows').glob('*.yml')))
check('Live target remains headless', 'ASKP_EDITOR_SOURCE' not in live_target and 'QrisLicensePanel' not in live_target)
check('Git ignores build trees', '/build-*/' in gitignore and '/dist/' in gitignore)
check('Git ignores fetched dependencies', '.deps/' in gitignore and 'third_party/JUCE/' in gitignore)
check('Git ignores signing material', '*.pfx' in gitignore and '*private-signing-key*.txt' in gitignore)
check('Fader bypass state is explicit', 'void setBypassed (bool value) noexcept;' in widgets_h)
check('Fader has integrated chassis', 'BYPASSED - MOVE FADER TO ACTIVATE' in widgets and 'valuePill' in widgets)
check('Stereo meter has lightweight peak hold', 'peakHoldFramesL' in widgets_h and 'approximately 500 ms' in widgets)
check('Premium fader thumb implemented', 'thumbGrad' in theme and 'grooveY' in theme)
check('Rack row full click toggles bypass', 'toggleBypassFromUi' in widgets and 'Click the row or power icon' in widgets)
check('Rack row status capsules implemented', '"BYPASS" : "ON"' in widgets and '"SAFE"' in widgets)
check('Noise control inset bay exists', 'noiseControlSurface' in main and 'drawInsetBay' in main)
check('Visualizer inset bay exists', 'noiseVisualSurface' in main and 'LIVE RESPONSE' in main)
check('Enhancer preset bay exists', 'enhancerPresetSurface' in main and 'ACTIVE PRESET' in main)
check('Enhancer macro bay exists', 'enhancerMacroSurface' in main and 'MACRO CONTROL BAY' in main)
check('Fader bypass follows rack state', 'inputFader.setBypassed' in main and 'outputFader.setBypassed' in main)
check('24 Hz policy unchanged', 'startTimerHz (24)' in main)
check('P1 documentation exists', (ROOT / 'docs/P1_PREMIUM_POLISH_v0.10.0.md').exists())

passed = sum(ok for _, ok in checks)
failed = len(checks) - passed
lines = ['Sonkupik Live v0.10.0 P1 Premium Polish Audit', '']
lines.extend(f"[{'PASS' if ok else 'FAIL'}] {name}" for name, ok in checks)
lines += ['', f'Checks : {len(checks)}', f'Passed : {passed}', f'Failed : {failed}',
          f'Result : {"PASS" if failed == 0 else "FAIL"}']
report = '\n'.join(lines) + '\n'
(ROOT / 'qa/v0100_p1_audit.txt').write_text(report, encoding='utf-8')
print(report, end='')
raise SystemExit(1 if failed else 0)
