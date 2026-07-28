# Sonkupik Live v0.13.3

**Sonkupik Live** is a lightweight native Windows live-audio rack for streamers who need professional enhancement without opening a full DAW.

```text
Audio Interface
  -> Input Trim
  -> Musical Noise Clean
  -> ArSonKuPik Signature Engine v0.5.19
  -> Smooth Downward Expander
  -> Output Trim
  -> Locked final protection
  -> CABLE Input / VB-CABLE
  -> OBS or TikTok Studio captures CABLE Output
```

## v0.13.3 — Quiet evaluation UX and reliable device switching

- No active-trial countdown is shown in the main header. `TRIAL ENDED` appears only after the evaluation expires.
- License purchase and activation remain available from **Settings → License & Activation**.
- Input/output selection now retries with compatible automatic timing, clean reopen and safe rollback.
- Runtime UI strings use ASCII-safe separators to avoid Windows mojibake.

## v0.13.0 — 30-day trial and QRIS Full Version

- 30-day full-feature trial with clock rollback protection and separate Sonkupik Live state storage.
- Native Sonkupik Live purchase panel; the VST `PluginEditor` and `QrisLicensePanel` remain excluded.
- One-time Full Version price: **Rp249.000** for one computer.
- Existing MasArray licence server and Midtrans QRIS checkout are used through a background worker.
- Paid orders activate automatically after the locally verified signed activation is received.
- Offline Computer Request ID and signed-code activation remain available.
- After trial expiry, audio remains safe and continuous while creative controls become read-only.

See `docs/LICENSE_AND_QRIS_v0.13.0.md`, `docs/LICENSE_SERVER_PRODUCT_CONFIG_v0.13.0.md`, and `docs/PRICING_STRATEGY_v0.13.0.md`.

## v0.12.0 — mature enhancer engine port

The Smart Sound Enhancer has been refactored around `EnhancerEngineBridge` and ported to the supplied mature **ArSonKuPik v0.5.19 Signature Engine**.

```text
engine version : 0.5.19
header revision: 2026072702
factory profiles: 13
source archive SHA-256:
bb28592ba9b009dfe4f3ef6c7056ae6eee8e94a1db3e1e88ba6ba68a1a73931d
```

The engine is now vendored inside the repository. Builds no longer inspect a neighbouring `askp-vst` checkout and no longer change according to whichever local repository happens to exist.

The v0.5.19 platform includes:

- immutable Mas Ari Signature DSP foundation;
- 13 professional starting profiles;
- adaptive Signature scene lanes;
- mature low-energy vitality and bass authority policy;
- content-aware detail, articulation and density control;
- optional Gain Match comparator;
- final lookahead safety and `-1 dBFS` ceiling;
- factory profile handoff through one DSP topology.

## Deliberate Live integration policy

The port keeps the supplied audio engine and profile data. Only two standalone integration changes are intentional:

1. no VST editor is embedded;
2. creative macro glide is 12 ms for direct live mouse control instead of the VST's 80 ms host-automation glide.

`PluginEditor`, QRIS panel and update UI remain outside this milestone. Future Sonkupik licensing/purchase UX can be added as native Live panels without coupling the audio processor to the VST editor.

## Visualizer is isolated

The ASK-P v0.5.19 visualizer is **not** imported. Sonkupik Live uses a local snapshot of the approved v0.10.4 sphere with its single metallic navy-black bezel.

This prevents clean builds from restoring unwanted glitter, delayed impact frames or another visualizer revision merely because the ASK-P project changed.

## Deterministic source authority

CMake checks SHA-256 fingerprints of:

- DSP source and header;
- 13-profile bank;
- headless Live processor port;
- approved local sphere source.

A mismatch is a fatal configuration error. See:

- `ASKP_AUTHORITY.lock`
- `docs/ENHANCER_ENGINE_PORT_v0.12.0.md`

## Windows build — Visual Studio Community 2026

Double-click:

```text
build-live-windows.bat
```

Clean build:

```bat
build-live-windows.bat clean
```

PowerShell:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -Clean
```

No ASK-P checkout or `-AskpSourceDir` argument is required. JUCE 8.0.14 is fetched unless a local JUCE source is supplied:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -JuceSourceDir D:\SDK\JUCE-8.0.14 `
  -Clean
```

## Preset parity QA

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\run-p2-parity.ps1 `
  -Clean
```

The reference renderer uses the embedded v0.5.19 processor with its original 80 ms glide. The Live renderer uses the same engine/profile source with the documented 12 ms control glide.

## Workspace

- **Left:** Before Enhancer rack, input fader and stereo meter.
- **Centre top:** Musical Noise Clean, expander monitor and approved Sonkupik sphere.
- **Centre bottom:** 13-profile Smart Sound Enhancer and five macros.
- **Right:** After Chain rack, output fader and stereo meter.
- **Settings:** sample rate, buffer size, channel mode and denoise quality.
- **Footer:** readable CPU summary, latency, dropouts and runtime status.

## Current transport limitation

The application still uses the JUCE/Windows audio-device backend. A future native dual-client WASAPI bridge with adaptive clock-drift correction remains a separate transport milestone and is not claimed by this engine port.
