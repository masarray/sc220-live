# Sonkupik Live Development Context

Current milestone: **v0.13.2 — 30-day trial and QRIS Full Version licensing**.

## Audio authority

- ArSonKuPik engine version: `0.5.19`
- DSP header revision: `2026072702`
- Factory profiles: `13`
- Source: user-supplied `askp-vst-master (4).zip`
- Source archive SHA-256: `bb28592ba9b009dfe4f3ef6c7056ae6eee8e94a1db3e1e88ba6ba68a1a73931d`
- Integration: vendored, headless, deterministic

## Intentional Live delta

- VST editor excluded.
- Creative macro smoothing changed from 80 ms to 12 ms.
- Gain Match remains off by default.

## Visual authority

- Local approved v0.10.4 sphere snapshot.
- No dependency on the ASK-P v0.5.19 OrbVisualizer.
- Single metallic navy-black bezel.

## Architecture

`LiveAudioEngine` communicates with the enhancer exclusively through
`EnhancerEngineBridge`. Parameter IDs, preset handoff, meter atomics and
ScopeTap access must remain inside that adapter.

## Non-goals for v0.12.0

- external VST hosting;
- QRIS purchase panel;
- native dual-client WASAPI clock-drift bridge;
- automatic buffer recovery experiments from v0.11.0.
