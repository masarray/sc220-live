# Sonkupik Live v0.9.3 — Source Delivery Summary

This milestone completes **P2: Smart Sound Enhancer preset and engine parity verification**.

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
