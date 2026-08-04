from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/SC220MainComponent.h").read_text(encoding="utf-8")
source = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
generated_root = ROOT / "generated" / "sc220-monitor"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
final_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else source

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("custom titlebar supports window dragging", all(token in header + source for token in (
    "juce::ComponentDragger windowDragger",
    "void SC220MainComponent::mouseDown",
    "void SC220MainComponent::mouseDrag",
    "startDraggingComponent",
    "dragComponent")))
check("header double-click toggles maximise", all(token in source for token in (
    "void SC220MainComponent::mouseDoubleClick",
    "windowActions.toggleMaximise")))
check("titlebar uses Lucide minimise maximise close", all(token in header for token in (
    'LucideIconButton minimiseButton { "Minimise", "minimize_2_svg"',
    'LucideIconButton maximiseButton { "Maximise", "maximize_2_svg"',
    'LucideIconButton closeButton { "Close", "x_svg"')))
check("settings uses Lucide icon", 'LucideIconButton settingsButton { "Settings", "settings_svg"' in header)
check("Smart Enhancer power icon exists", all(token in header + source for token in (
    'LucideIconButton enhancerPowerButton { "Smart Enhancer", "power_svg"',
    "enhancerPowerButton.setClickingTogglesState",
    "audioEngine.setBypassed (! enhancerPowerButton.getToggleState())")))
check("final generated UI renders fixed-frequency programme spectrum", all(token in final_ui for token in (
    "class SC220MainComponent::LiveSpectrumDisplay",
    "updateFromTaps",
    "performFrequencyOnlyForwardTransform",
    "bandCount = 24",
    "segmentCount = 15",
    '"PROGRAM SPECTRUM"')))
check("final visualizer has explicit runtime recovery sources", all(token in final_ui for token in (
    "programmeTap.consumeLatest",
    "musicTap.consumeLatest",
    "voiceTap.consumeLatest",
    'sourceText = "POST-MASTER"',
    'sourceText = "MUSIC RECOVERY"',
    'sourceText = "VOICE RECOVERY"')))
check("voice and music recovery taps remain enabled", all(token in final_ui for token in (
    "getVoiceEnhancerBridge().setVisualTapEnabled (true)",
    "getMusicEnhancerBridge().setVisualTapEnabled (true)")))
check("main surface uses native Windows premium typography", 'font.setTypefaceName ("Segoe UI Variable")' in source)
check("hardware controls match approved dot-calibrated metal surface", all(token in source for token in (
    "for (int i = 0; i <= 20; ++i)",
    "const auto dotSize",
    "g.fillEllipse (dot.x - dotSize * 0.5f",
    "palette::metalHi",
    "palette::metalMid",
    "drawFaderScale",
    "capGradient.addColour",
    "sliderPos - 34.0f, 40.0f, 68.0f",
    "const auto accentStart = endAngle - 0.38f",
)))
check("full-height fader caps remain inside their endpoint bounds", all(token in source for token in (
    "getSliderThumbRadius",
    "slider.getSliderStyle() == juce::Slider::LinearVertical",
    "return 34;",
    "sliderPos - 34.0f, 40.0f, 68.0f",
)))
check("macro controls preserve approved endpoint labels", all(token in source for token in (
    'g.drawText ("0%"',
    'g.drawText ("100%"',
    "knob.setTextBoxStyle (juce::Slider::NoTextBox",
)))
check("SC220 device art uses a focused source crop", all(token in source for token in (
    "const int sourceY",
    "const int sourceWidth",
    "g.drawImage (sc220Image",
)))
check("free-days button removed from main surface", all(token not in header + source for token in (
    "licenseButton", "FREE 365 DAYS")))
check("license remains available from Settings", all(token in source for token in (
    'menu.addItem (2, "License & Activation")',
    "licensePanel->setVisible (true)")))
check("SC220 detection is based on real endpoint name", all(token in source for token in (
    "looksLikeSc220Input",
    'normalised == "MIXER"',
    'normalised.startsWith ("MIXER (")',
    '"SC220 NOT DETECTED"')))
check("fake unconditional SC220 status removed", 'g.drawText ("SC220 CONNECTED"' not in source)
check("Details button removed", all(token not in header + source for token in (
    "detailsButton", '"DETAILS"', "SC220 Live Details")))
check("output READY label remains removed", '"READY" : "SELECT VB-CABLE"' not in source)
check("old time-history visualizer is absent from final UI", all(token not in final_ui for token in (
    "PROGRAM IMPACT", "PC INPUT IMPACT", '"OLDER"', '"NOW"', "historySize = 32")))

failed = [name for name, ok in checks if not ok]
print("SC220 Live premium UX contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
