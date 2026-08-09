# Sonkupik Live v0.14.0 — ASK-P v0.5.23 Engine Port

> **Historical record only.** This authority was superseded by the approved ASK-P v0.5.24 production P0.3 port. Current builds are governed by `ASKP_AUTHORITY.lock` and `docs/ASKP_P03_DSP_PORT.md`; do not use the v0.5.23 commit below for new builds.

## Authority

- Repository: `masarray/askp-vst`
- Historical commit: `5757119cab7a1a75f09dcf0edea6e3be97975439`
- Historical product version: `0.5.23`
- DSP header revision: `2026073102`
- Factory profiles: `13`

## Compiled scope

Sonkupik compiled `engine/arsonkupik_dsp.*`, `arsonkupik_presets.cpp`,
`wow_headroom_guard.hpp`, the processor and LicenseManager from the pinned
historical authority. CMake generated two headless processor variants from that exact source:

- reference: editor removed, original 80 ms macro glide;
- Live: editor removed, 12 ms macro glide.

No ASK-P `PluginEditor`, QRIS panel, updater or `OrbVisualizer` was compiled.
Sonkupik's approved local sphere remained the only visualizer.

## v0.5.23 delta that entered Live

- shared post-Input Reference Bypass with the same final ceiling policy;
- honest input, pre-safety and output over telemetry;
- dry-authority path and scale-linear wet/punch architecture;
- body-stability and 13-profile tuning;
- lookahead safety and Gain Match implementation.

## Superseded build contract

The historical Windows build initialised and verified the private v0.5.23 submodule and failed closed on authority drift. Current builds instead pin the approved v0.5.24 product source and run the upstream P0.3 production source audit before generating the effective DSP translation unit.
