#!/usr/bin/env python3
"""Lightweight fail-closed regression checks; no private DSP or audio device required."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
def source(path):
    return (root / path).read_text(encoding="utf-8")
def check(value, why):
    if not value:
        raise AssertionError(why)

engine = source("live/LiveAudioEngine.cpp")
rack = source("live/InputVst3Rack.cpp")
ui = source("live/SC220MainComponent.cpp")
cmake = source("CMakeLists.txt")
ui_generator = source("cmake/GenerateSC220MonitorUi.cmake")
check("JUCE_ASIO=1" in cmake and "JUCE_PLUGINHOST_VST3=1" in cmake,
      "ASIO or VST3 host build switch missing")
check("live/InputVst3Rack.cpp" in cmake, "VST3 host source not linked")
check(engine.index("noiseClean.process (voiceBlock);") <
      engine.index("voiceVst3.process (voiceBlock, voiceMidi);") <
      engine.index("voiceEnhancerBridge.process (voiceBlock, voiceMidi);"),
      "Voice FX must be post-denoise and pre-enhancer")
check(engine.index("voiceVst3.process (voiceBlock, voiceMidi);") <
      engine.index("pcLoopback.read (musicBlock, numSamples);"),
      "Plugin routing accidentally reaches PC Music")
check("masterLimiter.process (masterContext);" in engine,
      "Protected master output was removed")
check("quiesceForPluginSwap()" in engine and "audioCallbacksInFlight" in engine
      and "vstSwapSuspended" in engine,
      "Prepared VST hot-swap must use a nonblocking audio-thread quiescence gate")
check("InputVst3Rack::createPrepared" in engine and "voiceVst3.install" in engine,
      "Plugin creation must finish before quiescing the audio callback")
check(engine.count("deviceManager.removeAudioCallback (this);") == 1,
      "Plugin swaps must not restart the entire device manager")
check("voiceVst3.prepare (sampleRate, blockSize);" in engine
      and "voiceVst3.release();" in engine,
      "Plugin device lifecycle incomplete")
check("currentSampleRate.store (sampleRate);" in engine
      and "currentBufferSize.store (blockSize);" in engine,
      "Device format telemetry is stale")
check("voiceVst3.getLatencySamples()" in engine, "Missing FX latency report")
check("setBusesLayout (layout)" in rack
      and "getTotalNumOutputChannels() != 2" in rack,
      "Plugin stereo bus validation missing")
check("std::isfinite" in rack, "Non-finite VST3 outputs must not poison the mixer")
monitor = source("live/MonitorOutputClient.cpp")
push = monitor.split("void MonitorOutputClient::push (", 1)[1].split(
    "MonitorOutputClient::Status MonitorOutputClient::getStatus()", 1)[0]
check("enqueueFromAudioThread" in push and "GetBuffer" not in push
      and "AcquireSRWLock" not in push and "ensureComForCurrentThread" not in push,
      "Monitor callback path must only publish SPSC samples")
check("void run() override" in monitor and "renderAvailableLocked" in monitor,
      "WASAPI rendering/reconnect needs a dedicated worker")
check("const int discard = requested - freeSpace" not in monitor,
      "FIFO producer must never move the consumer read cursor")
noise = source("live/MusicalNoiseClean.cpp")
capture = noise.split("void MusicalNoiseClean::process (", 1)[1].split(
    "void SmoothDownwardExpander::prepare", 1)[0]
check("buffer.setSample" not in capture and "buffer.copyFrom" not in capture,
      "Learn Static Noise must not modify or delay live voice")
check("voiceFxEditorWindow.reset();" in ui and "showVoiceVst3Editor()" in ui,
      "Voice FX editor teardown absent")
check("&voiceFxButton" in ui_generator, "Generated UI omitted input FX button")
for path in ("SETUP-LOCAL-RUNNER.cmd", "scripts/setup-sc220-local-runner.ps1",
             "scripts/configure-sc220-runner-service.ps1",
             "scripts/verify-sc220-runner-host.ps1"):
    check(not (root / path).exists(), "Self-hosted runner setup left in public source: " + path)
workflow_dir = root / ".github/workflows"
if workflow_dir.exists():
    for f in list(workflow_dir.glob("*.yml")) + list(workflow_dir.glob("*.yaml")):
        check(not re.search(r"self-hosted", f.read_text(encoding="utf-8"), re.I),
              "Self-hosted workflow runner remains: " + f.name)
print("PASS: ASIO, voice-only VST3 chain, generated UI and runner cleanup")
