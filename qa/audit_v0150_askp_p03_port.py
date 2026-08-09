from pathlib import Path
import subprocess

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

# Latest approved ASK-P v0.5.24 release request names this exact immutable
# product source commit. Later upstream HEAD commits only adjust release request
# / release-audit helper metadata, not the product DSP/preset/processor source.
source_commit = "1cf050d40338cbcb5d8cddd9fc2de5f23e3d625f"
latest_audited_head = "9b275b42e6668c958ac4367b86c6e8660922c8cb"
check("ASK-P v0.5.24 approved source pinned", source_commit in cmake and source_commit in lock and source_commit in sync_script)
check("latest upstream audited head recorded", latest_audited_head in lock)
check("ASK-P submodule declared", "third_party/askp-vst" in gitmodules)

try:
    tree = subprocess.run(
        ["git", "ls-tree", "HEAD", "--", "third_party/askp-vst"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
except (OSError, subprocess.CalledProcessError):
    tree = ""
check("v0.5.24 approved source gitlink pinned", f"160000 commit {source_commit}" in tree)

check("DSP revision 2026073102", "2026073102" in cmake and "2026073102" in bridge_cpp)
check("13-profile contract", all(x in cmake + bridge_h + lock for x in [
    "profiles.reserve", "factoryProfileCount = 13", "factory_profiles=13"]))
check("upstream production source audit required", all(x in cmake for x in [
    "p0_musical_rebalance_source_audit.py",
    "ASKP_P03_SOURCE_AUDIT",
    "WORKING_DIRECTORY \"${ASKP_PORT_ROOT}\"",
    "production source audit failed",
]))
check("upstream P0.3 generator required", all(x in cmake for x in [
    "generate_p0_musical_rebalance_dsp.py",
    "find_package(Python3 REQUIRED COMPONENTS Interpreter)",
    "--input \"${ASKP_DSP_CANONICAL_SOURCE}\"",
    "--output \"${ASKP_DSP_SOURCE}\"",
]))
check("generated production DSP compiled instead of canonical", all(x in cmake for x in [
    "set(ASKP_DSP_CANONICAL_SOURCE",
    "set(ASKP_DSP_SOURCE \"${ASKP_GENERATED_DIR}/arsonkupik_dsp.cpp\")",
    'target_sources(SC220Live PRIVATE\n    "${ASKP_DSP_SOURCE}"',
]))
check("P0.3 smart warm vocal anchor", "lowerBodyLedVocalTarget" in cmake)
check("P0.2 sparkling air anchor", "tickleMidLift = 1.21" in cmake and "0.066, 0.026, 0.094" in cmake)
check("Signature full-band compressor bypass anchor", "working_.compressor.enabled = false" in cmake)
check("P0 warm transient guard anchor", "runtime.warm_transient_guard = 0.58" in cmake)
check("preset calibration guarded", "P0 Musical Rebalance" in cmake)
check("production release state recorded", all(x in lock for x in [
    "checkpoint_release_ready=true", "release_request_approved=true", "engine_version=0.5.24"]))
check("engine version is v0.5.24", 'engineVersion = "0.5.24"' in bridge_h and "JucePlugin_Version=0.5.24" in cmake and "JucePlugin_VersionCode=0x518" in cmake)
check("build channel names approved v0.5.24 source", "Embedded Authority v0.5.24 @ 1cf050d" in cmake)
check("Signature Smart Bass fallback follows P0", "getParameter (smartBassId, 70.0f)" in bridge_cpp)
check("headless editor generation", "return nullptr" in cmake and "hasEditor() const override { return false; }" in cmake)
check("reference keeps 80 ms", "PluginProcessor_Reference.cpp" in cmake and "0.08" in cmake)
check("Live glide 12 ms", "PluginProcessor_Live.cpp" in cmake and "0.012" in cmake)
check("honest safety meters bridged", all(x in bridge_h + bridge_cpp for x in [
    "safetyGainReductionDb", "preSafetyPeakDbL", "preSafetyPeakDbR",
    "inputOver", "preSafetyOver", "outputOver"]))
check("ASK-P Orb excluded", "${ASKP_PROCESSOR_DIR}/OrbVisualizer.cpp" not in cmake and "${ASKP_PORT_ROOT}/src/OrbVisualizer.cpp" not in cmake)
check("local sphere retained", "third_party/sonkupik-sphere-v0.10.4" in cmake)
check("sphere header is unambiguous", all(x in sphere_host for x in [
    '../third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.h',
    'Do not use a generic']))
check("generic Orb header lookup removed", '#include "OrbVisualizer.h"' not in sphere_host)
check("tracked C++ source forced to LF", "*.cpp text eol=lf" in gitattributes)
check("legacy CRLF sphere normalised before CMake", all(x in sync_script for x in [
    "Normalize-TextFileToLf", "OrbVisualizer.cpp", ".Replace(\"`r`n\", \"`n\")"]))
check("sync verifies both production audit and generator", all(x in sync_script for x in [
    "p0_musical_rebalance_source_audit.py", "generate_p0_musical_rebalance_dsp.py"]))

failed = [name for name, ok in checks if not ok]
print("SC220 Live ASK-P v0.5.24 production P0.3 port audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
