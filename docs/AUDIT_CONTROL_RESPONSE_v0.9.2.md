# Sonkupik Live v0.9.2 — P1 Control-Response Audit

## Symptom

Creative controls changed visually at once, but the audible result followed later, especially while a knob was continuously dragged.

## Audited path

```text
KnobControl::Slider::onValueChange
  -> LiveAudioEngine::setEnhance / setSmartBass / ...
  -> APVTS parameter setValueNotifyingHost
  -> askp::Processor::parameterChanged
  -> APVTS atomic raw value
  -> askp::Processor::processBlock
  -> juce::SmoothedValue target
  -> bounded coefficient retune
  -> ArSonKuPikEngine::set_runtime_params
```

No background engine builder, message-queue delay, mutex, or heap allocation remains in this control path.

## Root cause

The authoritative askp-vst processor initializes all continuous controls with an 80 ms linear glide:

```cpp
const double ramp = 0.08;
```

This is safe and polished for DAW automation, but during repeated mouse events each new target continues to move ahead of the smoothed value. The control therefore feels detached even though the raw parameter is published immediately.

The 400 Hz coefficient-retune ceiling is not the main delay. At normal live buffer sizes, the callback/block rate is already near or below that ceiling:

- 128 samples at 48 kHz: 375 callbacks/s;
- 256 samples: 187.5 callbacks/s;
- 480 samples: 100 callbacks/s.

Therefore coefficients can update on every callback at the common 128–480 sample settings.

## Correction

CMake reads the locked authoritative `PluginProcessor.cpp`, checks for the audited 80 ms source anchor, and writes a generated `PluginProcessor_Live.cpp` with one intentional delta:

```cpp
const double ramp = 0.012;
```

The original private askp-vst checkout is never edited.

CMake stops with a fatal error when the anchor is missing. This prevents an automatic textual overlay from being silently applied to an unknown future processor revision.

## What remains identical

- DSP engine source and filter topology;
- all 40 factory presets and macro values;
- macro-to-runtime mapping;
- Smart Protect policy;
- 400 Hz maximum creative coefficient-retune rate;
- dual-engine preset transition and priming;
- WOW calibration and deterministic-level policy;
- latency-aligned bypass;
- one-millisecond lookahead safety and final ceiling;
- final value reached by every knob.

Only the travel time between old and new user-selected macro values changes.

## Expected response at 48 kHz

Because coefficient changes occur at block boundaries, total perceived response is the 12 ms glide plus callback quantisation.

| Buffer | Block time | Expected complete response |
|---:|---:|---:|
| 64 | 1.33 ms | about 12–13.3 ms |
| 128 | 2.67 ms | about 12–13.3 ms |
| 256 | 5.33 ms | about 12–16 ms |
| 480 | 10.00 ms | about 20 ms; first block reaches ~83% |
| 512 | 10.67 ms | about 21.3 ms |

The previous 80 ms setting required roughly 80–90.7 ms after the final target update, and continuous dragging could make the perceived trailing motion longer.

## Acceptance test

1. Build Release with `-Clean`.
2. Verify CMake prints the 12 ms live-control overlay status.
3. Use 48 kHz and 128 or 256 samples where stable.
4. Play a steady full-range music loop.
5. Move each creative knob quickly between low and high positions.
6. Confirm an audible response begins in the next callback and settles in roughly 12–20 ms.
7. Stop moving the knob and compare the settled sound to askp-vst at the same preset and macro values.
8. Confirm there is no zipper noise, click, dropout, or callback-peak spike.

