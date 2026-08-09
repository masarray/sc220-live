# SC220 Live implementation checklist

Updated: 2026-08-05  
Branch: `fix/pc-cable-source-analyzer`

Status legend:

- `[x]` implemented in source and covered by repository audit
- `[~]` implemented in source, but still needs local Windows or hardware validation
- `[ ]` not implemented yet

## Product and audio architecture

- [x] Native JUCE/C++ application foundation
- [x] Pinned private ASK-P v0.5.23 engine authority
- [x] Separate Voice Signature and Music Signature engine instances
- [x] SC220 Input Trim confined to the Voice lane
- [x] PC Music enhancement occurs before voice-keyed ducking
- [x] Voice and Music meet only after independent lane processing
- [x] Protected Master Bus with final trim and `-1 dB` limiter
- [x] Separate Voice, Music, and Master gain-reduction telemetry
- [x] Separate Voice and Music DSP-latency telemetry
- [x] Primary programme graph can run output-only without SC220 attached
- [x] SC220 `MIXER` is auto-selected when it becomes available
- [x] Dual Mono transport no longer discards SC220 channel 2
- [ ] Independent gain, mute, pan, and meter controls for both Dual Mono channels

## Input and routing

- [x] SC220 presence is reported from the real endpoint name, not a fake connected state
- [x] Device names matching `MIXER`, `SC220`, or `Recording Tech` are recognised as SC220
- [~] Automatic selection of the SC220 `MIXER` input needs hardware validation
- [x] PC Music is captured through Windows WASAPI loopback
- [x] Windows render endpoints are enumerated using stable endpoint IDs and friendly names
- [x] Selectable PC playback/loopback endpoint in the main PC Audio dropdown
- [x] `SYSTEM DEFAULT` remains available when safe relative to Stream and Monitor Output
- [x] Selected PC playback endpoint is persisted and restored before audio starts
- [x] Physical playback endpoints can be selected as PC Music sources when they are not also Monitor Output
- [x] VB-CABLE and VB-CABLE A/B/C/D can be selected as PC Music sources
- [x] Different virtual-cable families may be used for PC Music and Stream Output
- [x] The same endpoint or same virtual-cable family is blocked to prevent feedback
- [x] The active physical Monitor Output is withheld from the PC Audio source list
- [x] The active PC Audio source endpoint is withheld from the Monitor Output list
- [x] PC Audio Source = Monitor Output is rejected and monitoring is muted before recursive feedback can start
- [x] Stream selector remains restricted to virtual-cable endpoints
- [x] WASAPI startup failures display `CAPTURE ERROR / RETRYING` with the real error text
- [x] Event-driven WASAPI capture falls back to polling mode when an endpoint rejects event callback capture
- [x] Runtime capture failures recreate the selected endpoint automatically
- [x] PC capture FIFO, meter peaks, and monitor delay are cleared during endpoint switching
- [x] PC meters decay to `-120 dB` when no capture frames arrive instead of freezing at their last value
- [~] PC endpoint switching, persistence, reconnect, and anti-loop alerts need Windows hardware validation
- [ ] Adaptive asynchronous clock-drift correction for long-duration PC Audio capture

### Processed-monitor routing requirement

A physical render endpoint cannot be both PC Audio Source and processed-only Monitor Output. Windows already plays the producing application directly to that endpoint. Returning the DSP result to the same endpoint creates a delayed second copy, and WASAPI loopback can capture the returned monitor audio again.

Use distinct endpoint families:

```text
Audio-producing application -> VB-CABLE family A -> PC Audio Source
SC220 Live DSP              -> physical headphone -> Monitor Output
SC220 Live programme        -> VB-CABLE family B -> Stream Output / OBS
```

The producing application must be assigned to the chosen PC Audio source in Windows Volume Mixer or in the application's own output setting. Selecting a source inside SC220 Live chooses which endpoint to capture; it cannot redirect another application's Windows playback session.

## Music, ducking, and broadcast workflow

- [x] PC Music can operate without SC220 connected
- [x] PC Music receives ASK-P enhancement
- [x] Ducking is applied after Music Signature enhancement
- [x] Ducking is keyed from the SC220 Voice lane when SC220 is present
- [x] Ducking amount is user-adjustable
- [x] Enhanced PC Music can be heard through Program Monitor or Music Cue
- [ ] Voice-band detector filter, hysteresis, and hold timing
- [ ] Automatic/adaptive ducking threshold
- [ ] Dedicated compact Music Signature editor on the main surface
- [~] Music preset and all five Music Signature macro states are persisted

## Stream and physical monitoring

