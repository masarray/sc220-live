# Sonkupik Live v0.11.0 — P0 Stability and Visual Hierarchy

## Engine P0

The v0.11.0 Stability Guard adds application-level protection around the JUCE audio device backend:

- OS/native xrun count through `AudioDeviceManager::getXRunCount()`.
- Callback deadline measurement against the current block budget.
- Callback-start gap monitoring for scheduling stalls.
- A conservative 128-block decision window.
- Automatic move to the next supported buffer after repeated timing pressure.
- A 30-second recovery cooldown to prevent restart loops.
- A 128-sample safe floor when separate input/output endpoints are selected.
- `STABLE`, `GUARDED`, `WATCH`, `RECOVERING`, and `MANUAL` states in the footer.

The guard does not allocate, lock, enumerate devices, or restart the engine on every callback. The audio thread only updates fixed counters and requests one asynchronous message-thread recovery after the decision threshold is crossed.

JUCE/WASAPI remains responsible for the actual Windows endpoint transport. This milestone strengthens observability and recovery around that backend; it does not claim to replace WASAPI with a custom native dual-client resampler.

## DSP hot-path cleanup

- Expander time constants are prepared once per sample-rate change.
- Expander sample access now uses fixed channel pointers instead of repeated `getSample()`/`setSample()` calls.
- Output peak metering uses `AudioBuffer::getMagnitude()` rather than a handwritten scalar scan.
- No recipe, threshold, ratio, timing constant, preset, limiter, or enhancer policy was intentionally changed.

## Visual P0

- Side panels reduced from 240 px to 220 px.
- Centre processor workspace receives more width.
- Rack rows use a two-line module/state hierarchy so long names cannot collide with state text.
- Smart Sound Enhancer is compacted to a 266–286 px card.
- Noise Clean receives the remaining vertical priority and a wider 72 px expander meter.
- Muted text and soft borders are raised slightly for small-label readability.
- Footer `Tray Mode` is replaced by an actual real-time `Stability` state.

## Acceptance test

1. Select the real audio interface as input.
2. Select VB-CABLE as output.
3. Start at 128 or 256 samples.
4. Confirm footer state is `GUARDED`.
5. Run OBS/TikTok Studio and stream audio for at least eight hours.
6. Record deadline misses, driver xruns, automatic recoveries, and final buffer size.
7. A buffer increase is acceptable only after repeated pressure; it must not oscillate.
