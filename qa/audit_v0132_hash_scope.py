from pathlib import Path
import re, hashlib, sys
ROOT=Path(__file__).resolve().parents[1]
cmake=(ROOT/'CMakeLists.txt').read_text(encoding='utf-8')
checks=[]
def check(name, cond): checks.append((name,bool(cond)))
check('CMake project version 0.13.2','VERSION 0.13.2' in cmake)
check('Application version 0.13.2','return "0.13.2"' in (ROOT/'live/LiveApplication.cpp').read_text(encoding='utf-8'))
check('Checkout User-Agent 0.13.2','Sonkupik-Live/0.13.2' in (ROOT/'live/LicenseCheckoutClient.cpp').read_text(encoding='utf-8'))
check('No checkout client hard lock','live/LicenseCheckoutClient.cpp"\n    "' not in cmake)
check('No licence panel hard lock','live/LiveLicensePanel.cpp"\n    "' not in cmake)
check('DSP remains hard locked','${ASKP_DSP_SOURCE}' in cmake and 'sonkupik_assert_sha256' in cmake)
check('Preset remains hard locked','${ASKP_PRESET_SOURCE}' in cmake)
check('Processor remains hard locked','${ASKP_LIVE_PROCESSOR_SOURCE}' in cmake)
check('Sphere remains hard locked','${SONKUPIK_ORB_SOURCE}' in cmake)
check('Build BAT version updated','-Version 0.13.2' in (ROOT/'build-live-windows.bat').read_text(encoding='utf-8'))
# verify every remaining assertion that can be resolved
vars={
'ASKP_DSP_SOURCE':ROOT/'third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.cpp',
'ASKP_ENGINE_DIR':ROOT/'third_party/askp-engine-v0.5.19/engine',
'ASKP_PRESET_SOURCE':ROOT/'third_party/askp-engine-v0.5.19/engine/arsonkupik_presets.cpp',
'ASKP_LIVE_PROCESSOR_SOURCE':ROOT/'third_party/askp-engine-v0.5.19/src/PluginProcessor.cpp',
'ASKP_PROCESSOR_DIR':ROOT/'third_party/askp-engine-v0.5.19/src',
'ASKP_LICENSE_SOURCE':ROOT/'third_party/askp-engine-v0.5.19/src/LicenseManager.cpp',
'SONKUPIK_ORB_SOURCE':ROOT/'third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.cpp',
}
pat=re.compile(r'sonkupik_assert_sha256\("\$\{([^}]+)\}(/[^\"]*)?"\s*\n\s*"([0-9a-f]{64})"\)')
resolved=0
for m in pat.finditer(cmake):
    var,suffix,expected=m.groups()
    if var not in vars: continue
    p=Path(str(vars[var])+(suffix or ''))
    actual=hashlib.sha256(p.read_bytes()).hexdigest()
    resolved+=1
    check(f'hash lock valid: {p.relative_to(ROOT)}',actual==expected)
check('All remaining hash locks resolved',resolved==9)
lines=['Sonkupik Live v0.13.2 hash-scope audit','',f'Checks : {len(checks)}',f'Passed : {sum(v for _,v in checks)}',f'Failed : {sum(not v for _,v in checks)}','']
lines += [f"[ {'PASS' if ok else 'FAIL'} ] {name}" for name,ok in checks]
out='\n'.join(lines)+'\n'
(ROOT/'qa/v0132_hash_scope_audit.txt').write_text(out,encoding='utf-8')
print(out,end='')
sys.exit(0 if all(v for _,v in checks) else 1)
