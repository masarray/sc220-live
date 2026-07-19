# Sonkupik Live Development Context

Current milestone: **v0.9.9 — headless Smart Sound Enhancer processor and QrisLicensePanel linker fix**.

## Product intent

A lightweight live audio rack for streamers who currently open a full DAW only to clean, enhance and route their audio interface signal into VB-CABLE for OBS or TikTok Studio.

## Current engine authority

- Repository: `masarray/askp-vst`
- Version: `0.5.14`
- Commit: `655874dee6ee69df7212b8073ab941f9607d8faa`
- Factory presets: 40
- Full wrapper authority: `askp::Processor`
- Live-only processor delta: creative-control smoothing 80 ms → 12 ms

## P2 parity boundary

Enhancer parity means:

```text
Stereo input
-> askp::Processor
-> same settled output as askp-vst
```

Whole-rack audio can intentionally differ when Musical Noise Clean, Expander, non-unity outer trims, mono duplication, device resampling or VB-CABLE format conversion are active.

Use `scripts/run-p2-parity.ps1` for binary preset verification and the Settings action **Match VST Reference** for manual A/B conditions.

## Next logical milestone

P3 remains functional third-party VST3 hosting with crash-isolated scanning, latency reporting, state recall and safe bypass. It must not compromise the Live Safe built-in chain.

## v0.9.9 P0 layout rule

The main window now has one authoritative `WorkspaceLayout` used by both `paint()` and `resized()`. Do not reintroduce separate ad-hoc rectangle calculations. Premium spacing and radius tokens live in `live/LiveTheme.h`. See `docs/P0_PREMIUM_LAYOUT_AUDIT_v0.9.9.md` and `docs/P0_LAYOUT_REDESIGN_BLUEPRINT_v0.9.9.md`.
