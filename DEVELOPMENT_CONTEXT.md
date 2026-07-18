# Sonkupik Live Development Context

Current milestone: **v0.8.2 — authoritative engine/preset sync and lightweight liquid visualizer**.

The application is intentionally not a DAW. It is a compact live signal rack for audio-interface input to VB-CABLE/OBS/TikTok Studio.

## Non-negotiable architecture

- Product name: **Sonkupik Live**.
- C++17 + JUCE shell.
- Smart Enhancer DSP and all factory presets come from the locked private `masarray/askp-vst` source; do not fork or hand-copy recipe values.
- Built-in Live Safe chain remains usable without external VST hosting.
- No lock, allocation, file I/O, UI call or device enumeration in the audio callback.
- Visual output is disposable: it may drop data but may never delay audio.
- Safety limiter remains the final non-bypassable stage.
- Main window may hide to tray without stopping audio.

## Read before modifying

- `ASKP_AUTHORITY.lock`
- `docs/AUDIT_VST_AUTHORITY_v0.8.0.md`
- `docs/VISUALIZER_ARCHITECTURE_v0.8.0.md`
- `docs/MUSICAL_NOISE_CLEAN_DESIGN.md`
- `docs/REALTIME_SAFETY.md`
- `docs/UX_SPEC.md`
- `qa/v080_source_audit.txt`
