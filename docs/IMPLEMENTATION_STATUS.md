# Implementation Status — v0.10.1

## v0.10.1 build reliability completed

- Automatically closes a running Sonkupik Live preview before linking.
- Removes stale Release executable with bounded retry.
- Retries the linker once after releasing output locks.
- Single-click BAT and portable packaging use the same lock-safe workflow.
- DSP and P1 workspace source remain unchanged from v0.10.0.

## Previous milestone — v0.10.0

## v0.10.0 P1 completed

- Premium side fader/meter chassis.
- UI-only peak hold at the existing 24 Hz cadence.
- Full-row rack bypass interaction and explicit stage-state capsules.
- Noise control/visualizer inset bays.
- Active Preset and Macro Control bays.
- Hardened `.gitignore` and single-click Windows Release/package BAT.


## Implemented

- Native Windows JUCE live audio application.
- Authoritative `askp::Processor` Smart Sound Enhancer path.
- 40 factory presets from locked `askp-vst` v0.5.14.
- P1 12 ms Live creative-control glide.
- P2 optional reference/live offline renderers.
- P2 exact preset output comparison infrastructure.
- Musical Noise Clean and smooth expander.
- Input/output device routing, tray lifecycle, meters and authoritative sphere visualizer.
- Functional built-in rack bypass controls.
- Manual **Match VST Reference** conditions.
- P0 premium workspace layout with one authoritative geometry model.
- Premium header status capsule, I/O strip, refined expander meter and preset hierarchy.

## Requires Windows validation

- Compile both P2 parity renderer targets under MSVC.
- Run all 40 deterministic renders.
- Confirm 40/40 exact fingerprints.
- Perform listening A/B using aligned recordings.
- Eight-hour live soak test with zero dropouts.

## Roadmap

- Crash-isolated VST3 scanner and hosting.
- Native dual-client WASAPI/ASIO drift-corrected bridge.
- Signed installer strategy with VB-CABLE redistribution compliance.

- Headless askp processor integration: implemented in v0.9.7; VST editor/QRIS/update UI excluded from Live target.
