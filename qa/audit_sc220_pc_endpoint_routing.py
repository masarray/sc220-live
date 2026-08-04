from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "live/PcAudioLoopbackCapture.h").read_text(encoding="utf-8")
source = (ROOT / "live/PcAudioLoopbackCapture.cpp").read_text(encoding="utf-8")
engine_h = (ROOT / "live/LiveAudioEngine.h").read_text(encoding="utf-8")
generator = (ROOT / "cmake/GenerateSC220PcEndpointRouting.cmake").read_text(encoding="utf-8")
stabilizer = (ROOT / "cmake/StabilizeSC220PcMusicMode.cmake").read_text(encoding="utf-8")
state_generator = (ROOT / "cmake/GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")

generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"
if not generated_root.exists():
    generated_root = ROOT / "generated" / "sc220-monitor"
generated_engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
generated_app_path = generated_root / "LiveApplication_BroadcastState.cpp"
generated_engine = generated_engine_path.read_text(encoding="utf-8") if generated_engine_path.exists() else ""
generated_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else ""
generated_app = generated_app_path.read_text(encoding="utf-8") if generated_app_path.exists() else ""

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "Windows render endpoints are enumerated with stable IDs and friendly names",
    all(token in source for token in (
        "EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE",
        "PKEY_Device_FriendlyName",
        "getDeviceId",
        "getDeviceFriendlyName",
        "EndpointInfo",
    )),
)
check(
    "Windows property-key definitions are available before endpoint keys",
    "#include <propkeydef.h>" in source
    and source.find("#include <propkeydef.h>")
    < source.find("#include <functiondiscoverykeys_devpkey.h>"),
)
check(
    "WRL ComPtr endpoints use explicit Get access before dereference",
    "getDeviceId (*defaultEndpoint.Get())" in source
    and "getDeviceId (*endpoint.Get())" in source
    and "getDeviceFriendlyName (*endpoint.Get())" in source
    and "getDeviceId (*defaultEndpoint)" not in source,
)
check(
    "SYSTEM DEFAULT and explicit endpoint selection are retained across restarts",
    all(token in header + source for token in (
        'systemDefaultLabel = "SYSTEM DEFAULT"',
        "selectEndpoint",
        "requestedEndpointId",
        "requestedEndpointFriendlyName",
        "requestedEndpointName()",
        "selectedEndpointName()",
    )),
)
check(
    "capture thread opens the selected endpoint ID instead of only the Windows default",
    all(token in source for token in (
        "requestedId = requestedEndpointId",
        "enumerator->GetDevice",
        "GetDefaultAudioEndpoint (eRender, eConsole",
        "activeEndpointId = endpointId",
        "activeEndpointFriendlyName = endpointName",
    )),
)
check(
    "endpoint conflict primitives remain available",
    all(token in source for token in (
        "endpointNamesMatch",
        "endpointWouldConflict",
        "isUnsafeVirtualEndpoint",
        'containsIgnoreCase ("CABLE Input")',
        'containsIgnoreCase ("CABLE Output")',
        'containsIgnoreCase ("VB-CABLE")',
    )),
)
check(
    "LiveAudioEngine exposes PC endpoint selection and routing telemetry",
    all(token in engine_h for token in (
        "getPcAudioEndpointNames",
        "selectPcAudioEndpoint",
        "getPcAudioEndpointName",
        "pcAudioLoopBlocked",
        "pcAudioDevice",
        "pcAudioEndpointId",
        "pcAudioError",
    )),
)
check(
    "PC endpoint and stabilizer generators run in the intended order",
    'include("${CMAKE_CURRENT_LIST_DIR}/GenerateSC220PcEndpointRouting.cmake")' in state_generator
    and 'include("${CMAKE_CURRENT_LIST_DIR}/StabilizeSC220PcMusicMode.cmake")' in state_generator
    and state_generator.find("GenerateSC220PcEndpointRouting.cmake")
    < state_generator.find("PolishSC220FinalUi.cmake")
    < state_generator.find("StabilizeSC220PcMusicMode.cmake"),
)
check(
    "generated engine allows safe virtual sources and blocks the same Stream cable",
    all(token in generated_engine for token in (
        "getPcAudioEndpointNames",
        "selectPcAudioEndpoint",
        "pcEndpointConflictsWithStream",
        "virtualCableIdentity",
        "sourceCable.isNotEmpty() && sourceCable == streamCable",
        "PC Audio cannot capture the same cable used by Stream Output",
        "PC Audio capture was blocked",
    ))
    and "isUnsafeVirtualEndpoint (endpoint.name)" not in generated_engine[
        generated_engine.find("juce::StringArray LiveAudioEngine::getPcAudioEndpointNames"):
        generated_engine.find("juce::String LiveAudioEngine::selectPcAudioEndpoint")
    ]
    and "same cable" in stabilizer.lower(),
)
check(
    "generated UI provides a real PC playback endpoint selector",
    all(token in generated_ui for token in (
        "audioEngine.getPcAudioEndpointNames()",
        'pcSourceBox.setTextWhenNothingSelected ("SELECT PC AUDIO")',
        "audioEngine.selectPcAudioEndpoint",
        '"Unsafe PC Audio routing"',
        '"PLAYBACK CAPTURE ACTIVE"',
        '"ROUTING BLOCKED"',
    )),
)
check(
    "PC endpoint selection is restored before the audio graph initialises",
    "selectPcAudioEndpoint" in generated_app
    and generated_app.find("selectPcAudioEndpoint")
    < generated_app.find("audioEngine->initialise (deviceState.get())"),
)
check(
    "PC endpoint selection is persisted on shutdown",
    'file->setValue ("pcAudioEndpoint", audioEngine->getPcAudioEndpointName())' in generated_app,
)
check(
    "generated endpoint sources exist for direct audit",
    generated_engine_path.exists() and generated_ui_path.exists() and generated_app_path.exists(),
)
check(
    "audio callback still only consumes the prepared FIFO",
    "pcLoopback.read (musicBlock, numSamples);" in generated_engine
    and "availableEndpoints()" not in generated_engine[
        generated_engine.find("audioDeviceIOCallbackWithContext"):
        generated_engine.find("audioDeviceAboutToStart")
    ],
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live selectable PC playback endpoint and anti-loop audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
