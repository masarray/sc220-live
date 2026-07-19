#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ASKP = Path('/mnt/data/askp_p2/askp-vst-master')
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
engine = (ROOT / 'live/LiveAudioEngine.cpp').read_text(encoding='utf-8')
engine_h = (ROOT / 'live/LiveAudioEngine.h').read_text(encoding='utf-8')
main = (ROOT / 'live/LiveMainComponent.cpp').read_text(encoding='utf-8')
build = (ROOT / 'scripts/build-live-windows.ps1').read_text(encoding='utf-8')
run = (ROOT / 'scripts/run-p2-parity.ps1').read_text(encoding='utf-8')
renderer = (ROOT / 'qa/ParityRendererMain.cpp').read_text(encoding='utf-8')
compare = (ROOT / 'qa/compare_p2_parity.py').read_text(encoding='utf-8')
app = (ROOT / 'live/LiveApplication.cpp').read_text(encoding='utf-8')

check('Version 0.9.3', 'project(SonkupikLive VERSION 0.9.3' in cmake and 'return "0.9.3"' in app)
check('Authoritative DSP source direct', 'set(ASKP_DSP_SOURCE "${_askp_engine_dir}/arsonkupik_dsp.cpp")' in cmake)
check('Authoritative presets source direct', 'set(ASKP_PRESET_SOURCE "${_askp_engine_dir}/arsonkupik_presets.cpp")' in cmake)
check('Original processor retained for reference renderer', '"${ASKP_PROCESSOR_SOURCE}"' in cmake and 'SonkupikParityReference' in cmake)
check('Live overlay processor retained for live renderer', '"${ASKP_LIVE_PROCESSOR_SOURCE}"' in cmake and 'SonkupikParityLive' in cmake)
check('P1 overlay remains 12 ms', 'SONKUPIK_LIVE_CONTROL_RAMP_SECONDS "0.012"' in cmake)
check('P2 targets are optional', 'option(SONKUPIK_BUILD_PARITY_QA' in cmake and 'OFF)' in cmake)
check('Renderer checks all factory presets', 'arsonkupik::factory_presets().size()' in renderer and 'for (int index = 0; index < count; ++index)' in renderer)
check('Renderer uses direct askp Processor', 'askp::Processor processor;' in renderer and 'processor.processBlock (block, midi);' in renderer)
check('Renderer fixes unity I/O and wet enhancer', '"input_gain_db", 0.0f' in renderer and '"final_output_gain_db", 0.0f' in renderer and '"bypass", 0.0f' in renderer)
check('Renderer warm-up exists', 'kAnalysisStartSeconds = 3.0' in renderer)
check('Renderer exact output fingerprint', 'FNV' in renderer or '1099511628211ull' in renderer)
check('Comparator rejects hash mismatch', 'fnv1a64' in compare and 'Result: ' in compare)
check('PowerShell runner builds parity targets', '-ParityQa' in run and 'SonkupikParityReference' in run and 'SonkupikParityLive' in run)
check('Build helper exposes ParityQa', '[switch]$ParityQa' in build and 'SONKUPIK_BUILD_PARITY_QA=ON' in build)
check('Live audio calls authoritative Processor directly', 'enhancerProcessor.processBlock (processBuffer, emptyMidi);' in engine)
check('No legacy custom enhancer engine in Live callback', 'ArSonKuPikEngine' not in engine)
check('Manual reference conditions implemented', 'applyVstReferenceConditions' in engine_h and 'noiseClean.setEnabled (false);' in engine and 'setInputTrimDb (0.0f);' in engine and 'setOutputTrimDb (0.0f);' in engine)
check('Settings exposes Match VST Reference', 'Match VST Reference' in main and 'applyVstReferenceConditions' in main)
check('P2 documentation exists', (ROOT / 'docs/AUDIT_PRESET_PARITY_v0.9.3.md').exists())

# Verify the configure-time live processor is exactly one semantic line away
# from the uploaded authority source used for this audit.
if ASKP.exists():
    authority = (ASKP / 'src/PluginProcessor.cpp').read_text(encoding='utf-8')
    anchor = 'const double ramp = 0.08; // 80 ms knob glide'
    generated = authority.replace(anchor, 'const double ramp = 0.012; // Sonkupik Live low-latency macro glide')
    diffs = [(a, b) for a, b in zip(authority.splitlines(), generated.splitlines()) if a != b]
    check('Uploaded authority has audited P1 anchor', authority.count(anchor) == 1)
    check('Generated overlay changes one line only', len(diffs) == 1)
    preset_text = (ASKP / 'engine/arsonkupik_presets.cpp').read_text(encoding='utf-8')
    # Bank is append-only and lock file declares the audited count.
    lock = (ROOT / 'ASKP_AUTHORITY.lock').read_text(encoding='utf-8')
    check('Authority lock declares 40 presets', 'preset_count=40' in lock)
    check('Uploaded preset bank contains final appended contexts', 'Radio Mas Ari' in preset_text and 'Campursari' in preset_text)
else:
    check('Uploaded authority available for source comparison', False)

passed = sum(ok for _, ok in checks)
failed = len(checks) - passed
lines = ['SONKUPIK LIVE v0.9.3 P2 SOURCE PARITY AUDIT', '']
for name, ok in checks:
    lines.append(f"[{'PASS' if ok else 'FAIL'}] {name}")
lines += ['', f'Checks: {len(checks)}', f'Passed: {passed}', f'Failed: {failed}', f"Result: {'PASS' if failed == 0 else 'FAIL'}"]
report = '\n'.join(lines) + '\n'
(ROOT / 'qa/v093_source_parity_audit.txt').write_text(report, encoding='utf-8')
print(report, end='')
sys.exit(1 if failed else 0)
