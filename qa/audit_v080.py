from __future__ import annotations

from pathlib import Path
import hashlib
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
checks: list[tuple[str, bool, str]] = []


def add(name: str, ok: bool, detail: str = "") -> None:
    checks.append((name, ok, detail))


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

cmake = text("CMakeLists.txt")
engine_h = text("live/LiveAudioEngine.h")
engine_cpp = text("live/LiveAudioEngine.cpp")
visual_h = text("live/LiveLiquidVisualizer.h")
visual_cpp = text("live/LiveLiquidVisualizer.cpp")
noise_h = text("live/MusicalNoiseClean.h")
noise_cpp = text("live/MusicalNoiseClean.cpp")
main_h = text("live/LiveMainComponent.h")
main_cpp = text("live/LiveMainComponent.cpp")
build_ps = text("scripts/build-live-windows.ps1")
lock = text("ASKP_AUTHORITY.lock")

add("Version is 0.8.0", "project(SonkupikLive VERSION 0.8.0" in cmake)
add("JUCE matches authority 8.0.14", "GIT_TAG        8.0.14" in cmake)
add("Authoritative source required", "Authoritative askp-vst source is required" in cmake and "FATAL_ERROR" in cmake and "vendored fallback" not in cmake)
add("Exact authority commit locked", "655874dee6ee69df7212b8073ab941f9607d8faa" in cmake and "version=0.5.14" in lock)
add("Forty-preset authority recorded", "preset_count=40" in lock)
add("Build auto-detects and syncs authority", "Resolve-AskpAuthority" in build_ps and "sync-askp-authority.ps1" in build_ps and "askp-vst" in build_ps)
add("No VST3 host in Live Safe link", "juce_audio_processors" not in cmake and "JUCE_PLUGINHOST_VST3" not in cmake)
add("No JUCE DSP dependency", "juce::juce_dsp" not in cmake)
add("DSP-only fast math", "/fp:fast" in cmake and "ASKP_DSP_SOURCE" in cmake)
add("Sphere gloss follows authority", "ASKP_SPHERE_GLOSS" in cmake and "_askp_asset_dir" in cmake)
add("Fonts follow authority without bundled binaries", "ASKP_FONT_REGULAR" in cmake and "_askp_font_dir" in cmake and not list(ROOT.glob("assets/fonts/*.ttf")))

add("Authoritative header selected through include path", '#include "arsonkupik_dsp.hpp"' in engine_h and '../engine/arsonkupik_dsp.hpp' not in engine_h)
add("Redundant pre-denoise detector copy removed", engine_cpp.count("detectorBuffer.copyFrom") == 1)
add("Meter scans fused into one loop", "float inPeakL = 0.0f" in engine_cpp and "getMagnitude" not in engine_cpp)

add("Liquid visualizer hot path is time-domain only", all(token not in visual_h + visual_cpp for token in ["juce::dsp::FFT", "FixedFft", "performRealOnly", "std::complex"]))
add("Liquid visualizer uses 4096 newest samples", "analysisSamples = 4096" in visual_h)
add("Liquid visualizer fixed 32-node lattice", "fluidNodes = 32" in visual_h)
add("Liquid visualizer fixed sparse pools", "bubbleCount = 8" in visual_h and "dropletCount = 12" in visual_h)
add("Static visual layers cached", "rebuildStaticLayers" in visual_cpp and "staticBack" in visual_h and "staticFront" in visual_h)
add("Visual idle sleep", "const bool asleep" in visual_cpp and "return false" in visual_cpp)
add("Visual refresh is 24 Hz", "startTimerHz (24)" in main_cpp)
add("Visual tap disabled in tray", "visualizer.setVisualEnabled (visible)" in main_cpp and "stopTimer" in main_cpp)
add("Visual queue mirrors VST 16k SPSC", "queueSize = 16384" in text("live/VisualScopeTap.h") and "droppedBlocks" in text("live/VisualScopeTap.h"))

add("Custom fixed FFT replaces JUCE FFT", "struct FixedFft" in noise_h and "juce::dsp::FFT" not in noise_h and "juce::dsp::FFT" not in noise_cpp)
add("FFT plan is prepared outside callback", "fixedFft.configure (fftSize)" in noise_cpp and "reconfigureIfNeeded" in noise_cpp)
add("Legacy GUI spectrum path removed", "displayCurrent" not in noise_h and "copyDisplaySpectrum" not in noise_cpp and "SpectrumDisplay" not in text("live/LiveWidgets.h"))
add("Increment-wrap FIFO in callback", "% outputFifoSize" not in noise_cpp and "% fftSize" not in noise_cpp)

add("Numeric preset index used", "params.preset_index = index" in engine_cpp)
add("VST wrapper external bypass contract", "params.external_bypass = true" in engine_cpp)
add("VST wrapper external limiter contract", "params.external_peak_limiter = true" in engine_cpp)
add("Prepared engine reset after config", engine_cpp.count("->reset();") >= 2)
add("Full engine rebuilds coalesced", "wait (28)" in engine_cpp and "roughly 35 Hz" in engine_cpp)
add("Triple prepared engine slots retained", "engineSlotCount = 3" in engine_h)
add("Callback peak telemetry", "callbackPeakPercent" in engine_h and "callbackPeakPercent" in main_cpp)

callback_match = re.search(
    r"void LiveAudioEngine::audioDeviceIOCallbackWithContext[\s\S]*?\n}\n\nvoid LiveAudioEngine::audioDeviceAboutToStart",
    engine_cpp,
)
callback = callback_match.group(0) if callback_match else ""
for forbidden in ["new ", "delete ", "ScopedLock", "CriticalSection", "File", "Xml", "sleep", "wait ("]:
    add(f"Audio callback excludes {forbidden.strip()}", forbidden not in callback, forbidden)

add("Main layout keeps left before chain", "BEFORE ENHANCER" in main_cpp and "inputTrimRow" in main_h)
add("Main layout keeps right after chain", "AFTER CHAIN" in main_cpp and "limiterRow" in main_h)
add("Technical settings stay in callout", "AudioSettingsCard" in main_cpp and "sampleRate" in main_cpp and "CallOutBox" in main_cpp)
add("Main workspace uses liquid hero", "LiveLiquidVisualizer visualizer" in main_h and "SpectrumDisplay spectrum" not in main_h)

failed = [c for c in checks if not c[1]]
lines = [
    "SONKUPIK LIVE v0.8.0 SOURCE AUDIT",
    "=================================",
    f"Checks : {len(checks)}",
    f"Passed : {len(checks) - len(failed)}",
    f"Failed : {len(failed)}",
    "",
]
for name, ok, detail in checks:
    lines.append(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" -- {detail}" if detail else ""))

report = "\n".join(lines) + "\n"
(ROOT / "qa" / "v080_source_audit.txt").write_text(report, encoding="utf-8")
print(report)
sys.exit(1 if failed else 0)
