from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
chain = (ROOT / "cmake" / "GenerateSC220BroadcastState.cmake").read_text(encoding="utf-8")
routing = (ROOT / "cmake" / "SC220P3RoutingCore.cmake").read_text(encoding="utf-8")
input_ux = (ROOT / "cmake" / "SC220P3InputUx.cmake").read_text(encoding="utf-8")
loudness_pass = (ROOT / "cmake" / "SC220P3Loudness.cmake").read_text(encoding="utf-8")
ui_header = (ROOT / "live" / "SC220MainComponent.h").read_text(encoding="utf-8")
runtime_state = (ROOT / "live" / "SC220P3RuntimeState.h").read_text(encoding="utf-8")
loudness = (ROOT / "live" / "ProgrammeLoudnessIntegrator.h").read_text(encoding="utf-8")

generated_root = ROOT / "build-sc220" / "generated" / "sc220-monitor"
if not generated_root.exists():
    generated_root = ROOT / "generated" / "sc220-monitor"
generated_ui_path = generated_root / "SC220MainComponent_Monitor.cpp"
generated_engine_path = generated_root / "LiveAudioEngine_Monitor.cpp"
generated_ui = generated_ui_path.read_text(encoding="utf-8") if generated_ui_path.exists() else ""
generated_engine = generated_engine_path.read_text(encoding="utf-8") if generated_engine_path.exists() else ""

checks: list[tuple[str, bool]] = []

def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))

check(
    "P3 finalizer runs after responsive visualizer",
    "FinalizeSC220LiveUxP3.cmake" in chain
    and chain.rfind("FinalizeSC220LiveUxP3.cmake") > chain.rfind("RebuildSC220ImpactVisualizer.cmake"),
)
check(
    "PC Audio is explicit opt-in instead of hidden SYSTEM DEFAULT",
    all(token in routing for token in (
        "pcAudioEnabled.store (false)",
        "requested.isEmpty()",
        "systemDefaultLabel",
        "PC AUDIO OFF",
        "audio start respects PC Audio OFF",
    )),
)
check(
    "GO LIVE and Stream unmute revalidate capture routing",
    "GO LIVE routing check" in routing
    and "Stream MUTE routing check" in routing
    and routing.count("selectPcAudioEndpoint (audioEngine.getPcAudioEndpointName())") >= 2,
)
check(
    "Monitor conflict disables PC capture instead of allowing double monitoring",
    "direct-plus-DSP doubling/echo" in routing
    and "pcLoopback.stop();" in routing
    and "pcAudioEnabled.store (false);" in routing,
)
check(
    "direct DENOISE and LEARN controls exist on SC220 input",
    all(token in ui_header for token in (
        'noiseCleanButton { "DENOISE" }',
        'learnNoiseButton { "LEARN" }',
    ))
    and all(token in input_ux for token in (
        "DENOISE ON", "DENOISE OFF", "noise.startLearning (2.0)",
        "getLearningProgress", "FROZEN  ",
    )),
)
check(
    "normal programme path remains frozen-profile and zero spectral runtime",
    "noiseClean.shouldCaptureProfile()" in (ROOT / "cmake" / "FinalizeSC220NaturalLivePath.cmake").read_text(encoding="utf-8")
    and "getLatencySamples() const noexcept { return 0; }" in (ROOT / "live" / "MusicalNoiseClean.h").read_text(encoding="utf-8"),
)
check(
    "Dual Mono has two UI faders and two lock-free gain values",
    "sc220MonoLeftFader" in ui_header
    and "sc220MonoRightFader" in ui_header
    and "dualMonoLeftFaderDb" in runtime_state
    and "dualMonoRightFaderDb" in runtime_state
    and "voiceBlock.applyGainRamp (0, 0, numSamples" in input_ux
    and "voiceBlock.applyGainRamp (1, 0, numSamples" in input_ux,
)
check(
    "loudness integrator exposes true live 400 ms momentary LUFS",
    "float momentaryLufs() const noexcept" in loudness
    and "momentary = blockLufs" in loudness
    and "LUFS-M" in loudness_pass
    and "PROGRAM SILENCE" in loudness_pass,
)
check(
    "responsive 14-band visualizer remains untouched by P3",
    "RebuildSC220ImpactVisualizer.cmake" in chain
    and "14 BAND  /  30 FPS" not in routing + input_ux + loudness_pass,
)

if generated_ui or generated_engine:
    check(
        "generated product contains final P3 controls and routing",
        all(token in generated_ui + generated_engine for token in (
            "PC AUDIO OFF", "DENOISE ON", "sc220MonoLeftFader",
            "LUFS-M", "sc220P3RuntimeState", "direct-plus-DSP doubling/echo",
        )),
    )

failed = [name for name, ok in checks if not ok]
print("SC220 Live P3 hardware-driven UX and routing audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
