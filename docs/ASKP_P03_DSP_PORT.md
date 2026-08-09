# SC220 Live — ASK-P P0.3 Effective DSP Port

## Authority

- Source repository: `masarray/askp-vst`
- Pinned checkpoint: `240b9d3be3ca1fbdedfa5b7027e465c22a56b0ea`
- Upstream checkpoint: **P0.3 Smart Warm Vocal Enhancement**
- Canonical base engine version: `0.5.23`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- Release status: **development/listening checkpoint; not an upstream production release**

SC220 Live intentionally keeps the embedded plugin version at `0.5.23`. ASK-P has not yet folded P0.3 into canonical `engine/arsonkupik_dsp.cpp` or published a new production version. The SC220 build identity therefore names the P0.3 checkpoint explicitly instead of inventing `0.5.24`.

## Why the port generates DSP

Between the previous SC220 authority `5757119cab7a1a75f09dcf0edea6e3be97975439` and P0.3 checkpoint `240b9d3be3ca1fbdedfa5b7027e465c22a56b0ea`, upstream changed the presets and added the accepted P0/P0.1/P0.2/P0.3 calibration pipeline, but did not yet fold that calibration into canonical `engine/arsonkupik_dsp.cpp`.

Compiling the canonical file directly would therefore produce the wrong sound even though the submodule and presets looked current.

SC220 Live now runs the pinned upstream generator during CMake configuration:

```text
scripts/generate_p0_musical_rebalance_dsp.py
    canonical engine/arsonkupik_dsp.cpp
        -> generated/arsonkupik_dsp_p0_3.cpp
```

The generated translation unit is the one compiled into SC220 Live, the reference renderer and the Live renderer. Generation is fail-closed: if upstream source anchors drift, configuration fails rather than silently compiling a partial sonic update.

## Audio improvements now carried by SC220 Live

### P0 — musical rebalance / rounder authority

- broader low foundation rather than narrow attack emphasis;
- Signature sub glide at 52 Hz;
- deeper 78 Hz bloom and rounder 118 Hz torque;
- 170/185 Hz acoustic/chest support;
- stronger Core and Living Body foundations;
- warm-particle cushion and reduced warm transient guard;
- static Signature output trim of -0.35 dB;
- linked full-band Signature macro compressor disabled so enhancement can move without programme-level pumping.

### P0.1 — anti-ringing vocal presence

- broad presence geometry moved to the approved 2180 Hz / +0.40 dB / Q0.64 balance;
- Vocal Presence gain law restrained and Q moved to 0.95;
- Vocal Clarity moved to 3500 Hz / Q1.05 with restrained gain;
- P6 Presence Mid reduced to 1.04x;
- removes the residual ~2.3 kHz spotlight without sacrificing chest/throat authority or upper clarity.

### P0.2 — sparkling / 3D air

- P6 Tickle Mid 1.21x;
- air articulation modulation 0.030;
- skin articulation modulation 0.035;
- bounded Smart Side Air authority increase;
- generated 3D air increased approximately 8–10%;
- existing 6.2 kHz generated-air and 7.8 kHz Side-Air high-pass protection remains authoritative;
- scene-space, correlation, anti-phase, density and brightness guards remain active.

### P0.3 — Smart Warm Vocal Enhancement

P0.3 adds a second, bounded lower-body-led identity route to the existing Smart Warm Vocal Body detector. It is designed for warm/dark/chest-led vocals that can sit behind bright accompaniment even when a normal 1.5–3 kHz presence boost would be the wrong fix.

The new detector reuses existing envelopes only. It adds no FFT, filter, compressor, audio lane or realtime allocation. It requires a presence floor, musical throat/body energy, centred low-mid energy and a Core-safe condition so kick/bass or wide low-mid material do not receive vocal-like warm bloom.

The new route supplements, rather than replaces, the existing presence-led detector. It changes detection coverage, not the global vocal wet amount or static upper-mid gain.

## Presets

SC220 Live compiles `engine/arsonkupik_presets.cpp` from the same P0.3-pinned authority. All 13 current factory profiles remain available through the existing `EnhancerEngineBridge` API.

The Signature profile now carries the current P0 calibration, including the round-bass foundation, 575 Hz warm-body bridge, P0.1 presence geometry, sparkle/detail settings and -0.35 dB Signature Balanced output trim.

## Existing SC220-specific behaviour retained

- headless ASK-P processor; ASK-P editor and Orb visualizer remain excluded;
- local `sonkupik-sphere-v0.10.4` remains the only visualizer;
- SC220 Live control glide remains 12 ms;
- 80 ms reference processor remains available for parity QA;
- ASK-P safety/telemetry bridge remains intact: input, pre-safety and output peaks/overs, safety gain reduction and Gain Match telemetry;
- SC220-specific licence storage isolation remains intact;
- final lookahead/ceiling policy and existing safety architecture remain owned by the ASK-P processor.

## Build and validation contract

CMake requires the exact checkpoint commit and the upstream P0.3 generator. It then verifies generated-DSP anchors for:

- `lowerBodyLedVocalTarget`;
- P6 Tickle Mid `1.21`;
- P0.2 generated 3D air `0.066 / 0.026 / 0.094`;
- Signature compressor bypass;
- warm transient guard `0.58`.

CI additionally checks the gitlink, port boundary, factory profile contract and, when the private ASK-P token is configured, generates and compiles the effective P0.3 DSP together with the current preset source.

## Release caution

This port is suitable for SC220 listening/build validation but should not be described as an upstream ASK-P production release. Before a public SC220 release based on this checkpoint, run the full Windows MSVC/audio-device acceptance matrix and listening checks, especially:

- warm/dark vocal foreground improvement without 2–3 kHz bite;
- no male-vocal boom or kick/bass false triggering;
- no return of 2.3 kHz ringing;
- stable round bass without programme pumping;
- sparkling air without fizz or centre-image instability;
- 48/96 kHz and 32/64/128-sample realtime stress tests.
