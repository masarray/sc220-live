# Sonkupik Live v0.9.2 — Source Delivery Summary

## P1 result

The slow knob feel was traced to the authoritative VST processor's 80 ms creative macro glide. That value is appropriate for DAW automation but trails continuous mouse movement in a live rack.

Sonkupik Live now generates `PluginProcessor_Live.cpp` during CMake configuration and changes only that glide to **12 ms**. The build uses the same askp-vst engine, 40 presets, macro mapping, 400 Hz coefficient update ceiling, preset crossfade, WOW calibration, lookahead safety, bypass path, meters, and final ceiling.

## Expected control response at 48 kHz

- 128 samples: audible movement in the next 2.67 ms block; full 12 ms glide in about 13.3 ms.
- 256 samples: audible movement in the next 5.33 ms block; full glide in about 16 ms.
- 480 samples: first block reaches about 83% of the requested move; full glide by the second block, about 20 ms.
- 512 samples: full glide by the second block, about 21.3 ms.

Static sound at the final knob position remains identical to the authoritative processor because only transition time changed.

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-live-windows.ps1 -Clean
```

CMake must print:

```text
Sonkupik Live creative-control glide: 0.012 s (authoritative VST default: 0.080 s)
```
