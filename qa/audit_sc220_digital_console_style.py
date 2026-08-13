from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
P9 = (ROOT / "cmake/SC220P9DigitalConsoleStyle.cmake").read_text(encoding="utf-8")
PIPELINE = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
GENERATED_PATH = ROOT / "build-sc220/generated/sc220-monitor/SC220MainComponent_Monitor.cpp"
GENERATED = GENERATED_PATH.read_text(encoding="utf-8") if GENERATED_PATH.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "P9 is the final presentation authority",
    PIPELINE.rfind("SC220P9DigitalConsoleStyle.cmake")
    > PIPELINE.rfind("SC220P8PremiumAlignment.cmake"),
)
check(
    "graphite chassis replaces the mixed cream palette",
    all(token in SOURCE for token in (
        "const auto ivory = juce::Colour (0xff10151a)",
        "const auto surface = juce::Colour (0xff151b21)",
        "const auto header = juce::Colour (0xff090d11)",
        "const auto screen = juce::Colour (0xff080d11)",
    )),
)
check(
    "cyan is the primary digital-console accent",
    "const auto cyan = juce::Colour (0xff35c4d8)" in SOURCE
    and "palette::cyan.withAlpha (alpha)" in SOURCE
    and "palette::cyan.withAlpha (0.78f)" in SOURCE
    and "juce::ComboBox::textColourId, palette::cyan" in SOURCE,
)
check(
    "button and selector language uses inset dark hardware surfaces",
    "button.getToggleState() ? palette::cyanDeep : palette::surfaceAlt" in SOURCE
    and "palette::cyanDeep" in SOURCE
    and "juce::Colours::black.withAlpha (0.38f)" in SOURCE,
)
check(
    "SC220 and PC Audio use matched channel bays",
    "const auto sc220ChannelBay" in P9
    and "const auto pcChannelBay" in P9,
)
check(
    "enhancer uses one header screen and one macro console bay",
    "const auto enhancerHeaderBay" in P9
    and "const auto macroConsoleBay" in P9,
)
check(
    "Stream and Monitor use matched output bays",
    "const auto streamOutputBay" in P9
    and "const auto monitorOutputBay" in P9,
)
check(
    "illuminated pads remain part of the shared console language",
    "drawIlluminatedPad" in GENERATED
    and "dspPadHeight = static_cast<float> (enhancerPowerButton.getHeight())" in GENERATED,
)
check(
    "post-master analyzer remains a dark cyan-edged screen",
    "g.setColour (palette::screen);" in GENERATED
    and "palette::cyanDeep.withAlpha (0.72f)" in GENERATED,
)
check(
    "final generated source contains the dark console and no legacy cream palette",
    bool(GENERATED)
    and "const auto sc220ChannelBay" in GENERATED
    and "const auto streamOutputBay" in GENERATED
    and "0xfff4f1ea" not in GENERATED
    and "0xfffbfaf7" not in GENERATED,
)

failed = [name for name, ok in checks if not ok]
print("SC220 premium digital-console visual-language audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
