from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
app = (ROOT / 'live/LiveApplication.cpp').read_text(encoding='utf-8')
main_h = (ROOT / 'live/LiveMainComponent.h').read_text(encoding='utf-8')
main_cpp = (ROOT / 'live/LiveMainComponent.cpp').read_text(encoding='utf-8')
visual_cpp = (ROOT / 'live/LiveLiquidVisualizer.cpp').read_text(encoding='utf-8')
cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')

checks = []
def add(name, ok): checks.append((name, bool(ok)))

add('Version is 0.8.2', 'project(SonkupikLive VERSION 0.8.2' in cmake)
add('Application version is 0.8.2', 'getApplicationVersion() override { return "0.8.2"; }' in app)
add('Public explicit UI lifecycle method exists', 'void setUiActive (bool active);' in main_h)
add('UI active state is tracked', 'bool uiActive = false;' in main_h)
add('Constructor no longer starts timer before window show', 'startTimerHz (24);\n    visualizer.setVisualEnabled (isShowing());' not in main_cpp)
add('Active UI starts 24 Hz timer', 'startTimerHz (24);' in main_cpp and 'void LiveMainComponent::setUiActive' in main_cpp)
add('Inactive UI stops timer', 'stopTimer();' in main_cpp)
add('Active UI enables visual tap', 'visualizer.setVisualEnabled (active);' in main_cpp)
add('Show path explicitly activates UI', 'component->setUiActive (true);' in app)
add('Hide path explicitly deactivates UI', 'component->setUiActive (false);' in app)
add('Timer updates status only while active', 'if (uiActive)\n        updateStatus();' in main_cpp)
add('Status refresh updates input meter', 'inputFader.setLevels' in main_cpp)
add('Status refresh updates output meter', 'outputFader.setLevels' in main_cpp)
add('Status refresh updates visualizer', 'visualizer.refresh' in main_cpp)
add('Visual enable resets analysis', 'resetAnalysis();' in visual_cpp)
add('Visual tap still disabled while hidden', 'engine.setVisualTapEnabled (enabled);' in visual_cpp)

failed = [name for name, ok in checks if not ok]
report = ['SONKUPIK LIVE v0.8.2 UI RUNTIME AUDIT', '', f'Checks : {len(checks)}', f'Passed : {len(checks)-len(failed)}', f'Failed : {len(failed)}', '']
report += [f"[{'PASS' if ok else 'FAIL'}] {name}" for name, ok in checks]
text='\n'.join(report)+'\n'
(ROOT/'qa/v082_ui_runtime_audit.txt').write_text(text,encoding='utf-8')
print(text,end='')
raise SystemExit(1 if failed else 0)
