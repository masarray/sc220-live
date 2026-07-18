# Sonkupik Live v0.8.2

**Sonkupik Live** is a lightweight native Windows live-audio rack for streamers who need professional enhancement without opening a full DAW.

```text
Audio Interface
  -> Input Trim
  -> Musical Noise Clean
  -> ArSonKuPik Smart Enhancer
  -> Smooth Downward Expander
  -> Output Trim
  -> Locked Safety Limiter
  -> CABLE Input / VB-CABLE
  -> OBS or TikTok Studio captures CABLE Output
```

## v0.8.0 authority lock

The Live app no longer maintains a divergent copy of the enhancer recipe. CMake compiles the exact DSP and all **40 factory presets** from the audited private repository:

```text
repository : masarray/askp-vst
version    : 0.5.14
commit     : 655874dee6ee69df7212b8073ab941f9607d8faa
```

The lock is recorded in `ASKP_AUTHORITY.lock`. The build script detects a sibling `askp-vst` checkout or automatically syncs the locked commit into `.deps/askp-vst` after Git authentication.

## v0.8.2 UI lifecycle correction

The main window explicitly owns the 24 Hz UI heartbeat. Showing the window enables status polling and the lock-free visual tap; hiding to tray stops both while the audio engine continues. This fixes frozen footer values, input/output meters and the liquid sphere after application startup.

## Lightweight visual architecture

The old spectrum graph is removed from the main workspace. The current hero visual follows the latest ArSonKuPik liquid visualizer architecture:

- 24 Hz only while the window is visible;
- audio callback performs only a lossy lock-free stereo tap copy;
- newest 4096 samples, six broad time-domain bands;
- fixed 32-node liquid lattice, 8 bubbles and 12 droplets;
- cached sphere/glass layers;
- no visual FFT, tempo detector, beat prediction or dynamic particle allocation;
- animation sleeps after silence and fully stops in tray mode.

Musical Noise Clean still uses its own fixed 256/512 transform because spectral denoise requires it. That transform has a precomputed per-instance plan and no JUCE/shared FFT lock.

## Windows build — Visual Studio Community 2026

From a clean source folder:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -Clean
```

The script:

1. detects Visual Studio Community 2026 and the MSVC C++ workload;
2. chooses `Visual Studio 18 2026` when CMake supports it, otherwise Ninja + MSVC;
3. finds or securely syncs the locked private `askp-vst` authority;
4. fetches JUCE 8.0.14 unless a local JUCE tree is supplied;
5. builds the `SonkupikLive` target.

Explicit local paths:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -JuceSourceDir D:\SDK\JUCE-8.0.14 `
  -Clean
```

## Main workspace

- **Left:** Before Enhancer rack, input fader and stereo meter.
- **Centre:** Musical Noise Clean controls + liquid sphere, then the 40-preset Smart Enhancer and five macros.
- **Right:** After Chain rack, output fader and stereo meter.
- **Settings card:** sample rate, buffer size, channel mode and denoise quality.
- **Footer:** average and callback-peak load, latency, dropouts, profile and tray state.

UX uses **Plus Jakarta Sans** from the authoritative `askp-vst` checkout and embedded Lucide-style SVG icons. The delivery ZIP intentionally does not duplicate font binaries.

## Source of truth

Read these first:

- `docs/AUDIT_VST_AUTHORITY_v0.8.0.md`
- `docs/VISUALIZER_ARCHITECTURE_v0.8.0.md`
- `docs/ASKP_SOURCE_SYNC.md`
- `docs/REALTIME_SAFETY.md`
- `qa/v080_source_audit.txt`

## Current limitation

v0.8.0 substantially reduces application-side deadline risk, but JUCE currently opens the physical input and VB-CABLE output through the Windows device backend. A future native dual-client `IAudioClient3` bridge with adaptive clock-drift correction remains the strongest route to a formal multi-hour anti-crackle guarantee.
