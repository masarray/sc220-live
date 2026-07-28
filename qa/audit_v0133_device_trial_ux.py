from pathlib import Path
import hashlib
ROOT=Path(__file__).resolve().parents[1]
def read(p): return (ROOT/p).read_text(encoding='utf-8')
checks=[]
def check(name, ok): checks.append((name,bool(ok)))
cmake=read('CMakeLists.txt'); main=read('live/LiveMainComponent.cpp'); engine=read('live/LiveAudioEngine.cpp'); panel=read('live/LiveLicensePanel.cpp')
check('CMake project version 0.13.3','VERSION 0.13.3' in cmake)
check('Application version 0.13.3','return "0.13.3"' in read('live/LiveApplication.cpp'))
check('Checkout User-Agent 0.13.3','Sonkupik-Live/0.13.3' in read('live/LicenseCheckoutClient.cpp'))
check('Active trial hidden from header','showLicenseWarning' in main and 'TRIAL •' not in main)
check('Expired warning retained','licenseButton.setButtonText ("TRIAL ENDED")' in main)
check('Neutral settings licensing access','License & Activation' in main)
check('Panel hides countdown','FULL FEATURES ACTIVE' in panel and 'DAYS LEFT' not in panel)
check('Runtime bullet removed','•' not in main and '•' not in panel)
check('Verified device transaction','applyDeviceSelection' in engine and 'selectionApplied' in engine)
check('Automatic rate buffer fallback','automatic.sampleRate = 0.0' in engine and 'automatic.bufferSize = 0' in engine)
check('Device reopen fallback','deviceManager.closeAudioDevice();' in engine and 'Clean reopen attempt' in engine)
check('Rollback on failure','setAudioDeviceSetup (previous, true)' in engine)
check('UI queues device switch','MessageManager::callAsync' in main and 'deviceSwitchInProgress' in main)
check('Timer cannot revert selection','! deviceSwitchInProgress' in main)

authority = {
    'third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.cpp': 'd716a086dfcba06332871405f85058304be7f96bc356e8aa755fcc5300f79df2',
    'third_party/askp-engine-v0.5.19/engine/arsonkupik_dsp.hpp': 'fbfe14be0ea8e7254341c85cc03f96cea8cf96c247c2fbb5b310cb88f02d62c2',
    'third_party/askp-engine-v0.5.19/engine/arsonkupik_presets.cpp': '948ecf3bc7520413d1ea7a7b42cfca44291bbbf1ecb12c74aaeaca58a746910f',
    'third_party/askp-engine-v0.5.19/src/PluginProcessor.cpp': '3b8a31b753449386febf4d28478b1023e0ec10ec9b64ac0d1aff4215b84493ed',
    'third_party/askp-engine-v0.5.19/src/PluginProcessor.h': 'e2b5fef5a34330a8e26c437aed82aef97ba3703115de5094edd7142fbdd7fc6e',
    'third_party/askp-engine-v0.5.19/src/LicenseManager.cpp': '5e052cf06d65692cac1e03765e5e037a38bc2e4fa3b211b89ccbef658a342228',
    'third_party/askp-engine-v0.5.19/src/LicenseManager.h': 'dc6832ad9ccdb566f45dadb4ca36ec744845d212714f696389ae2b41207e3755',
    'third_party/askp-engine-v0.5.19/src/LicensePublicKey.h': '8fa4238d321dbdeb00b5408a10365cbc8110cd28006aa2ccd526c90f2e5ca372',
    'third_party/sonkupik-sphere-v0.10.4/OrbVisualizer.cpp': 'f5ed2351fb9d22dfbc520b56c86e19091632c516d174cc83983d708d4eda7a04',
}
for rel, expected in authority.items():
    actual=hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()
    check('Authority unchanged: '+rel, actual == expected)
passed=sum(v for _,v in checks)
lines=['Sonkupik Live v0.13.3 device/trial UX source audit','',f'Checks : {len(checks)}',f'Passed : {passed}',f'Failed : {len(checks)-passed}','']
lines += [f"[ {'PASS' if ok else 'FAIL'} ] {name}" for name,ok in checks]
out='\n'.join(lines)+'\n'
(ROOT/'qa/v0133_device_trial_ux_audit.txt').write_text(out,encoding='utf-8')
print(out,end='')
raise SystemExit(0 if passed==len(checks) else 1)