- [x] Independent physical Monitor Bus
- [x] Monitor endpoints exclude VB-CABLE, the active Stream endpoint, and the captured PC endpoint
- [x] Monitor starts muted for safety
- [x] Monitor power revalidates source-versus-monitor endpoint identity before unmuting
- [x] `PROGRAM` monitoring uses the protected final programme mix
- [x] `MUSIC CUE` monitors enhanced PC Music before ducking
- [x] Monitoring works independently from the `GO LIVE` Stream mute state
- [x] Monitoring works when SC220 is absent and PC Music is the only source
- [x] Monitor device, gain, mute state, and source mode are persisted
- [x] Native WASAPI shared-mode endpoint uses its real Windows mix format
- [x] Fixed preallocated stereo resampling buffers
- [x] Adaptive source/output sample-rate conversion using two Lagrange interpolators
- [x] FIFO-fill clock-drift correction is bounded
- [x] Monitor callback avoids blocking the broadcast callback
- [x] Monitor telemetry reports source rate, endpoint rate, resampling state, correction, underflow, and reconnect health
- [~] Direct headphone selection without entering Audio Setup needs Windows validation
- [~] Mismatched source/headphone sample rates need listening tests
- [~] Long-duration monitor drift and underrun behaviour needs a soak test

## Programme analyzer and metering

- [x] Analyzer rebuilt as a fixed `14`-band post-limiter filter bank
- [x] Fixed centre frequencies: `50`, `80`, `125`, `200`, `315`, `500`, `800`, `1.25k`, `2k`, `3.15k`, `5k`, `8k`, `12k`, `16k`
- [x] Each band uses a proportional-bandwidth second-order band-pass biquad
- [x] Exact protected post-limiter stereo buffer feeds the analyzer
- [x] Left and right channels are combined by stereo energy, preserving anti-phase content
- [x] Pink-noise calibration integrates 4096 log-frequency response points
- [x] Stable calibrated level uses one identical `180 ms` envelope for all bands
- [x] Separate per-band activity uses fast attack and frequency-dependent release
- [x] Pink-flat baseline is never modified by transient activity
- [x] UI applies equal dynamic-range expansion to all bands
- [x] Per-band pulse cap and peak hold
- [x] No FFT, sample queue, lock, wait, allocation, or drawing in the audio callback
- [x] Native pink-noise test spread: `0.348 dB`
- [x] Native rhythm test: 80 Hz burst rises and releases on the correct band
- [~] Musical rhythm response needs visual confirmation in the Windows executable
- [x] Stereo input, PC Audio, and Stream level meters
- [x] Integrated LUFS-I and true-peak telemetry
- [x] Needle and LED ring use responsive 400 ms LUFS-M
- [x] Centre number remains gated LUFS-I
- [x] Endpoint switching resets stale loudness history
- [~] Loudness needle movement needs final Windows visual confirmation

## UX and visual identity

- [x] Recording Tech / SC220 Live branding
- [x] Purple removed from the visual identity
- [x] Custom black title bar supports window dragging
- [x] Double-click title bar toggles maximise/restore
- [x] Lucide icons for titlebar, Settings, enhancer power, and monitor power
- [x] Smart Enhancer power/bypass control
- [x] `FREE 365 DAYS` and Details controls removed from the main surface
- [x] License and Activation remain available from Settings
- [x] One responsive grid controls SC220, PC Audio, and Stream fader geometry
- [x] Stream and Monitor selectors use separate responsive columns
- [x] PC Audio status distinguishes active capture, startup, routing blocked, and capture error/retry
- [x] Monitor status explains when source and monitor resolve to the same endpoint
- [~] Final layout needs visual confirmation at `1180x720`, `1600x900`, and maximised size
- [ ] Embed the final application typeface instead of relying on Windows fallback

## Build, release, and quality assurance

- [x] Proprietary/binary-only release checks
- [x] Safe muted Stream and Monitor startup
- [x] Clean build closes the preview executable before removing the build tree
- [x] Normal CMake builds compile the generated engine, UI, capture recovery, and application state
- [x] CI validates pink-flat response, rhythm response, loudness movement, endpoint routing, monitor conversion, and self-monitor safety
- [x] Source-audit run #388 passes all repository checks
- [x] Windows/MSVC run #388 compiles native `MonitorOutputClient.cpp` and generated `PcAudioLoopbackCapture_Recovery.cpp`
- [x] Windows lifecycle smoke starts the capture recovery thread, reports retry state on a no-audio-device runner, and shuts down cleanly
- [~] PC Music-only + distinct VB-CABLE source/stream + physical monitor test
- [~] SC220 `MIXER` + PC Music + distinct source/stream cables + physical monitor end-to-end test
- [ ] One-click 10-minute transport stability test
- [ ] Two-hour zero-dropout transport qualification
- [ ] Eight-hour broadcast soak test
- [ ] Installer and portable release packaging validation for this branch

## Current merge decision

Draft PR #2 should remain unmerged until these checks pass locally:

1. Windows Release build succeeds from a clean tree.
2. The producing application is routed to a PC Audio endpoint different from both Monitor Output and Stream Output.
3. Changing PC Audio Source restarts capture and reaches `PLAYBACK CAPTURE ACTIVE` without opening Audio Setup.
4. When capture has no frame, PC meters fall to silence and the analyzer does not retain stale bars.
5. Program Monitor and Music Cue both produce one continuous processed signal without doubling or recursive feedback.
6. A 44.1/48 kHz source/monitor combination plays without clicks or sample-rate errors.
7. Endpoint unplug/reconnect recovers automatically.
8. A one-hour live test completes without growing underflows, routing feedback, or analyzer-induced CPU spikes.
