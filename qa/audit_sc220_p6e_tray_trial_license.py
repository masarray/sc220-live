from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6E = (ROOT / "cmake" / "SC220P6ETrayReliabilityAndSoftwareTrial.cmake").read_text(encoding="utf-8")
BRIDGE = (ROOT / "live" / "EnhancerEngineBridge.h").read_text(encoding="utf-8")
WATCHER = (ROOT / "live" / "SC220DeviceWatcher.h").read_text(encoding="utf-8")
CHECKOUT = (ROOT / "live" / "LicenseCheckoutClient.cpp").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6E authority missing {label}: {needle}")


require(P6D, 'include("${CMAKE_CURRENT_LIST_DIR}/SC220P6ETrayReliabilityAndSoftwareTrial.cmake")',
        "P6E chain after P6D")

for needle, label in [
    ("private juce::Timer", "tray retry timer"),
    ("juce::Image::ARGB, 32, 32, true", "native-sized tray surface"),
    ("setIconImage (trayImage, {})", "Windows full-colour tray registration"),
    ("startTimer (1200)", "Explorer startup retry"),
    ("betaDurationDays = 90", "90-day generated software-only trial"),
    ("audioEngine.fullFeaturesAllowed()", "hardware-aware creative controls"),
    ("90-DAY SOFTWARE TRIAL ENDED", "expired software-only copy"),
    ("SC220 HARDWARE ACCESS", "hardware entitlement copy"),
    ("softwareOnlyExpired", "automatic software-only lock"),
    ("audioEngine.setOutputMuted (true)", "expired stream safety lock"),
    ("BUY SOFTWARE-ONLY LICENCE - QRIS", "payment CTA"),
]:
    require(P6E, needle, label)

for needle, label in [
    ("setHardwareEntitled", "bridge hardware entitlement setter"),
    ("hardwareEntitled_", "bridge hardware entitlement state"),
    ("isHardwareEntitled() || processor_.licenseManager.editingAllowed()",
     "SC220-or-licence editing rule"),
]:
    require(BRIDGE, needle, label)

for needle, label in [
    ("getVoiceEnhancerBridge().setHardwareEntitled (presentNow)", "voice entitlement binding"),
    ("getMusicEnhancerBridge().setHardwareEntitled (presentNow)", "music entitlement binding"),
]:
    require(WATCHER, needle, label)

# P6E must reuse the already-reviewed payment backend rather than inventing a
# second purchase transport in the UI layer.
for needle, label in [
    ('"/v1/checkout"', "licence checkout API"),
    ("Midtrans QRIS", "Midtrans purchase flow"),
    ("/refresh", "server-side payment verification"),
]:
    require(CHECKOUT, needle, label)

if "askp_app_logo_png" in P6E:
    raise AssertionError("P6E may not reintroduce the ASK-P/VST tray icon")

if "setVisible (true)" in P6E and "licensePanel->setVisible (true)" not in P6E:
    raise AssertionError("SystemTrayIconComponent must not be made visible as a normal Component")

print("SC220 P6E tray reliability + 90-day software-only licence audit passed")
