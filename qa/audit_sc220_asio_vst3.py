#!/usr/bin/env python3
"""Public SC220 source-only CI gates: no private DSP checkout or local runner."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
def source(name): return (root/name).read_text(encoding="utf-8")
def require(v,msg):
    if not v: raise AssertionError(msg)

cmake=source("CMakeLists.txt")
bridge=source("live/EnhancerEngineBridge.cpp")
header=source("live/EnhancerEngineBridge.h")
engine=source("live/LiveAudioEngine.cpp")
monitor=source("live/MonitorOutputClient.cpp")
rack=source("live/InputVst3Rack.cpp")
noise=source("live/MusicalNoiseClean.cpp")
require("JUCE_ASIO=1" in cmake and "JUCE_PLUGINHOST_VST3=1" in cmake,
        "Windows ASIO/VST3 host support missing")
require("askp-vst" not in cmake and "ASKP_DSP_SOURCE" not in cmake,
        "Build must not require the proprietary ASK-P checkout")
require("LicenseManager.cpp" in cmake and "EnhancerEngineBridge.cpp" in cmake,
        "SC220 license and plugin facade must build from this repository")
require('#include "PluginProcessor.h"' not in header,
        "Private ASK-P processor header is forbidden in the runtime")
require("loadBundled" in bridge and "getParameters()" in bridge,
        "Voice and Music must load first-party binary plugin via parameter facade")
require("voiceEnhancerBridge.loadBundled" in engine
        and "musicEnhancerBridge.loadBundled" in engine,
        "First-party VST3 must be loaded independently for both audio lanes")
require("quiesceForPluginSwap" in engine,
        "Third-party voice insert must maintain audio callback quiescence gate")
push=monitor.split("void MonitorOutputClient::push (",1)[1].split(
    "MonitorOutputClient::Status MonitorOutputClient::getStatus()",1)[0]
require("enqueueFromAudioThread" in push and "AcquireSRWLock" not in push,
        "Monitor callback may only publish SPSC data")
require("void run() override" in monitor and "renderAvailableLocked" in monitor,
        "Physical monitor must render on its own worker")
require("buffer.setSample" not in noise.split("void MusicalNoiseClean::process (",1)[1].split(
    "void SmoothDownwardExpander::prepare",1)[0], "Noise Learn must not delay dry voice")
require("getTotalNumOutputChannels() != 2" in rack,
        "Require a stereo VST3 output bus")
for f in list((root/".github/workflows").glob("*.yml")):
    require(not re.search("self-hosted",source(str(f.relative_to(root))),re.I),
            "Public CI must not use local runner")
print("PASS: PUBLIC SC220 VST3 host, source independence, ASIO, SPSC realtime policies")
