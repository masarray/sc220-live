from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
script = (ROOT / "scripts/build-live-windows.ps1").read_text(encoding="utf-8")
package = (ROOT / "scripts/package-live-windows.ps1").read_text(encoding="utf-8")
bat = (ROOT / "build-live-windows.bat").read_text(encoding="utf-8")
app = (ROOT / "live/LiveApplication.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

checks = [
    ("CMake version 0.10.1", "VERSION 0.10.1" in cmake),
    ("Application version 0.10.1", 'return "0.10.1"' in app),
    ("Build closes running preview", "Stop-SonkupikLiveProcesses" in script),
    ("Build removes stale output", "Remove-StaleSonkupikOutputs" in script),
    ("Build retries once", "automatic linker-lock retry" in script),
    ("BAT preflight taskkill", 'taskkill /F /IM "Sonkupik Live.exe"' in bat),
    ("BAT packages v0.10.1", "-Version 0.10.1" in bat),
    ("Package cleanup has retry", "Remove-PathWithRetry" in package),
    ("Package default version 0.10.1", "[string]$Version = '0.10.1'" in package),
]
failed = [name for name, ok in checks if not ok]
lines = ["Sonkupik Live v0.10.1 Linker Lock Audit", ""]
lines += [("[PASS] " if ok else "[FAIL] ") + name for name, ok in checks]
lines += ["", f"Checks : {len(checks)}", f"Passed : {len(checks)-len(failed)}", f"Failed : {len(failed)}"]
(ROOT / "qa/v0101_build_lock_audit.txt").write_text("\n".join(lines)+"\n", encoding="utf-8")
if failed:
    raise SystemExit("Failed: " + ", ".join(failed))
