from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P6D = (ROOT / "cmake" / "SC220P6DTrayPolish.cmake").read_text(encoding="utf-8")
P6C = (ROOT / "cmake" / "SC220P6CStatusChips.cmake").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"P6D authority missing {label}: {needle}")


# P6D must be chained after P6C and remain application/tray UI only.
if 'include("${CMAKE_CURRENT_LIST_DIR}/SC220P6DTrayPolish.cmake")' not in P6C:
    raise AssertionError("P6D must be chained after P6C")

for forbidden in [
    "SC220_ENGINE_GENERATED_SOURCE",
    "MusicalNoiseClean",
    "expander.",
    "noiseClean.",
    "selectPcAudioEndpoint",
    "selectMonitorOutputDevice",
]:
    if forbidden in P6D:
        raise AssertionError(f"P6D must remain tray/application UI only: {forbidden}")

for needle, label in [
    ('g.drawText ("PRESET", 146, 184, 148, 18', "unclipped PRESET caption"),
    ('restartButton.setBounds (146, 142, 154, 36)', "restart spacing"),
    ('presetBox.setBounds (146, 204, 154, 38)', "preset box spacing"),
    ('miniWindow->setSize (320, 324)', "larger compact controller"),
    ('BinaryData::getNamedResource ("favicon_96x96_png"', "primary SC220 compact-panel icon"),
    ('SC220 Live - running in background', "compact tray tooltip"),
    ('SC220P6ETrayReliabilityAndSoftwareTrial.cmake', "P6E tray reliability chain"),
    ('SC220P6FNativeTrayIcon.cmake', "P6F final ICO tray authority chain"),
]:
    require(P6D, needle, label)

# The legacy ASK-P/VST icon may appear only as the replacement anchor that P6D
# explicitly removes; it must never appear in a replacement side.
if P6D.count("askp_app_logo_png") != 1:
    raise AssertionError("P6D should reference the ASK-P tray icon exactly once as the stale anchor to remove")

print("SC220 P6D tray polish audit passed")
