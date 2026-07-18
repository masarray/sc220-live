# Real-Time Safety and Stability Requirements

## Audio callback rules

The callback must not:

- allocate memory;
- read or write files;
- call GUI functions;
- scan or instantiate external plugins;
- block on a mutex;
- construct log strings;
- perform network operations;
- rebuild preset topology or filter vectors.

## Current implementation

Implemented:

- Processing, detector and transition buffers allocated in `audioDeviceAboutToStart`.
- FFT objects and fixed arrays allocated before processing.
- Output cleared before every callback.
- Oversized unexpected blocks fail silent and increment the dropout counter.
- UI status is transferred through atomics; only device-name strings use a message-thread lock.
- Spectral processor has fixed deterministic latency and an aligned dry delay.
- FFT quality changes are committed only by `prepare()` through an explicit audio restart.
- Smart Enhancer preset/macro rebuild happens on `EngineBuilderThread`, not the audio callback.
- Three engine slots prevent the builder from overwriting active or transitioning engines.
- A prepared pending engine is activated by the callback and crossfaded for about 5 ms.
- Noise Clean, expander and final limiter remain separate rack stages.
- Safety limiter is always final and non-bypassable.

Still requiring real-hardware hardening:

- Device hotplug and Windows sleep/resume endurance testing.
- Validation against unusually large or changing device callback block sizes.
- OS-level scheduler/MMCSS tuning and measurement.
- CPU percentage is a callback-budget estimate, not an OS scheduler measurement.
- Total latency includes device-reported latency and native denoise latency; future VST latency
  must be added to it.
- External plugins will require their own no-blocking lifecycle and crash safeguards.

## Parameter handoff model

```text
Message/UI thread changes atomics
  -> increments parameter revision
  -> background builder prepares a free Smart Enhancer engine
  -> pending slot published atomically
  -> audio callback activates it
  -> old/new engines crossfade for ~5 ms
  -> old slot becomes free
```

The callback never calls `ArSonKuPikEngine::set_runtime_params()`.

## Stability test matrix

- 8-hour continuous audio at 48 kHz / 128 samples.
- 8-hour fallback at 48 kHz / 256 samples.
- Repeated Learn Noise while audio is active and silent.
- 500 enhancer bypass toggles with click detection.
- 500 preset and macro changes while audio runs.
- Repeated Live 256 / Clean 512 changes with audio restart.
- Unplug/replug input interface.
- Disable/re-enable VB-CABLE.
- Sleep/resume.
- Change sample rate and buffer.
- Minimise/hide/show the UI repeatedly.
- CPU contention while OBS encodes.

## Acceptance criteria

- Zero crash.
- Zero NaN/Inf output.
- No sustained dropout loop.
- No audible transition click above the defined test threshold.
- No ambiguous device/engine state in the UI.
- Noise cleanup remains stereo stable.
- Expander never behaves like a hard gate at default settings.
- Safety ceiling is never exceeded by the native chain.
