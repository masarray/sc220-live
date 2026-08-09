# SC220 Live — ASK-P v0.5.24 Production DSP Port

## Authority

- Source repository: `masarray/askp-vst`
- Latest audited upstream HEAD: `f91aa04e953cf14eb0682a1bc305138b6507d73a`
- Latest approved release: **v0.5.24**
- Approved product source commit: `1cf050d40338cbcb5d8cddd9fc2de5f23e3d625f`
- Current approved release-helper commit: `811b7376dd784ce9f64e03512c7baf7de5e77425`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- P0.3 feature: **Smart Warm Vocal Enhancement**

The approved upstream release request explicitly names `1cf050d4...` as the immutable product source commit. SC220 Live pins that product source rather than following the moving repository HEAD. The later upstream commits through `f91aa04e...` change release-request, release-audit, macOS/Pages setup and verification helpers only; the product DSP, presets, processor and production CMake source remain the approved v0.5.24 source.

## Production DSP source contract

ASK-P v0.5.24 production does not compile the canonical `engine/arsonkupik_dsp.cpp` directly. The approved sound is represented by the immutable canonical input plus a fail-closed audit/generator pipeline.

SC220 Live now mirrors the same production sequence:

```text
tools/p0_musical_rebalance_source_audit.py
        |
        v
scripts/generate_p0_musical_rebalance_dsp.py
        |
        v
<SC220 build>/generated/askp-v0.5.24-1cf050d/arsonkupik_dsp.cpp
```

CMake refuses configuration when the upstream source audit fails, generation fails, the generated file is missing, or SC220's supplemental reviewed anchors are absent. The generated translation unit is compiled by SC220 Live and both optional parity renderers.

This closes the main gap in the earlier port, which ran the generator but did not yet run the complete upstream production source audit first.

## Audio improvements carried by SC220 Live

### P0 — musical rebalance / rounder authority

- low-body shelf: 76 Hz / +1.12 dB / Q0.64;
- Signature sub glide: 52 Hz / +0.74 dB;
- Signature deep bloom: 78 Hz / +0.72 dB;
- Signature bass torque: 118 Hz / +0.74 dB / Q0.70;
- Signature chest authority: 185 Hz / +0.53 dB;
- warm particles: 78;
- Smart Bass: 70;
- body focus: 1.030;
- Signature output trim: -0.35 dB;
- Signature linked full-band macro compressor remains disabled so enhancement does not create programme-level pumping.

### P0.1 — anti-ringing vocal presence

- base Presence: 2180 Hz / +0.40 dB / Q0.64;
- restrained Vocal Presence gain law with Q0.95;
- Vocal Clarity: 3500 Hz / Q1.05;
- P6 Presence Mid: 1.04x;
- residual ~2.3 kHz spotlight remains removed without sacrificing higher vocal clarity.

### P0.2 — sparkling / 3D air

- P6 Tickle Mid: 1.21x;
- Air articulation modulation: 0.030;
- Skin articulation modulation: 0.035;
- bounded Smart Side Air authority increase;
- generated 3D Air coefficients: 0.066 / 0.026 / 0.094;
- generated Air remains high-passed at 6.2 kHz;
- dedicated Side Air remains high-passed at 7.8 kHz;
- brightness, density, scene-space, correlation and anti-phase protection remains active.

### P0.3 — Smart Warm Vocal Enhancement

P0.3 keeps the proven Presence-led vocal-body detector and adds a bounded lower-body-led route for warm/dark/chest-led vocals whose identity is carried more by throat/body than 1.5–3 kHz Presence.

The route reuses existing analysis envelopes only. It adds no FFT, filter, compressor, audio lane, allocation or latency. It requires a real Presence floor, musical 240–1100 Hz throat/body evidence, centred lower-body content, non-dominant 72–150 Hz Core, and the existing mud/sustain/freshness/centre guards.

This changes detector coverage, not global vocal gain. Vocal Body remains 76, the 575 Hz / +1.96 dB warm-body bridge remains unchanged, and kick/bass or wide low-mid material remains explicitly rejected by the production source audit.

## Presets and bridge defaults

SC220 compiles `engine/arsonkupik_presets.cpp` from the same approved v0.5.24 product source. All 13 factory profiles remain available through `EnhancerEngineBridge`.

The bridge now reports engine version `0.5.24`. Its defensive Smart Bass fallback is also aligned to the production Signature value of `70` rather than the older `68` fallback.

## SC220-specific behaviour retained

- headless ASK-P processor; ASK-P editor and Orb visualizer remain excluded;
- local `sonkupik-sphere-v0.10.4` remains the only visualizer;
- SC220 Live control glide remains 12 ms;
- 80 ms reference processor remains available for parity QA;
- ASK-P safety/telemetry bridge remains intact: input, pre-safety and output peaks/overs, safety gain reduction and Gain Match telemetry;
- SC220-specific licence storage isolation remains intact;
- final lookahead/ceiling policy remains owned by the approved ASK-P processor.

## Build behaviour

`scripts/build-live-windows.ps1` now automatically runs the authority sync helper before CMake configuration. This verifies the exact v0.5.24 gitlink and required upstream source-audit/generator files before compilation.

The production build identity is:

```text
SC220 Live Embedded Authority v0.5.24 @ 1cf050d
```

Optional parity targets also compile the same generated production DSP, with the reference retaining 80 ms macro glide and Live retaining 12 ms.

## Validation status

Source integration is aligned to the latest approved ASK-P v0.5.24 product source contract. The previous GitHub Actions attempt was blocked before execution by repository/account runner billing, so full hosted-runner MSVC/audio-device validation still depends on Actions becoming runnable or a local Windows build.

The critical listening/runtime checks remain:

- warm/dark vocal foreground improvement without 2–3 kHz bite;
- no male-vocal boom or kick/bass false triggering;
- no return of 2.3 kHz ringing;
- stable round bass without programme pumping;
- sparkling air without fizz or centre-image instability;
- 48/96 kHz and 32/64/128-sample realtime stress tests;
- silence/resume, preset transitions, bypass, automation and crackle/deadline checks.
