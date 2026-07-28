#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checks = []

def read(path): return (ROOT / path).read_text(encoding="utf-8")
def check(name, condition): checks.append((name, bool(condition)))

panel = read("live/LiveLicensePanel.cpp")
header = read("live/LiveLicensePanel.h")
theme = read("live/LiveTheme.cpp")
app = read("live/LiveApplication.cpp")
build = read("scripts/build-live-windows.ps1")
cmake = read("CMakeLists.txt")

check("Explicit homogeneous component array", "std::array<juce::Component*, 21> components" in panel)
check("No heterogeneous component braced loop", "for (auto* component : {" not in panel)
check("Array header included", "#include <array>" in header)
check("Unused ComboBox bool unnamed", "int width, int height, bool," in theme)
check("Display coordinates explicitly rounded", app.count("juce::roundToInt") >= 2)
check("Application version 0.13.1", 'return "0.13.1"' in app)
check("CMake version 0.13.1", "VERSION 0.13.1" in cmake)
check("Retry restricted to LNK1104", "isExecutableLock" in build and "LNK1104" in build)
check("Compile errors skip linker retry", "compile/source error; linker-lock retry was skipped" in build)

passed = sum(ok for _, ok in checks)
lines = ["Sonkupik Live v0.13.1 MSVC build-fix audit", "", f"Checks : {len(checks)}", f"Passed : {passed}", f"Failed : {len(checks)-passed}", ""]
for name, ok in checks: lines.append(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
out = ROOT / "qa/v0131_build_fix_audit.txt"
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("\n".join(lines))
raise SystemExit(0 if passed == len(checks) else 1)
