from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/SC220MainComponent.h").read_text(encoding="utf-8")
source = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
band_telemetry = (ROOT / "live/ProgrammeBandTelemetry.h").read_text(encoding="utf-8")
release_pass = (ROOT / "cmake/RefineSC220ReleaseUi.cmake").read_text(encoding="utf-8")
generated_root = ROOT / "generated" / "sc220-monitor"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
final_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else source

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("custom titlebar supports window dragging", all(token in header + source for token in (
    "juce::ComponentDragger windowDragger", "void SC220MainComponent::mouseDown",
    "void SC220MainComponent::mouseDrag", "startDraggingComponent", "dragComponent")))
check("header double-click toggles maximise", all(token in source for token in (
    "void SC220MainComponent::mouseDoubleClick", "windowActions.toggleMaximise")))
check("titlebar uses Lucide minimise maximise close", all(token in header for token in (
    'LucideIconButton minimiseButton { "Minimise", "minimize_2_svg"',
    'LucideIconButton maximiseButton { "Maximise", "maximize_2_svg"',
    'LucideIconButton closeButton { "Close", "x_svg"')))
check("settings uses Lucide icon", 'LucideIconButton settingsButton { "Settings", "settings_svg"' in header)
check("Smart Enhancer power icon exists", all(token in header + source for token in (
    'LucideIconButton enhancerPowerButton { "Smart Enhancer", "power_svg"',
    "enhancerPowerButton.setClickingTogglesState",
    "audioEngine.setBypassed (! enhancerPowerButton.getToggleState())")))
check("final UI renders the rhythmic atomic programme analyzer", all(token in final_ui for token in (
    "class SC220MainComponent::LiveSpectrumDisplay", "updateFromTelemetry",
    "programmeBandTelemetry.read", "bandCount = ProgrammeBandTelemetry::bandCount",
    "segmentCount = 16", "pulses[band]", "positiveRise", '"PROGRAM ANALYZER"')))
check("pink calibration remains an engine contract rather than release copy", all(token in band_telemetry for token in (
    "pinkCalibrationDb", "calibratePinkNoise", "pinkNoiseWeight"))
    and '"PINK-FLAT BANDS' not in final_ui
    and '"WAITING FOR ENGINE BANDS"' not in final_ui
    and '"WAITING FOR POST-LIMITER AUDIO"' not in final_ui)
check("all failed visual transport paths are absent", all(token not in final_ui for token in (
    "ProgrammeSpectrumSnapshot", "programmeSpectrumSnapshot",
    "performFrequencyOnlyForwardTransform", "updateFromSnapshot", "updateFromTaps",
    "programmeTap.consumeLatest", "musicTap.consumeLatest", "voiceTap.consumeLatest",
    "PcAudioLoopbackCapture::consumeLatestVisual", "MUSIC RECOVERY",
    "VOICE RECOVERY", "PC CAPTURE", "NO FRAMES")))
check("main surface uses native Windows premium typography", 'font.setTypefaceName ("Segoe UI Variable")' in source)
check("hardware controls match approved dot-calibrated metal surface", all(token in source for token in (
    "for (int i = 0; i <= 20; ++i)", "const auto dotSize",
    "g.fillEllipse (dot.x - dotSize * 0.5f", "palette::metalHi",
    "palette::metalMid", "drawFaderScale", "capGradient.addColour",
    "sliderPos - 34.0f, 40.0f, 68.0f", "const auto accentStart = endAngle - 0.38f")))
check("full-height fader caps remain inside endpoint bounds", all(token in source for token in (
    "getSliderThumbRadius", "slider.getSliderStyle() == juce::Slider::LinearVertical",
    "return 34;", "sliderPos - 34.0f, 40.0f, 68.0f")))
check("macro controls keep names and current values but remove endpoint clutter", all(token in final_ui for token in (
    '"ENHANCE", "SMART BASS", "SMART TREBLE", "VOCAL BODY", "STEREO MAGIC"',
    'juce::String (knob.getValue(), 0) + "%"'))
    and 'g.drawText ("0%"' not in final_ui
    and 'g.drawText ("100%"' not in final_ui)
check("SC220 device art uses a focused source crop", all(token in source for token in (
    "const int sourceY", "const int sourceWidth", "g.drawImage (sc220Image")))
check("free-days button removed from main surface", all(token not in header + source for token in (
    "licenseButton", "FREE 365 DAYS")))
check("license remains available from Settings", all(token in source for token in (
    'menu.addItem (2, "License & Activation")', "licensePanel->setVisible (true)")))
check("SC220 detection is based on real endpoint name", all(token in source for token in (
    "looksLikeSc220Input", 'normalised == "MIXER"',
    'normalised.startsWith ("MIXER (")', '"SC220 NOT DETECTED"')))
check("fake unconditional SC220 status removed", 'g.drawText ("SC220 CONNECTED"' not in source)
check("Details button removed", all(token not in header + source for token in (
    "detailsButton", '"DETAILS"', "SC220 Live Details")))
check("output READY label remains removed", '"READY" : "SELECT VB-CABLE"' not in source)
check("old time-history visualizer is absent", all(token not in final_ui for token in (
    "PROGRAM IMPACT", "PC INPUT IMPACT", '"OLDER"', '"NOW"', "historySize = 32")))
check("release UI removes platform and engineering noise copy", all(token not in final_ui for token in (
    "YT / TIKTOK", "TP <= -1", "MAS ARI SIGNATURE ENGINE",
    "14 BAND  /  30 FPS", "SC220 + PC AUDIO", "VOICE / MUSIC SIGNATURE",
    "SMART PROTECT  /  ON", "AUTO DUCK  /  SC220 KEY",
    "VB-CABLE / OBS", "HEADPHONES / SPEAKERS", "ACTIVE: ")))
check("release UI retains only essential operational hierarchy", all(token in final_ui for token in (
    '"INPUT MIX"', '"PC AUDIO"', '"SMART ENHANCER"', '"PROGRAM ANALYZER"',
    '"LOUDNESS"', '"STREAM OUTPUT"', '"MONITOR OUTPUT"', '"PROTECTED"',
    '"PLAYBACK CAPTURE ACTIVE"', '"MONITOR ACTIVE"', '"LUFS-I"')))
check("release refinement is chained after meter calibration", all(token in release_pass for token in (
    "premium lean release UI", "remove platform target legend",
    "remove analyzer debug generation and fps text",
    "remove footer signal-chain breadcrumb")))

failed = [name for name, ok in checks if not ok]
print("SC220 Live premium lean release UX contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
