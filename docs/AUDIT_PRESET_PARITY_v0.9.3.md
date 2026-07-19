# Sonkupik Live v0.9.3 — P2 Preset and Engine Audio Parity

## Objective

Verify that the Smart Sound Enhancer embedded in Sonkupik Live reaches exactly the same settled audio output as the authoritative `askp-vst` processor for every factory preset.

This audit deliberately separates two questions:

1. **Enhancer parity:** the `askp::Processor` audio result must match.
2. **Whole Live rack sound:** Musical Noise Clean, Expander, input routing, output routing and device conversion are additional stages and may intentionally change the result.

## Authority boundary

Sonkupik Live compiles these files directly from the locked `askp-vst` checkout:

- `engine/arsonkupik_dsp.cpp`
- `engine/arsonkupik_dsp.hpp`
- `engine/arsonkupik_presets.cpp`
- `src/PluginProcessor.cpp`
- processor support sources required by its vtable/editor linkage

The only intentional processor-source overlay remains the P1 control glide:

```cpp
// askp-vst
const double ramp = 0.08;

// Sonkupik Live
const double ramp = 0.012;
```

That delta affects movement time only. When parameters are established before `prepareToPlay()`, both smoothers begin on the same current value, so settled preset rendering must be bit-identical.

## Deterministic render test

P2 adds two independent console executables:

- `SonkupikParityReference` — compiles the original authoritative `PluginProcessor.cpp`.
- `SonkupikParityLive` — compiles generated `PluginProcessor_Live.cpp`.

Separate executables avoid C++ symbol/ODR collisions between two classes with the same `askp::Processor` identity.

For each of the 40 factory presets, both renderers:

1. instantiate a new processor;
2. select the same stereo bus layout;
3. apply the factory preset and its macro values before preparation;
4. use 48 kHz, 256 samples, single precision;
5. keep Input Gain and Final Output Gain at 0 dB;
6. keep wrapper bypass off;
7. render the same deterministic five-second stereo music/transient signal;
8. discard the first three seconds for engine/WOW warm-up;
9. fingerprint every remaining float sample using FNV-1a 64-bit;
10. record RMS, peak, latency and analysed sample count.

The comparison passes only when every preset has:

- the same ID and display name;
- the same reported latency;
- the same analysed sample count;
- the same output hash;
- RMS and peak deltas no greater than `1e-12`.

## Manual VST reference conditions

The Settings card now includes **Match VST Reference**. It applies:

- Musical Noise Clean bypassed;
- Smooth Expander bypassed;
- Input Trim 0 dB;
- Output Trim 0 dB;
- stereo input mode;
- Smart Sound Enhancer active;
- output unmuted.

The disabled denoiser keeps its fixed latency line, so recorded A/B files must be aligned by the total latency shown in the footer. After alignment, only the enhancer output is being compared.

## Run the P2 test on Windows

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\run-p2-parity.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -Clean
```

The script builds the application plus both optional renderers, renders all presets and writes:

```text
qa-results/p2-parity/askp-vst-reference.json
qa-results/p2-parity/sonkupik-live.json
qa-results/p2-parity/p2-preset-parity.txt
```

A successful report ends with:

```text
Presets checked: 40
Passed: 40
Failed: 0
Result: PASS
```

## What this test does not claim

It does not prove that two different physical device paths are sample-identical. WASAPI/ASIO drivers, operating-system sample-rate conversion, VB-CABLE settings, input channel selection, and the Live-only denoise/expander stages remain outside the embedded enhancer parity boundary.
