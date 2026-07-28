from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition):
    checks.append((name, bool(condition)))

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')

cmake = read('CMakeLists.txt')
license_h = read('third_party/askp-engine-v0.5.19/src/LicenseManager.h')
license_cpp = read('third_party/askp-engine-v0.5.19/src/LicenseManager.cpp')
checkout_h = read('live/LicenseCheckoutClient.h')
checkout_cpp = read('live/LicenseCheckoutClient.cpp')
panel_h = read('live/LiveLicensePanel.h')
panel_cpp = read('live/LiveLicensePanel.cpp')
bridge_h = read('live/EnhancerEngineBridge.h')
bridge_cpp = read('live/EnhancerEngineBridge.cpp')
engine_h = read('live/LiveAudioEngine.h')
engine_cpp = read('live/LiveAudioEngine.cpp')
main_cpp = read('live/LiveMainComponent.cpp')

check('Application version 0.13.0', 'VERSION 0.13.0' in cmake and 'return "0.13.0"' in read('live/LiveApplication.cpp'))
check('Thirty-day trial', 'betaDurationDays = 30' in license_h)
check('Final seven-day warning', 'remainingDays <= 7 ? State::BetaExpiring' in license_cpp)
check('Product-specific state salt', 'SONKUPIK-LIVE-LOCAL-STATE' in license_cpp)
check('Product-specific fingerprint', 'SONKUPIK-LIVE-HARDWARE-FINGERPRINT-V1' in license_cpp)
check('Separate registry storage', 'MasArray\\\\Sonkupik Live' in license_cpp)
check('Separate application-data storage', '.getChildFile("Sonkupik Live")' in license_cpp)
check('Request ID stays server-compatible', 'return "ASKP-R1-"' in license_cpp)
check('Local RSA signature verification retained', 'juce::RSAKey publicKey' in license_cpp and 'kRsaPublicKey' in license_cpp)
check('No private RSA key embedded', 'PRIVATE KEY' not in ''.join([license_cpp, checkout_cpp, panel_cpp]).upper())
check('QRIS client source compiled', 'live/LicenseCheckoutClient.cpp' in cmake)
check('Native Live licence panel compiled', 'live/LiveLicensePanel.cpp' in cmake)
check('MasArray API endpoint', 'masarray-license-api.masari.workers.dev' in cmake and 'SONKUPIK_LICENSE_API_URL' in checkout_cpp)
check('Dedicated product ID', 'SONKUPIK_LICENSE_PRODUCT_ID="sonkupik-live"' in cmake and '"sonkupik-live"' in checkout_cpp)
check('Rp249000 product price', 'SONKUPIK_LICENSE_PRICE_IDR=249000' in cmake and 'fullVersionPriceIdr = SONKUPIK_LICENSE_PRICE_IDR' in panel_h)
check('Server price mismatch rejected', 'created.amountIdr != SONKUPIK_LICENSE_PRICE_IDR' in checkout_cpp)
check('Only trusted Midtrans checkout domains', 'app.midtrans.com' in checkout_cpp and 'app.sandbox.midtrans.com' in checkout_cpp)
check('Checkout worker is low priority', 'startThread(juce::Thread::Priority::low)' in checkout_cpp)
check('Checkout recovery persisted', 'checkout-state-v2.json' in checkout_cpp and 'order_token' in checkout_cpp)
check('Automatic payment polling', 'automaticPollingStartedAt_' in checkout_h and 'nextAutomaticPollDelayMs' in checkout_cpp)
check('Automatic activation delivery', 'takeActivationCode' in panel_cpp and 'license.activate' in panel_cpp)
check('Offline activation fallback', 'ACTIVATE OFFLINE CODE' in panel_h and 'COPY REQUEST ID' in panel_h)
check('Engine bridge gates creative parameters', bridge_cpp.count('if (! editingAllowed()) return;') >= 7)
check('Live engine exposes licence manager', 'getLicenseManager()' in engine_h and 'fullFeaturesAllowed()' in engine_h)
check('Trim changes gated after trial', engine_cpp.count('if (! fullFeaturesAllowed()) return;') >= 2)
check('Bypass changes gated after trial', engine_cpp.count('if (! fullFeaturesAllowed()) return;') >= 5)
check('Noise controls gated in UI', 'const bool noiseEnabled = fullFeatures' in main_cpp and 'noiseBypass.setEnabled (fullFeatures)' in main_cpp)
check('Enhancer controls gated in UI', 'const bool enhancerEnabled = fullFeatures' in main_cpp)
check('Enhancer bypass disabled after trial', 'enhancerPower.setEnabled (fullFeatures);' in main_cpp)
check('Licence status in header', 'UNLOCK • Rp249K' in main_cpp and 'FULL VERSION' in main_cpp)
check('Expired trial opens purchase panel', 'if (! audioEngine.fullFeaturesAllowed())' in main_cpp and 'licensePanel.setVisible (true)' in main_cpp)
check('Audio callback has no checkout or licence network work', 'LicenseCheckoutClient' not in engine_cpp and 'license.activate' not in engine_cpp)
check('Server credentials absent', not re.search(r'(MIDTRANS_SERVER_KEY|SB-Mid-server|sk_live_|BEGIN RSA PRIVATE KEY)', '\n'.join([cmake, checkout_cpp, panel_cpp]), re.I))
check('Licence documentation', (ROOT/'docs/LICENSE_AND_QRIS_v0.13.0.md').exists())
check('Server product configuration documentation', (ROOT/'docs/LICENSE_SERVER_PRODUCT_CONFIG_v0.13.0.md').exists())

# Verify hash locks declared in CMake for the sensitive implementation files.
for rel in [
    'third_party/askp-engine-v0.5.19/src/LicenseManager.cpp',
    'third_party/askp-engine-v0.5.19/src/LicenseManager.h',
    'third_party/askp-engine-v0.5.19/src/LicensePublicKey.h',
    'live/LicenseCheckoutClient.cpp',
    'live/LiveLicensePanel.cpp',
]:
    digest = hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()
    check(f'Hash lock {rel}', digest in cmake)

passed = sum(ok for _, ok in checks)
failed = len(checks) - passed
lines = ['Sonkupik Live v0.13.0 licensing source audit', '', f'Checks : {len(checks)}', f'Passed : {passed}', f'Failed : {failed}', '']
lines.extend(f'[ {"PASS" if ok else "FAIL"} ] {name}' for name, ok in checks)
report = '\n'.join(lines) + '\n'
print(report)
(ROOT/'qa/v0130_licensing_audit.txt').write_text(report, encoding='utf-8')
raise SystemExit(1 if failed else 0)
