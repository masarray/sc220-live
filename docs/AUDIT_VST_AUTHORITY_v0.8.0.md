# Sonkupik Live v0.8.0 - Visual, DSP and Realtime Audit

## Authority inspected

Requested reference: `masarray/askp-vstvisualizer`.

The connected GitHub installation did not expose a separate repository by that
name. The active implementation, liquid-visualizer history, performance releases,
engine and preset bank are in the private repository `masarray/askp-vst`.
Sonkupik Live v0.8.0 is locked to the audited source below:

- repository: `masarray/askp-vst`
- version: `0.5.14`
- commit: `655874dee6ee69df7212b8073ab941f9607d8faa`
- preset count: `40`
- engine blob: `f2f64bf032283d08a2d046b993134e2397c022b8`
- preset blob: `2e52397c5cb49fb6966b78913a69c36573413aae`
- visualizer blob: `9455d518987059c02c763eb24de9a23fa32307b7`

`ASKP_AUTHORITY.lock` is the machine-readable copy of this lock.

## Findings in Sonkupik Live v0.7.0

| Area | v0.7.0 finding | Risk |
|---|---|---|
| Smart Enhancer core | Vendored engine predates the latest performance release | Different tone/preset behaviour and more CPU |
| Presets | Vendored bank had 26 presets | Missing current 40-preset authority |
| Runtime contract | Used `preset_id` and did not enable wrapper-owned bypass/peak limiter flags | Unnecessary string work and double safety processing |
| Engine initialisation | Prepared engine was not reset after loading the new preset | Different deterministic headroom initial state |
| Visual hero | Main workspace used a denoise FFT spectrum | Visually unlike the current VST and duplicated FFT-derived work |
| Old VST visualizer source | Local snapshot used FFT 2048, 64 bands and 76 sparks | Too heavy for a live background utility |
| UI visual path | Denoise spectrum publication and graph painting duplicated work already unrelated to the product identity | Extra callback atomics and UI paint cost |
| Build surface | Linked `juce_audio_processors` and enabled VST3 hosting before hosting existed | Larger binary/startup surface with no user benefit |
| Denoise transform | JUCE FFT backend remained in the realtime denoise path | Avoidable framework lock/dispatch overhead |
| Parameter drag | Every macro update woke the builder immediately | Excess background rebuild churn |
| CPU telemetry | Only smoothed average callback load was shown | Short deadline spikes could remain invisible |

## v0.8.0 visual strategy

The current VST no longer uses an FFT spectrum inside the sphere. The live visual
surface follows the same lightweight architecture:

- 24 Hz message-thread refresh;
- audio thread performs only a lossy lock-free stereo tap copy;
- newest 4096 samples only;
- six broad musical time-domain bands;
- fixed 32-node spring/fluid lattice;
- fixed eight-bubble and twelve-droplet pools;
- no glitter field;
- no kick classifier, tempo detector or beat prediction;
- cached vessel, rim, front shade and top-gloss layers;
- dynamic repaint sleeps after the liquid settles;
- visual tap and UI timer stop while the main window is hidden in the tray.

The result is the current ArSonKuPik liquid identity without putting visual FFT,
allocation or UI drawing on the audio callback.

## v0.8.0 engine strategy

Sonkupik Live does not fork the Smart Enhancer recipe anymore. CMake compiles:

- `engine/arsonkupik_dsp.cpp`
- `engine/arsonkupik_presets.cpp`

straight from the authoritative `askp-vst` checkout supplied through
`SONKUPIK_ASKP_SOURCE_DIR`.

Runtime parameters now mirror the VST wrapper contract:

- numeric `preset_index`;
- preset-provided `smart_protect`;
- `external_bypass = true`;
- `external_peak_limiter = true` because Live owns the final limiter;
- `advanced_override = true`;
- configure first, then `reset()` the prepared engine.

The latest authority also brings the P1/P2/P3 performance work: cached runtime
coefficients, inactive colour-lane gates, block-boundary state sanitation, fused
wrapper passes, faster saturation and DSP-only fast floating-point contraction.

## v0.8.0 realtime changes

- Custom fixed-size radix-2 FFT for the 256/512 denoise stage, with numerical reference verification.
- No heap allocation, shared FFT lock or plan creation in the callback.
- Increment-and-wrap FIFO indices replace callback modulo operations.
- The legacy GUI spectrum publication path and its display atomics are removed entirely; the denoise transform now performs audio work only.
- Visual queue mirrors the current VST architecture: 16,384-sample SPSC, fixed-capacity and lossy by design; it drops visual data instead of blocking audio.
- Full prepared-engine rebuild notifications are coalesced for 28 ms (roughly 35 Hz) on the background builder, reducing allocator and CPU bursts during knob drag.
- Prepared-engine triple buffer and five-millisecond correlated crossfade remain.
- One redundant full-buffer copy was removed, four meter scans were fused into one pass, and callback peak deadline telemetry is shown beside average CPU.
- VST3 host modules are removed from the default Live Safe binary until scanner,
  crash isolation and watchdog bypass are ready.
- JUCE updated to 8.0.14 to match the authority toolchain.

## Layout audit

The retained layout is appropriate for the product goal:

- left: **Before Enhancer** rack, input fader and stereo meter;
- centre top: Musical Noise Clean controls plus the current liquid visualizer;
- centre bottom: factory preset and five Smart Enhancer macros;
- right: **After Chain** rack, output fader and stereo meter;
- top workspace: input and output device dropdowns only;
- settings callout: sample rate, buffer, channel mode and denoise quality;
- footer: average/peak callback load, total latency, dropouts, profile and tray state.

No timeline, large mixer, browser or permanently visible technical setup is added.

## Remaining anti-crackle risk

The application still depends on the Windows/JUCE device backend to bridge a
physical input interface and VB-CABLE output. These endpoints can have different
hardware clocks. v0.8.0 reduces application-side deadline risk, but a future native
`IAudioClient3` capture/render bridge with ring-fill drift correction is still the
strongest route for a formal long-duration anti-crackle guarantee.

Release acceptance therefore requires a Windows soak test, not only source audit:

1. 48 kHz, 128 samples, Live FFT 256.
2. Input audio interface and output VB-CABLE.
3. OBS or TikTok Studio capturing CABLE Output.
4. Main window open for 30 minutes, then tray mode for at least 8 hours.
5. `Dropouts = 0`, callback peak below 70%, no audible discontinuity.
6. Repeat at 256 samples when the vendor driver cannot sustain 128.
