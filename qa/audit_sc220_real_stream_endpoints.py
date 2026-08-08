from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PASS = ROOT / "cmake" / "UseSC220RealStreamEndpoints.cmake"
CHAIN = ROOT / "cmake" / "GenerateSC220BroadcastState.cmake"
POLISH = ROOT / "cmake" / "PolishSC220FinalUi.cmake"

stream_pass = PASS.read_text(encoding="utf-8")
chain = CHAIN.read_text(encoding="utf-8")
polish = POLISH.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("real Stream Output generator exists", PASS.exists())
check(
    "real endpoint pass runs after final UI polish",
    'include("${CMAKE_CURRENT_LIST_DIR}/PolishSC220FinalUi.cmake")' in chain
    and 'include("${CMAKE_CURRENT_LIST_DIR}/UseSC220RealStreamEndpoints.cmake")' in chain
    and chain.index("PolishSC220FinalUi.cmake")
    < chain.index("UseSC220RealStreamEndpoints.cmake"),
)
check(
    "intermediate synthetic alias is explicitly replaced",
    'i == 0 ? "VB-CABLE"' in stream_pass
    and '"VB-CABLE " + juce::String (i + 1)' in stream_pass,
)
check(
    "final menu displays exact Windows friendly names",
    "outputBox.addItemList (streamOutputs, 1)" in stream_pass,
)
check(
    "stream names are preserved and deduplicated",
    "streamOutputs.addIfNotAlreadyThere (name)" in stream_pass
    and "outputBox.getText().trim()" in stream_pass,
)
check(
    "installed virtual render sinks remain discoverable",
    all(token in stream_pass for token in (
        'name.containsIgnoreCase ("CABLE")',
        'name.containsIgnoreCase ("VOICEMEETER INPUT")',
        'name.containsIgnoreCase ("VIRTUAL AUDIO")',
    )),
)
check(
    "device removal refreshes the exact endpoint list",
    all(token in stream_pass for token in (
        "streamListChanged",
        "outputBox.clear (juce::dontSendNotification)",
        "outputBox.addItemList (streamOutputs, 1)",
        "endpointStillAvailable",
        "Stream device changed",
    )),
)
check(
    "selected endpoint identity remains visible",
    "outputBox.setTooltip (selectedEndpoint)" in stream_pass
    and "outputBox.setTooltip (streamOutputs[outputIndex])" in stream_pass,
)
check(
    "final generator rejects numbered synthetic labels",
    all(token in stream_pass for token in (
        "VB-CABLE 2",
        "VB-CABLE 3",
        "generated UI still contains a synthetic Stream Output alias",
    )),
)
check(
    "legacy alias source is guarded by later replacement pass",
    'i == 0 ? "VB-CABLE"' in polish
    and "UseSC220RealStreamEndpoints.cmake" in chain,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live real Windows Stream Output endpoint audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
