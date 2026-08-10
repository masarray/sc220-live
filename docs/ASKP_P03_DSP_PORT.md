# SC220 Live — ASK-P v0.5.24 Production DSP Port

## Authority

- Source repository: `masarray/askp-vst`
- Latest audited upstream HEAD: `44b7d1657fbbd2c13cd490e251e8867734238623`
- Approved release line: **v0.5.24**
- Approved product source commit: `2c764f82b0fc99eaff04d918cef4f839db7509e5`
- Approved release-helper commit: `811b7376dd784ce9f64e03512c7baf7de5e77425`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- Effective DSP checkpoint: **P0.3 Smart Warm Vocal Enhancement**
- Processor delta included in the approved source: **Gain Match settle-controller fix**

SC220 pins the immutable approved product source rather than following the moving ASK-P branch head. The upstream release metadata commit after `2c764f82...` does not introduce another product-source change.

## Production DSP source contract

ASK-P v0.5.24 production does not compile the canonical `engine/arsonkupik_dsp.cpp` directly. The approved effective sound is represented by immutable canonical input plus a fail-closed audit/generator pipeline:

```text
tools/p0_musical_rebalance_source_audit.py
        |
        v
scripts/generate_p0_musical_rebalance_dsp.py
        |
        v
<SC220 build>/generated/askp-v0.5.24-2c764f8/arsonkupik_dsp.cpp
```

CMake refuses configuration when the upstream source audit fails, generation fails, the generated file is missing, or SC220's supplemental reviewed anchors are absent. The generated translation unit is compiled by SC220 Live and the optional parity renderers.

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
- Signature linked full-band macro compressor remains disabled to prevent programme-level pumping.

### P0.1 — anti-ringing vocal presence

- base Presence: 2180 Hz / +0.40 dB / Q0.64;
- restrained Vocal Presence law with Q0.95;
- Vocal Clarity: 3500 Hz / Q1.05;
- P6 Presence Mid: 1.04x;
- the old ~2.3 kHz spotlight remains removed without sacrificing higher vocal clarity.

### P0.2 — sparkling / 3D air

- P6 Tickle Mid: 1.21x;
- Air articulation modulation: 0.030;
- Skin articulation modulation: 0.035;
- bounded Smart Side Air authority;
- generated 3D Air coefficients: 0.066 / 0.026 / 0.094;
- generated Air high-pass: 6.2 kHz;
- dedicated Side Air high-pass: 7.8 kHz;
- brightness, density, scene-space, correlation and anti-phase protection remain active.

### P0.3 — Smart Warm Vocal Enhancement

P0.3 retains the Presence-led vocal-body detector and adds a bounded lower-body-led route for warm/dark/chest-led vocals whose identity is carried more by throat/body than 1.5–3 kHz Presence.

The route reuses existing analysis envelopes. It adds no FFT, filter, compressor, audio lane, allocation or latency. It requires a Presence floor, musical 240–1100 Hz throat/body evidence, centred lower-body content, non-dominant 72–150 Hz Core, and existing mud/sustain/freshness/centre guards.

This expands detector coverage rather than increasing global vocal gain. Vocal Body remains 76 and the 575 Hz / +1.96 dB warm-body bridge remains unchanged.

## Gain Match parity status

The approved product source `2c764f82...` contains the latest ASK-P Gain Match controller behaviour: the target can remain live while Gain Match is off and during the short engagement settle period, then freezes after engagement settles to avoid long-term song-section-following breathing.

SC220 compiles this processor source for parity, but SC220 Live intentionally keeps:

```text
gain_match_default_enabled=false
gain_match_ui_exposed=false
```

Therefore this processor fix does not change the normal SC220 Live tonal path when Gain Match remains off. It is included so the embedded processor authority no longer lags the approved ASK-P product source.

## Presets and bridge defaults

SC220 compiles `engine/arsonkupik_presets.cpp` from the same approved v0.5.24 product source. All 13 factory profiles remain available through `EnhancerEngineBridge`.

The bridge reports engine version `0.5.24`; its defensive Smart Bass fallback is `70`.

## SC220-specific behaviour retained

- headless ASK-P processor; ASK-P editor and Orb visualizer excluded;
- local `sonkupik-sphere-v0.10.4` remains the visualizer;
- Live control glide: 12 ms;
- reference/parity control glide: 80 ms;
- input, pre-safety and output peaks/overs remain bridged;
- safety gain reduction and Gain Match telemetry remain available internally;
- SC220-specific licence storage and product identity remain isolated;
- final ASK-P processor safety policy remains intact.

## Build identity

The production embedded authority identity is:

```text
SC220 Live Embedded Authority v0.5.24 @ 2c764f8
```

## v0.1.1 release receipt

SC220 Live v0.1.1 was published on 10 August 2026 from private source commit:

```text
994b983a7f02b6a2285c41781ecb77dc07e3b217
```

Public installer SHA-256:

```text
589b174357cb36c62611d7a0e89bfa6a7d25251fc27015b5dd5b25fb54c04686
```

The public `masarray/sc220-download` publish/verification workflow run `31346714396` completed successfully and the release is published as stable v0.1.1.

This publication receipt proves public asset/tag verification. It should not be used as a substitute for separate listening, realtime device, or host-specific audio validation evidence.
