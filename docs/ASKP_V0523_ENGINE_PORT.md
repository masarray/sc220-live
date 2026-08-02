# Sonkupik Live v0.14.0 — ASK-P v0.5.23 Engine Port

## Authority

- Repository: `masarray/askp-vst`
- Commit: `5757119cab7a1a75f09dcf0edea6e3be97975439`
- Product version: `0.5.23`
- DSP header revision: `2026073102`
- Factory profiles: `13`

## Compiled scope

Sonkupik compiles `engine/arsonkupik_dsp.*`, `arsonkupik_presets.cpp`,
`wow_headroom_guard.hpp`, the processor and LicenseManager from the pinned
authority. CMake generates two headless processor variants from that exact source:

- reference: editor removed, original 80 ms macro glide;
- Live: editor removed, 12 ms macro glide.

No ASK-P `PluginEditor`, QRIS panel, updater or `OrbVisualizer` is compiled.
Sonkupik's approved local sphere remains the only visualizer.

## v0.5.23 delta now available to Live

- shared post-Input Reference Bypass with the same final ceiling policy;
- honest input, pre-safety and output over telemetry;
- dry-authority path and scale-linear wet/punch architecture;
- body-stability and current 13-profile tuning;
- current lookahead safety and Gain Match implementation.

## Build

The Windows build script initialises and verifies the private submodule. Git
Credential Manager must be authenticated for both private repositories. CMake
fails closed when the checked-out commit, engine revision, profile contract or
processor transformation anchors differ.

The full MSVC/audio-device acceptance test remains required before merging.
