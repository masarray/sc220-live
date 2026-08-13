from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

source = (ROOT / "live/SC220MainComponent.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
state = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
p8 = (ROOT / "cmake/SC220P8PremiumAlignment.cmake").read_text(encoding="utf-8")
generated_path = ROOT / "build-sc220/generated/sc220-monitor/SC220MainComponent_Monitor.cpp"
generated = generated_path.read_text(encoding="utf-8") if generated_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "Inter Regular Medium and SemiBold are embedded",
    all(token in cmake + source for token in (
        "assets/fonts/inter_regular.ttf",
        "assets/fonts/inter_medium.ttf",
        "assets/fonts/inter_semibold.ttf",
        "BinaryData::inter_regular_ttf",
        "BinaryData::inter_medium_ttf",
        "BinaryData::inter_semibold_ttf",
        "createSystemTypefaceFor",
        "setDefaultSansSerifTypeface",
    )),
)
check(
    "system font masquerading as Inter is removed",
    'font.setTypefaceName ("Segoe UI Variable")' not in source + generated,
)
check(
    "P8 remains the final presentation authority",
    "SC220P8PremiumAlignment.cmake" in state
    and state.rfind("SC220P8PremiumAlignment.cmake")
        > state.rfind("SC220P7CompactPleasant.cmake"),
)
check(
    "input header grid has explicit non-overlapping rows",
    all(token in generated for token in (
        "inputContent.removeFromTop (52)",
        "juce::jlimit (228, 270",
        "sourceSelectorY - 66, 64, 64",
        "sourceSelectorY + 86",
        "const int sc220StatusY = sc220StripArea.getY() + 68",
        "const int pcStatusY = pcStripArea.getY() + 68",
    )),
)
check(
    "output area receives a deliberate wider proportion",
    "area.getWidth() * 0.310f" in generated
    and "area.getWidth() * 0.420f" in generated,
)
check(
    "idle diagnostic noise is absent from the mixer surface",
    all(token not in generated for token in (
        "ACTIVE: ",
        "LEARN IN SILENCE",
        "AUTO DUCK  /  SC220 KEY",
        "VB-CABLE / OBS",
        "HEADPHONES / SPEAKERS",
    )),
)
check(
    "loudness is isolated above the aligned output header row",
    all(token in generated for token in (
        "masterArea.getY() + 34, 136, 136",
        'g.drawText ("STREAM OUTPUT", outputBox.getX(), outputBox.getY() - 27',
        'g.drawText ("MONITOR OUTPUT", monitorOutputBox.getX(), monitorOutputBox.getY() - 27',
        "const int outputDividerY = outputBox.getY() - 39",
    )),
)
check(
    "Stream and Monitor controls share a balanced two-column grid",
    all(token in generated for token in (
        "masterColumnGap = 14",
        "outputBox.setBounds (streamColumn.getCentreX() - selectorWidth / 2",
        "monitorOutputBox.setBounds (monitorColumn.getCentreX() - selectorWidth / 2",
        "monitorOutputBox.getBottom() + 12, 26, 26",
        "monitorKnob.setBounds (monitorColumn.getCentreX() - 41",
    )),
)
check(
    "headphone toggle is centred below its selector",
    "monitorPowerButton.setBounds (monitorColumn.getCentreX() - 13" in generated
    and "const auto monitorPowerPlate" in generated,
)
check(
    "macro endpoint labels do not repeat across the console",
    'g.drawText ("0%"' not in generated
    and 'g.drawText ("100%"' not in generated,
)
check(
    "presentation pass is fail-closed",
    "SC220 P8 alignment anchor changed" in p8
    and "SC220 P8 noisy or non-embedded UI authority remains" in p8,
)

failed = [name for name, ok in checks if not ok]
print("SC220 embedded Inter and premium alignment audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
