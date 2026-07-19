# Sonkupik Live v0.10.0

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

## v0.10.0 P1 premium polish

P1 refines the P0 structure without changing the audio path. Side faders now use an integrated mixer chassis, realistic metallic thumbs and lightweight peak hold. Rack rows expose clear ON/BYPASS/ADD/SAFE states and can be toggled across the full row. Noise Clean and Smart Sound Enhancer now use inset workstation-style control bays, reducing empty space and making the centre workspace feel like one professional audio surface.

The UI still refreshes at 24 Hz and P1 adds no work to the real-time audio callback. See `docs/P1_PREMIUM_POLISH_v0.10.0.md`.

## v0.9.9 P0 premium workspace redesign

The main workspace now uses one authoritative geometry model for both painting and child placement. Header, routing strip, side racks, Noise Clean, Smart Sound Enhancer and footer share a compact spacing system. The redesign adds a premium engine-status capsule, quiet cached titlebar icons, a balanced control/expander/sphere layout, a clear preset hierarchy and refined card depth without increasing the 24 Hz UI refresh rate.

The headless processor architecture from v0.9.7 remains intact: `PluginEditor`, `QrisLicensePanel`, and `UpdateChecker` are not part of the Live executable, while DSP, presets, LicenseManager, ScopeTap, OrbVisualizer, gain staging, bypass, and final protection remain authoritative.

## v0.9.6 authoritative engine with preset parity QA

Sonkupik Live compiles the authoritative DSP, 40 presets, WOW calibration, preset handover, lookahead safety, bypass path, meters, and scope tap from `askp-vst`. The only intentional processing-wrapper delta is a configure-time **12 ms creative-control glide** for live mouse control; the VST's 80 ms glide remains unchanged in the VST repository. Settled sound, preset values, coefficient recipes, limiter behaviour, and output policy remain authoritative.

```text
repository : masarray/askp-vst
version    : 0.5.14
commit     : 655874dee6ee69df7212b8073ab941f9607d8faa
```

The lock is recorded in `ASKP_AUTHORITY.lock`. The build script detects a sibling `askp-vst` checkout or automatically syncs the locked commit into `.deps/askp-vst` after Git authentication.

## Lightweight sphere visualizer

The centre visual is a sphere visualizer that consumes the authoritative VST `ScopeTap`. Visual data remains disposable and never blocks the audio callback:

- 24 Hz only while the window is visible;
- fixed-size arrays and a lossy lock-free stereo scope queue;
- no allocation, lock, FFT plan creation, or message-thread call in the audio callback;
- timer and scope tap fully stop in tray mode.

The Live workspace uses the authoritative VST sphere behaviour with a dark UX-matched bezel overlay; the audio tap and processor remain shared.

## Windows build — Visual Studio Community 2026

### Single-click build

Double-click:

```text
build-live-windows.bat
```

It builds Release x64, creates the portable ZIP, and opens the `dist` folder. For a forced clean build from Command Prompt:

```bat
build-live-windows.bat clean
```

### PowerShell build

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

## P2 preset audio parity test

Run the deterministic reference/live comparison on Windows:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\run-p2-parity.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -Clean
```

The test independently renders the original `askp-vst` processor and Sonkupik Live processor for all 40 presets. It requires identical latency, output fingerprint, RMS and peak after warm-up. See `docs/AUDIT_PRESET_PARITY_v0.9.3.md`.

The Settings card also provides **Match VST Reference**, which bypasses Noise Clean and Expander, resets both outer faders to 0 dB, selects stereo input mode and keeps Smart Sound Enhancer active for a manual latency-aligned A/B test.

## Main workspace

- **Left:** Before Enhancer rack, input fader and stereo meter.
- **Centre:** Musical Noise Clean controls + sphere visualizer visualizer, then the 40-preset Smart Enhancer and five macros.
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
- `docs/AUDIT_CONTROL_RESPONSE_v0.9.2.md`
- `docs/AUDIT_PRESET_PARITY_v0.9.3.md`
- `qa/v092_control_response_audit.txt`
- `qa/v093_source_parity_audit.txt`
- `docs/P1_PREMIUM_POLISH_v0.10.0.md`
- `qa/v0100_p1_audit.txt`

## Current limitation

v0.10.0 retains the v0.9.2 direct processor path and adds deterministic preset parity QA; v0.9.2 removed the delayed background-engine rebuild and uses the VST real-time processor directly, but JUCE currently opens the physical input and VB-CABLE output through the Windows device backend. A future native dual-client `IAudioClient3` bridge with adaptive clock-drift correction remains the strongest route to a formal multi-hour anti-crackle guarantee.

## Live knob response

At configure time CMake creates a generated `PluginProcessor_Live.cpp` from the locked `askp-vst` processor and replaces only the 80 ms creative macro glide with 12 ms. The build fails if the audited source anchor is missing, preventing a silent patch against an unknown processor revision. See `docs/AUDIT_CONTROL_RESPONSE_v0.9.2.md`.
