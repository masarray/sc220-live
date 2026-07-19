from __future__ import annotations

from pathlib import Path
import math
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ASKP = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

checks: list[tuple[str, bool]] = []

def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
engine = (ROOT / "live/LiveAudioEngine.cpp").read_text(encoding="utf-8")
ui = (ROOT / "live/LiveMainComponent.cpp").read_text(encoding="utf-8")
app = (ROOT / "live/LiveApplication.cpp").read_text(encoding="utf-8")

check("Version 0.9.2", 'project(SonkupikLive VERSION 0.9.2' in cmake and 'return "0.9.2"' in app)
check("12 ms live control overlay", 'SONKUPIK_LIVE_CONTROL_RAMP_SECONDS "0.012"' in cmake)
check("Audited 80 ms anchor", 'const double ramp = 0.08; // 80 ms knob glide' in cmake)
check("Unknown authority revision fails", 'Unable to apply the Sonkupik Live fast-control overlay' in cmake)
check("Generated processor compiled", '"${ASKP_LIVE_PROCESSOR_SOURCE}"' in cmake)
check("Original processor not compiled in target", cmake.count('"${ASKP_PROCESSOR_SOURCE}"') == 2)  # read + authority check only
check("Direct askp processor block", 'enhancerProcessor.processBlock (processBuffer, emptyMidi);' in engine)
check("Direct UI macro setters", all(token in ui for token in (
    'audioEngine.setEnhance', 'audioEngine.setSmartBass', 'audioEngine.setSmartTreble',
    'audioEngine.setVocalBody', 'audioEngine.setStereoMagic')))
check("No legacy live engine builder", not any(token in engine for token in (
    'engineBuilder', 'pendingEngine', 'coalescing', 'buildRequested')))
check("P1 audit documentation", (ROOT / 'docs/AUDIT_CONTROL_RESPONSE_v0.9.2.md').exists())

if ASKP is not None:
    processor_cpp = ASKP / 'src/PluginProcessor.cpp'
    processor_h = ASKP / 'src/PluginProcessor.h'
    check("Authority Processor source exists", processor_cpp.exists() and processor_h.exists())
    if processor_cpp.exists() and processor_h.exists():
        cpp = processor_cpp.read_text(encoding='utf-8')
        hdr = processor_h.read_text(encoding='utf-8')
        check("Authority retains 80 ms VST glide", 'const double ramp = 0.08; // 80 ms knob glide' in cpp)
        check("Authority retains 400 Hz retune ceiling", 'kCreativeRetuneHz = 400.0' in hdr)
        check("Authority APVTS target path present", 'smEnhance_.setTargetValue' in cpp and 'smStereo_.setTargetValue' in cpp)

print("SONKUPIK LIVE v0.9.2 P1 CONTROL RESPONSE AUDIT")
print("=" * 55)
for name, passed in checks:
    print(f"[{'PASS' if passed else 'FAIL'}] {name}")

# Mathematical callback quantisation report.
print("\n48 kHz RESPONSE ESTIMATE")
print("buffer  block_ms  old_complete_ms  live_complete_ms  first_live_move")
for buffer in (64, 128, 256, 480, 512):
    block_ms = buffer / 48000.0 * 1000.0
    old_complete = math.ceil(80.0 / block_ms) * block_ms
    live_complete = math.ceil(12.0 / block_ms) * block_ms
    first_fraction = min(1.0, block_ms / 12.0)
    print(f"{buffer:>6}  {block_ms:>8.2f}  {old_complete:>15.2f}  {live_complete:>16.2f}  {first_fraction*100:>14.1f}%")

failed = sum(not passed for _, passed in checks)
print(f"\nChecks: {len(checks)} | Passed: {len(checks)-failed} | Failed: {failed}")
raise SystemExit(1 if failed else 0)
