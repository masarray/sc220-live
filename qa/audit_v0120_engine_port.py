#!/usr/bin/env python3
from pathlib import Path
import hashlib
import re
import sys

root = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

def text(rel):
    return (root / rel).read_text(encoding='utf-8')

def sha(rel):
    return hashlib.sha256((root / rel).read_bytes()).hexdigest()

cmake = text('CMakeLists.txt')
app = text('live/LiveApplication.cpp')
engine_h = text('third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.hpp')
engine_cpp = text('third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.cpp')
presets = text('third_party/askp-engine-v0.5.19/engine/arsonkupik_presets.cpp')
live_proc = text('third_party/askp-engine-v0.5.19/src/PluginProcessor.cpp')
ref_proc = text('third_party/askp-engine-v0.5.19/reference/PluginProcessor.cpp')
live_proc_h = text('third_party/askp-engine-v0.5.19/src/PluginProcessor.h')
bridge_h = text('live/EnhancerEngineBridge.h')
bridge_cpp = text('live/EnhancerEngineBridge.cpp')
live_audio = text('live/LiveAudioEngine.cpp')
sphere = text('third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.cpp')
build_ps = text('scripts/build-live-windows.ps1')
package_ps = text('scripts/package-live-windows.ps1')
bat = text('build-live-windows.bat')

check('Version 0.12.0', 'VERSION 0.12.0' in cmake and 'return "0.12.0"' in app)
check('Embedded engine revision', 'ASKP_DSP_HEADER_REVISION 2026072702' in engine_h)
check('13 factory profiles', 'profiles.reserve(13)' in presets and presets.count('profiles.push_back(profile(') == 13)
check('Single immutable Signature DSP', 'presets.reserve(1)' in presets and 'Mas Ari Signature' in presets)
check('Live processor headless', 'hasEditor() const override { return false; }' in live_proc_h
      and 'return nullptr;' in live_proc and 'PluginEditor.h' not in live_proc)
check('Live control glide 12 ms', 'const double ramp = 0.012;' in live_proc)
check('Reference control glide 80 ms', 'const double ramp = 0.08; // 80 ms knob glide' in ref_proc)
check('Only intended processor delta', ref_proc.replace(
      'const double ramp = 0.08; // 80 ms knob glide',
      'const double ramp = 0.012; // Sonkupik Live 12 ms direct-control glide') == live_proc)
check('Engine bridge owns APVTS boundary', 'class EnhancerEngineBridge' in bridge_h
      and 'setParameter (' in bridge_cpp and 'apvts' not in live_audio)
check('Live audio uses bridge', 'enhancerBridge.process' in live_audio
      and 'enhancerBridge.prepare' in live_audio
      and 'enhancerBridge.setPresetIndex' in live_audio)
check('Gain Match disabled by default', 'setGainMatchEnabled (false)' in bridge_cpp
      and 'enhancerBridge.setGainMatchEnabled (false)' in live_audio)
check('No external ASK-P checkout dependency', 'SONKUPIK_ASKP_SOURCE_DIR' not in cmake
      and 'Resolve-AskpAuthority' not in build_ps
      and 'AskpSourceDir' not in build_ps + package_ps)
check('Hash-locked configure', 'sonkupik_assert_sha256' in cmake
      and 'Re-port and rerun QA' in cmake)
check('Local visualizer source', 'third_party/sonkupik-sphere-v0.10.4' in cmake
      and 'ASKP_ORB_SOURCE' not in cmake)
check('Approved sphere has no new delayed-frame regression', 'visualFrames_' not in sphere
      and 'kVisualDelayUiFrames' not in sphere
      and 'devImpactTelemetry_' not in sphere
      and 'No glitter' in sphere)
check('Single local black bezel', 'approved single bezel' in sphere
      and '0xFF2B3948' in sphere and '0xFFF8F8FA' not in sphere)
check('No configure-time visual source patch', '_askp_orb_text' not in cmake
      and 'string(REPLACE' not in cmake)
check('No VST editor/QRIS source in target', 'PluginEditor.cpp' not in cmake
      and 'QrisLicensePanel.cpp' not in cmake)
check('Build package version', '-Version 0.12.0' in bat and "[string]$Version = '0.12.0'" in package_ps)
check('Engine source hash', sha('third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.cpp') ==
      'd716a086dfcba06332871405f85058304be7f96bc356e8aa755fcc5300f79df2')
check('Preset source hash', sha('third_party/askp-engine-v0.5.19/engine/arsonkupik_presets.cpp') ==
      '948ecf3bc7520413d1ea7a7b42cfca44291bbbf1ecb12c74aaeaca58a746910f')
check('Live processor hash', sha('third_party/askp-engine-v0.5.19/src/PluginProcessor.cpp') ==
      '3b8a31b753449386febf4d28478b1023e0ec10ec9b64ac0d1aff4215b84493ed')
check('Sphere source hash', sha('third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.cpp') ==
      'f5ed2351fb9d22dfbc520b56c86e19091632c516d174cc83983d708d4eda7a04')

# Selected v0.5.19 mature-engine source contracts.
check('Adaptive generated scene lanes', all(token in engine_h for token in (
      'detail_focus', 'articulation_focus', 'density_tolerance', 'body_focus', 'space_focus')))
check('P7G floor-only gain policy', 'const float wantedDb = liftDb < minimumActiveLiftDb' in live_proc
      and '? minimumActiveLiftDb - liftDb' in live_proc
      and 'else if (liftDb >' not in live_proc)
check('Final limiter policy retained', 'kTransientLimiterBudgetDb = 0.65f' in
      text('third_party/askp-engine-v0.5.19/engine/wow_headroom_guard.hpp')
      and 'kSustainedLimiterBudgetDb = 0.20f' in
      text('third_party/askp-engine-v0.5.19/engine/wow_headroom_guard.hpp'))
check('Cold-start crackle guard retained', '// Cold-start crackle guard' in live_proc
      and 'sampleRate * 0.060' in live_proc
      and 'startupFadeSamplesRemaining_' in live_proc
      and 'Raised-cosine shape' in live_proc
      and 'startupDrySafetyGain' in live_proc)
check('Callback has no user-preset file IO', 'juce::File' not in live_proc[
      live_proc.index('void Processor::processBlock'):
      live_proc.index('// ---------------------------------------------------------------------------\n// A/B compare')])

passed = sum(ok for _, ok in checks)
failed = [name for name, ok in checks if not ok]
lines = [
    'Sonkupik Live v0.12.0 enhancer engine port audit',
    f'Checks : {len(checks)}',
    f'Passed : {passed}',
    f'Failed : {len(failed)}',
    '',
]
for name, ok in checks:
    lines.append(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
if failed:
    lines += ['', 'Failed checks:', *[f'- {name}' for name in failed]]
report = '\n'.join(lines) + '\n'
(root / 'qa/v0120_engine_port_audit.txt').write_text(report, encoding='utf-8')
print(report, end='')
sys.exit(0 if not failed else 1)
