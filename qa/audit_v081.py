from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
checks: list[tuple[str, bool, str]] = []


def add(name: str, ok: bool, detail: str = "") -> None:
    checks.append((name, ok, detail))


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

cmake = text("CMakeLists.txt")
noise_cpp = text("live/MusicalNoiseClean.cpp")
noise_h = text("live/MusicalNoiseClean.h")
app_cpp = text("live/LiveApplication.cpp")
tap_h = text("live/VisualScopeTap.h")
engine_cpp = text("live/LiveAudioEngine.cpp")
engine_h = text("live/LiveAudioEngine.h")
main_cpp = text("live/LiveMainComponent.cpp")
main_h = text("live/LiveMainComponent.h")
lock = text("ASKP_AUTHORITY.lock")

add("Version is 0.8.1", "project(SonkupikLive VERSION 0.8.1" in cmake)
add("Application version is 0.8.1", 'getApplicationVersion() override { return "0.8.1"; }' in app_cpp)
add("Exact ASK-P authority remains locked", "655874dee6ee69df7212b8073ab941f9607d8faa" in cmake and "version=0.5.14" in lock)
add("Stale displayProfile reference removed", "displayProfile" not in noise_cpp and "displayProfile" not in noise_h)
add("JUCE display API updated", "userBounds" in app_cpp and "userArea" not in app_cpp)
add("Intentional alignment warning is locally scoped", "#pragma warning(disable: 4324)" in tap_h and "#pragma warning(pop)" in tap_h)
add("Unused audio_utils linkage removed", "juce::juce_audio_utils" not in cmake)
add("No explicit audio_processors linkage", "juce::juce_audio_processors" not in cmake)
add("Direct audio device module retained", "juce::juce_audio_devices" in cmake)
add("No JUCE DSP dependency", "juce::juce_dsp" not in cmake)
add("Fixed FFT remains in denoiser", "struct FixedFft" in noise_h and "fixedFft.configure" in noise_cpp)
add("No JUCE FFT in denoiser", "juce::dsp::FFT" not in noise_h + noise_cpp)
add("Triple prepared engines retained", "engineSlotCount = 3" in engine_h)
add("Numeric preset index retained", "params.preset_index = index" in engine_cpp)
add("External bypass contract retained", "params.external_bypass = true" in engine_cpp)
add("External limiter contract retained", "params.external_peak_limiter = true" in engine_cpp)
add("Liquid visual remains central", "LiveLiquidVisualizer visualizer" in main_h)
add("Visual timer remains 24 Hz", "startTimerHz (24)" in main_cpp)

callback_match = re.search(
    r"void LiveAudioEngine::audioDeviceIOCallbackWithContext[\s\S]*?\n}\n\nvoid LiveAudioEngine::audioDeviceAboutToStart",
    engine_cpp,
)
callback = callback_match.group(0) if callback_match else ""
add("Audio callback found", bool(callback))
for forbidden in ["new ", "delete ", "ScopedLock", "CriticalSection", "File", "Xml", "sleep", "wait ("]:
    add(f"Audio callback excludes {forbidden.strip()}", forbidden not in callback, forbidden)

failed = [item for item in checks if not item[1]]
lines = [
    "SONKUPIK LIVE v0.8.1 SOURCE AUDIT",
    "=================================",
    f"Checks : {len(checks)}",
    f"Passed : {len(checks) - len(failed)}",
    f"Failed : {len(failed)}",
    "",
]
for name, ok, detail in checks:
    lines.append(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f" -- {detail}" if detail else ""))

report = "\n".join(lines) + "\n"
(ROOT / "qa" / "v081_source_audit.txt").write_text(report, encoding="utf-8")
print(report)
sys.exit(1 if failed else 0)
