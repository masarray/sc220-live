# Build Fix v0.9.7 — Headless askp Processor

## Symptom

MSVC linked `PluginEditor.obj` but could not resolve `askp::QrisLicensePanel` constructor/destructor. The private askp-vst editor acquired a QRIS licensing panel, while the Sonkupik Live target compiled `PluginEditor.cpp` without all of its editor-only implementation files.

## Resolution

Sonkupik Live does not need the VST editor. CMake now generates:

- `generated/askp-live/PluginProcessor.h` with `hasEditor() == false`;
- `PluginProcessor_Reference.cpp` with `createEditor() == nullptr` and the authoritative 80 ms control glide;
- `PluginProcessor_Live.cpp` with `createEditor() == nullptr` and the audited 12 ms Live control glide;
- a copied `OrbVisualizer.h` beside the generated processor header to keep one consistent Processor class definition.

The Live target excludes `PluginEditor.cpp` and `UpdateChecker.cpp`. Consequently `QrisLicensePanel` is not referenced at link time.

## Audio parity

The removed sources are UI-only. The target still compiles:

- `arsonkupik_dsp.cpp`;
- `arsonkupik_presets.cpp`;
- the authoritative processor body;
- `LicenseManager.cpp`;
- `AskpLookAndFeel.cpp` used by the Live UI;
- authoritative `OrbVisualizer`/`ScopeTap`;
- all limiter, WOW, gain, preset, bypass and metering logic.

## Expected configure log

```text
Sonkupik Live processor: headless/no VST editor; creative-control glide 0.012 s (VST authority: 0.080 s)
```
