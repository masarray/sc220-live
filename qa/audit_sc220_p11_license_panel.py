from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "live/LiveLicensePanel.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "live/LiveLicensePanel.h").read_text(encoding="utf-8")
MAIN_SOURCE = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
P6E = (ROOT / "cmake/SC220P6ETrayReliabilityAndSoftwareTrial.cmake").read_text(
    encoding="utf-8"
)
GENERATED_PATH = (
    ROOT / "build-sc220/generated/sc220-p6e/LiveLicensePanel_P6E.cpp"
)
GENERATED = GENERATED_PATH.read_text(encoding="utf-8") if GENERATED_PATH.exists() else ""


checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "calm embedded-font hierarchy replaces legacy Jakarta styling",
    "juce::FontOptions { size }" in SOURCE
    and "juce::FontOptions { 12.0f }" in SOURCE
    and "AskpLookAndFeel::jakarta" not in SOURCE
    and "setLabel (title, 22.0f" in SOURCE
    and "setLabel (subtitle, 12.5f" in SOURCE,
)
check(
    "embedded Inter look-and-feel preserves each label's intended hierarchy",
    "const auto requested = label.getFont()" in MAIN_SOURCE
    and "requested.getHeight()" in MAIN_SOURCE
    and "return interFont (height, requested.isBold())" in MAIN_SOURCE,
)
check(
    "licence card is a balanced 900 by 620 two-column workspace",
    "juce::jmin (900, area.getWidth())" in SOURCE
    and "juce::jmin (620, area.getHeight())" in SOURCE
    and SOURCE.count("constexpr int columnGap = 22") == 2
    and "auto purchasePanel" in SOURCE
    and "const auto offlinePanel" in SOURCE
    and "auto purchaseColumn" in SOURCE
    and "auto offlineColumn" in SOURCE,
)
check(
    "purchase and offline activation have explicit section hierarchy",
    "juce::Label purchaseLabel" in HEADER
    and 'purchaseLabel.setText ("ONLINE PURCHASE"' in SOURCE
    and 'requestLabel.setText ("OFFLINE ACTIVATION  /  COMPUTER REQUEST ID"' in SOURCE
    and 'activationLabel.setText ("SIGNED ACTIVATION CODE"' in SOURCE
    and "const std::array<juce::Component*, 22>" in SOURCE,
)
check(
    "privacy copy is concise, readable and preserves the audio-local promise",
    "privacyNote.setMinimumHorizontalScale (0.82f)" in SOURCE
    and 'privacyNote.setText ("MIDTRANS SECURE PAYMENT\\n' in SOURCE
    and "Audio is never uploaded." in SOURCE,
)
check(
    "final generated panel uses concise SC220-aware title and guidance",
    '"LICENSE & ACTIVATION"' in GENERATED
    and '"Choose secure QRIS payment or enter a signed offline activation code."' in GENERATED
    and "constexpr int columnGap = 22" in GENERATED
    and "purchaseLabel.setBounds" in GENERATED
    and "privacyNote.setBounds" in GENERATED,
)
check(
    "P6E keeps the generated licence copy authoritative",
    '"LICENSE & ACTIVATION"' in P6E
    and '"Choose secure QRIS payment or enter a signed offline activation code."' in P6E,
)

failed = [name for name, ok in checks if not ok]
print("SC220 P11 licence activation layout audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
