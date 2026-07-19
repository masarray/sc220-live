from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

cmake = (ROOT / 'CMakeLists.txt').read_text(errors='ignore')
main = (ROOT / 'live/LiveMainComponent.cpp').read_text(errors='ignore')
theme_h = (ROOT / 'live/LiveTheme.h').read_text(errors='ignore')
theme_cpp = (ROOT / 'live/LiveTheme.cpp').read_text(errors='ignore')
icons_h = (ROOT / 'live/LiveIcons.h').read_text(errors='ignore')
icons_cpp = (ROOT / 'live/LiveIcons.cpp').read_text(errors='ignore')
widgets = (ROOT / 'live/LiveWidgets.cpp').read_text(errors='ignore')
app = (ROOT / 'live/LiveApplication.cpp').read_text(errors='ignore')

check('CMake project version is 0.9.9', 'VERSION 0.9.9' in cmake)
check('Application version is 0.9.9', 'return "0.9.9"' in app)
check('Shared metrics namespace exists', 'namespace metrics' in theme_h)
check('Header metric is centralised', 'headerHeight = 60' in theme_h)
check('Footer metric is centralised', 'footerHeight = 48' in theme_h)
check('Single WorkspaceLayout exists', 'struct WorkspaceLayout' in main)
check('paint uses WorkspaceLayout', 'const auto layout = makeWorkspaceLayout (getLocalBounds());' in main)
check('resized uses WorkspaceLayout', main.count('const auto layout = makeWorkspaceLayout (getLocalBounds());') >= 2)
check('Premium I/O strip surface exists', 'fillPremiumPanel (g, layout.ioStrip.toFloat()' in main)
check('Engine status capsule exists', 'LIVE ENGINE  /  VB-CABLE READY' in main and 'statusPill' in main)
check('Noise control/visual divider exists', 'noiseDivider' in main)
check('Preset caption hierarchy exists', 'g.drawText ("PRESET"' in main)
check('Expander meter is vertical', 'gainReductionMeter.setVertical (true)' in main)
check('Expander meter uses negative scale labels', 'juce::String ("-") + juce::String (tick)' in widgets)
check('Expander meter shows actual negative readout', 'juce::String (-reductionDb, 1)' in widgets)
check('Momentary icon buttons are quiet at idle', 'const bool showSurface = down || highlighted || (toggleButton && active);' in icons_cpp)
check('Lucide active icon is cached', 'std::unique_ptr<juce::Drawable> activeIcon' in icons_h)
check('Lucide inactive icon is cached', 'std::unique_ptr<juce::Drawable> inactiveIcon' in icons_h)
check('Header icon fallback is present', 'drawFallbackIcon' in icons_cpp)
check('Panel inner highlight exists', 'topHighlight' in theme_cpp)
check('UI refresh remains 24 Hz', 'startTimerHz (24)' in main)
check('Option A audit documentation exists', (ROOT / 'docs/P0_PREMIUM_LAYOUT_AUDIT_v0.9.9.md').exists())
check('Option B blueprint documentation exists', (ROOT / 'docs/P0_LAYOUT_REDESIGN_BLUEPRINT_v0.9.9.md').exists())

passed = sum(ok for _, ok in checks)
failed = len(checks) - passed
lines = [
    'Sonkupik Live v0.9.9 P0 Layout Audit',
    '=' * 42,
    f'Checks : {len(checks)}',
    f'Passed : {passed}',
    f'Failed : {failed}',
    '',
]
for name, ok in checks:
    lines.append(f'[{"PASS" if ok else "FAIL"}] {name}')

report = '\n'.join(lines) + '\n'
(ROOT / 'qa/v099_layout_p0_audit.txt').write_text(report)
print(report)
raise SystemExit(1 if failed else 0)
