# Sonkupik Live v0.9.0 — Visual, Engine, and Interaction Audit

## Root causes found in v0.8.2

1. The Live visualizer was not the VST visualizer. `LiveLiquidVisualizer.cpp` was a shortened reimplementation of `OrbVisualizer.cpp`; roughly half of the VST onset, ballistic, return, rebound, stereo-placement, particle-depth, and idle-state logic had been removed or approximated.
2. The Live enhancer used only `ArSonKuPikEngine`. It omitted the authoritative VST wrapper stages in `PluginProcessor.cpp`: 80 ms macro smoothing, 400 Hz bounded in-place retune, prepared dual-engine preset handover, context WOW calibration, one-millisecond lookahead safety, latency-aligned bypass, and the VST meter/scope publication path.
3. Macro dragging rebuilt a complete engine on a background thread. The 28 ms coalescing wait, construction, prepare/reset, pending-slot wait, and five-millisecond crossfade caused the audible hundreds-of-milliseconds control lag.
4. Input Trim, Output Trim, and both external slots were constructed with bypass disabled. The disabled power icons looked interactive but were intentionally unwired.
5. The main Smart Enhancer had no visible bypass control.

## v0.9.0 correction

- Sonkupik Live compiles and instantiates the authoritative `askp::Processor` from `askp-vst/src/PluginProcessor.cpp`.
- Presets, macros, WOW calibration, lookahead protection, click-free bypass, parameter smoothing, and metering now run through the same implementation as the VST.
- The exact `askp::OrbVisualizer` is embedded. The custom Live sphere and custom visual tap were removed.
- Macro changes write directly to the VST APVTS. The VST performs its normal 80 ms glide and up-to-400 Hz in-place retune; no background engine construction occurs while dragging.
- A dedicated Smart Enhancer power button controls the VST's latency-aligned bypass.
- Input Trim, Musical Noise Clean, and Output Trim rack power buttons are functional.
- External slots use an active ADD button/row action instead of a fake disabled bypass switch.
- Safety Limiter remains visibly locked because it is the authoritative VST final protection stage.

## Parity boundary

With Musical Noise Clean bypassed, Expander bypassed, and both outer faders at 0 dB, Sonkupik Live sends the audio buffer directly through the same `askp::Processor::processBlock()` used by the VST. This is the parity reference mode.
