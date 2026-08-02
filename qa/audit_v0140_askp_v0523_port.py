from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(name, ok):
    checks.append((name, bool(ok)))

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

cmake = read("CMakeLists.txt")
bridge_h = read("live/EnhancerEngineBridge.h")
bridge_cpp = read("live/EnhancerEngineBridge.cpp")
sphere_host = read("live/SonkupikSphereVisualizer.h")
lock = read("ASKP_AUTHORITY.lock")
gitmodules = read(".gitmodules")
gitattributes = read(".gitattributes")
sync_script = read("scripts/sync-askp-v0523.ps1")

commit = "5757119cab7a1a75f09dcf0edea6e3be97975439"
check("Sonkupik version 0.14.0", "VERSION 0.14.0" in cmake)
check("ASK-P v0.5.23 commit pinned", commit in cmake and commit in lock)
check("ASK-P submodule declared", "third_party/askp-vst" in gitmodules)
check("DSP revision 2026073102", "2026073102" in cmake and "2026073102" in bridge_cpp)
check("13-profile contract", all(x in cmake + bridge_h + lock for x in [
    "profiles.reserve", "factoryProfileCount = 13", "factory_profiles=13"]))
check("headless editor generation", "return nullptr" in cmake and "hasEditor() const override { return false; }" in cmake)
check("reference keeps 80 ms", "PluginProcessor_Reference.cpp" in cmake and "0.08" in cmake)
check("Live glide 12 ms", "PluginProcessor_Live.cpp" in cmake and "0.012" in cmake)
check("bridge engine identity", 'engineVersion = "0.5.23"' in bridge_h)
check("honest safety meters bridged", all(x in bridge_h + bridge_cpp for x in [
    "safetyGainReductionDb", "preSafetyPeakDbL", "preSafetyPeakDbR",
    "inputOver", "preSafetyOver", "outputOver"]))
check("authoritative preset index API", "currentFactoryPresetIndex" in bridge_cpp)
check("ASK-P Orb excluded", "${ASKP_PROCESSOR_DIR}/OrbVisualizer.cpp" not in cmake and "${ASKP_PORT_ROOT}/src/OrbVisualizer.cpp" not in cmake)
check("local sphere retained", "third_party/sonkupik-sphere-v0.10.4" in cmake)
check("sphere header is unambiguous", all(x in sphere_host for x in [
    '../third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.h',
    'Do not use a generic']))
check("generic Orb header lookup removed", '#include "OrbVisualizer.h"' not in sphere_host)
check("tracked C++ source forced to LF", "*.cpp text eol=lf" in gitattributes)
check("legacy CRLF sphere normalised before CMake", all(x in sync_script for x in [
    "Normalize-TextFileToLf", "OrbVisualizer.cpp", ".Replace(\"`r`n\", \"`n\")"]))

failed = [name for name, ok in checks if not ok]
print("Sonkupik Live v0.14.0 ASK-P v0.5.23 port audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks)-len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
