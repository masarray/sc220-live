# Sonkupik Live — Architecture

## Repository structure

```text
engine/                      Shared Smart Enhancer DSP source
src/                         Existing VST wrapper and common Plus Jakarta look-and-feel
live/
  LiveApplication.cpp        App lifecycle, settings, main window, tray, compact controller
  LiveMainComponent.*        Premium control surface and device UX
  LiveAudioEngine.*          Real-time device bridge and native processing chain
  MusicalNoiseClean.*        Learned denoise, expander and final safety limiter
  LiveTheme.*                Dark premium visual system
  LiveIcons.*                Embedded Lucide SVG loader/buttons
  LiveWidgets.*              Knobs, meters, spectrum, rack and status components
assets/fonts/                Embedded Plus Jakarta Sans
assets/icons/                Vendored Lucide SVG subset and ISC licence
docs/                        Product, DSP, UX, safety, testing and roadmap source of truth
scripts/                     Windows build and packaging helpers
qa/                          Audit reports
```

## Runtime signal path

```text
Audio interface input
  -> channel mapper (stereo or mono-to-dual-mono)
  -> input trim (Before Enhancer fader)
  -> 24 Hz gentle subsonic cleanup
  -> learned short-FFT Musical Noise Clean
  -> pre-enhancer detector copy
  -> ArSonKuPik Smart Enhancer
  -> smooth downward-expander gain
  -> output trim (After Chain fader)
  -> locked linked safety limiter
  -> selected output, normally CABLE Input
  -> OBS / TikTok selects CABLE Output
```

The Noise Clean dry bypass is delayed by the same deterministic FFT latency as the wet path.
This avoids timing jumps in bypass and allows `Hear Removed` to subtract aligned wet audio from
aligned dry audio.

## Runtime modules

### LiveAudioEngine

Owns `juce::AudioDeviceManager` and implements `AudioIODeviceCallback`.

Responsibilities:

- Open one input and one output device.
- Prefer a VB-CABLE output on first run when available.
- Keep all audio buffers allocated before callbacks begin.
- Copy mono input to dual mono when selected.
- Execute the fixed native processing chain.
- Build changed Smart Enhancer state off the audio thread.
- Publish meters, CPU estimate, latency and dropout status.
- Restart the device without restarting the application.

### MusicalNoiseClean

A causal short-FFT processor with deterministic latency.

- Live mode: FFT 256, hop 128.
- Clean mode: FFT 512, hop 256.
- Square-root Hann analysis/synthesis windows.
- 50% overlap-add.
- Stereo-linked spectral gain.
- Conservative bounded reduction.
- User-learned static profile.
- Time/frequency smoothing and transient/tonal protection.
- Two-stage one-pole high-pass around 24 Hz before the spectral stage.

Changing quality invalidates the old profile because FFT bins no longer correspond. The app
restarts audio to commit the new prepared configuration, then the user learns noise again.

### SmoothDownwardExpander

Receives detector audio after spectral cleaning but before enhancement. Its gain is applied
after the enhancer, so noise re-raised by enhancement is reduced while detector activity is
not distorted by compressor/makeup movement.

### ArSonKuPik Smart Enhancer handoff

The app compiles the same `engine/arsonkupik_dsp.cpp` and presets as the VST. Because preset and
macro changes can rebuild filters/vectors, the live wrapper never performs that work in the
callback.

Three fixed engine slots are used:

1. Active engine processes the current signal.
2. A background thread prepares a free engine with the latest parameter snapshot.
3. The pending engine is atomically handed to the callback.
4. Old and new engines process the same input and crossfade for about 5 ms.
5. The old engine becomes a free slot.

The third slot permits another prepared update without overwriting an engine currently used by
the crossfade.

### LiveSafetyLimiter

Always final in the native chain. It catches enhancement and future external-plugin peaks. It
is intentionally not movable or bypassable.

## Thread model

### Audio thread

Allowed:

- Fixed-size buffer operations.
- FFT on preallocated arrays.
- Atomic reads/writes.
- DSP state updates.
- Prepared-engine pointer/slot handoff.

Forbidden:

- File access, GUI calls or logging.
- Blocking locks.
- Plugin scanning/instantiation.
- Heap allocation in steady-state processing.
- Smart Enhancer parameter/topology rebuild.

### Engine builder thread

- Reads the latest atomic parameter snapshot.
- Claims a free engine slot.
- Constructs/prepares/rebuilds that engine away from the callback.
- Publishes one pending slot atomically.
- Coalesces fast knob movements by revision number.

### Message thread

- UI painting/timers.
- Device selectors and restart actions.
- Tray menus and compact controller.
- Settings persistence.
- Future plugin scanning and editor windows.

## Device strategy

First release uses JUCE audio-device abstraction, normally Windows Audio/WASAPI. The code
supports separate input/output endpoint names where the active backend allows it.

Recommended operating point:

- Audio interface input: 48 kHz.
- App: 48 kHz.
- CABLE Input: 48 kHz.
- OBS/TikTok: 48 kHz.
- Buffer: 128 samples default, 256 fallback.

ASIO input plus WASAPI/VB-CABLE output is a later expert mode because those endpoints have
independent clocks. It must not ship until adaptive drift measurement and resampling are implemented.

## Failure handling

- Output is cleared before every callback.
- Shared DSP sanitises invalid samples.
- Oversized callbacks remain silent and increment dropouts rather than reallocating.
- Device restart is available in the main settings and tray menu.
- Window close hides to tray; it does not destroy audio processing.
- App is single-instance.
- Settings are persisted on shutdown.
