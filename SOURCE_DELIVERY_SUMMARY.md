# Sonkupik Live v0.10.0 — Source Delivery Summary

## P1 premium polish and repository/build hygiene

Implemented in v0.10.0:

- integrated side-fader chassis and realistic premium fader thumb;
- lightweight 24 Hz peak-hold meter display;
- rack-row state capsules, hover hierarchy and full-row bypass interaction;
- inset Noise Clean controls/visualizer bays;
- dedicated Active Preset and Macro Control bays;
- strengthened `.gitignore` for generated output, dependencies, packages and secrets;
- root-level single-click `build-live-windows.bat`;
- Release portable packaging with optional clean rebuild.

Documentation and audit:

```text
docs/P1_PREMIUM_POLISH_v0.10.0.md
qa/v0100_p1_audit.txt
```

## P0 premium layout milestone

This milestone implements Option A (component-level audit/checklist) and Option B (workspace redesign blueprint), then applies the P0 changes to the actual JUCE application.

Implemented:

- one authoritative `WorkspaceLayout` for paint and placement;
- shared spacing/radius metrics;
- premium brand/status header and quiet cached utility icons;
- unified Input/Output routing strip;
- rebalanced Noise Clean controls, expander reduction monitor and sphere;
- explicit PRESET hierarchy and equal macro-knob grid;
- refined card depth, header separators and border precision;
- no increase to visual refresh rate or audio-thread workload.

Documentation:

```text
docs/P0_PREMIUM_LAYOUT_AUDIT_v0.9.9.md
docs/P0_LAYOUT_REDESIGN_BLUEPRINT_v0.9.9.md
qa/v099_layout_p0_audit.txt
```

## Runtime architecture

Sonkupik Live continues to process the enhancer through the authoritative `askp::Processor::processBlock()` implementation. DSP, all 40 presets, WOW calibration, preset handover, lookahead safety, bypass, metering and scope publication come directly from the locked `askp-vst` checkout.

The only intentional processor-source delta remains the P1 control glide:

```text
askp-vst       : 80 ms
Sonkupik Live  : 12 ms
```

This changes control travel time, not settled preset output.

## P2 additions

- Optional `SonkupikParityReference` renderer using original `PluginProcessor.cpp`.
- Optional `SonkupikParityLive` renderer using generated `PluginProcessor_Live.cpp`.
- Deterministic five-second stereo input for all 40 presets.
- Three-second warm-up before output analysis.
- Exact FNV-1a 64-bit float-output fingerprint.
- RMS, peak, latency and analysed-sample validation.
- `scripts/run-p2-parity.ps1` for one-command build/render/compare.
- **Match VST Reference** action in Settings for manual A/B conditions.

## Source-level audit performed in this environment

```text
Checks : 24
Passed : 24
Failed : 0
Result : PASS
```

The actual 40-preset binary render comparison must be run on the Windows/MSVC machine because this environment does not contain the Windows JUCE/MSVC toolchain.

## Windows command

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\run-p2-parity.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -Clean
```

Expected final result:

```text
Presets checked: 40
Passed: 40
Failed: 0
Result: PASS
```

## P1 source audit

```text
Checks : 25
Passed : 25
Failed : 0
```

## P0 source audit

```text
Checks : 23
Passed : 23
Failed : 0
```

This audit verifies the shared layout model, spacing tokens, status capsule, I/O strip, Noise Clean zoning, vertical expander reduction meter, preset hierarchy, cached icon rendering and unchanged 24 Hz refresh policy.
