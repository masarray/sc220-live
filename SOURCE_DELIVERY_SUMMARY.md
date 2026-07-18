# Sonkupik Live v0.8.2 — Source Delivery Summary

## Delivered product

A native C++17/JUCE Windows application that routes a physical audio-interface input through Musical Noise Clean, the authoritative ArSonKuPik Smart Enhancer, a smooth expander and locked output protection to VB-CABLE.

## v0.8.0 audit result

The requested `askp-vstvisualizer` reference was not exposed as a separate connected repository. The current visualizer history, DSP and 40-preset bank were verified in `masarray/askp-vst` at commit:

```text
655874dee6ee69df7212b8073ab941f9607d8faa
```

Sonkupik Live now builds the DSP and presets directly from that checkout. A stale local header can no longer shadow the authority because `LiveAudioEngine.h` includes `arsonkupik_dsp.hpp` through CMake's authoritative include directory.

## v0.8.2 runtime UI fix

The application now activates the main workspace timer and visual tap explicitly from `showMainWindow()` and disables them from `hideMainWindow()`. This removes a JUCE construction-time visibility race that left the status footer, meters and liquid sphere frozen even though the audio callback was running.

## Implemented lightweight changes

- Replaced the denoise spectrum hero with a time-domain liquid sphere based on the current VST architecture.
- Added exact-size fixed pools: 32 liquid nodes, 8 bubbles and 12 droplets.
- Added a 16,384-sample lock-free SPSC visual queue matching the current VST strategy.
- Limited visual analysis to the newest 4096 samples at 24 Hz.
- Cached all static glass/rim/gloss layers and suspended visual work in tray mode.
- Removed the legacy GUI spectrum class and 256 per-frame display atomics.
- Replaced JUCE FFT in Musical Noise Clean with a preplanned per-instance fixed radix-2 transform.
- Removed `juce_dsp`, `juce_audio_processors` and dormant VST-host linkage from the Live Safe target.
- Adopted current engine runtime fields: numeric preset index, preset Smart Protect, external bypass and external peak-limiter ownership.
- Configured and reset every prepared engine in the same order as the current VST wrapper.
- Kept triple prepared-engine slots and click-free crossfade; full engine rebuilds are coalesced to roughly 35 Hz during knob drag.
- Removed one redundant full-buffer detector copy.
- Fused four level-meter scans into one pass.
- Added average and decaying callback-peak telemetry.
- Confined `/fp:fast` to the authoritative DSP translation unit in Release builds.

## Validation completed here

- 44/44 structural, authority, layout and realtime source checks passed.
- Fixed 256/512 transform was compared numerically against a reference DFT/FFT implementation with floating-point error near machine precision.
- Legacy spectrum/display publishing symbols were removed.
- Audio callback static audit found no heap allocation, lock, file/XML operation, sleep or wait call.

A full Windows MSVC/JUCE build and long-duration audio soak test must still run on the target machine.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-live-windows.ps1 -Clean
```

When no local authority checkout is found, the script runs `scripts/sync-askp-authority.ps1`. Git Credential Manager may request access to the private repository.
