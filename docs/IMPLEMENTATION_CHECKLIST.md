# SC220 Live implementation checklist

Updated: 2026-08-04  
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
- [x] `SYSTEM DEFAULT` remains available when it is safe relative to the Stream endpoint
- [x] Selected PC playback endpoint is persisted and restored before the audio graph starts
- [x] Physical playback endpoints can be selected as PC Music sources
- [x] VB-CABLE and VB-CABLE A/B/C/D can be selected as PC Music sources
- [x] Different virtual-cable families may be used for PC Music source and Stream Output
- [x] The exact same endpoint or same virtual-cable family is blocked to prevent digital feedback
- [x] Stream selector remains restricted to virtual-cable endpoints
- [x] An older saved physical Stream output is recovered toward VB-CABLE routing
- [x] WASAPI startup failures are displayed as `CAPTURE ERROR` with the actual error text
- [~] PC endpoint enumeration, cable-family naming, switching, persistence, and anti-loop alerts need Windows validation
- [ ] Adaptive asynchronous sample-rate and clock-drift correction for long broadcasts

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
- [~] Music preset and all five Music Signature macro states are persisted, but the final dedicated UX still needs validation

## Stream and monitoring

- [x] Independent physical Monitor Bus
- [x] Monitor endpoints exclude VB-CABLE and the active Stream endpoint
- [x] Monitor starts muted for safety
- [x] `PROGRAM` monitoring uses the protected final programme mix
- [x] `MUSIC CUE` monitors enhanced PC Music before ducking
- [x] Monitoring works independently from the `GO LIVE` Stream mute state
- [x] Monitoring works when SC220 is absent and PC Music is the only active source
- [x] Monitor device, gain, mute state, and source mode are persisted
- [~] Physical monitoring requires listening tests with headphones/speakers
- [ ] Asynchronous clock correction for the physical Monitor Bus

## Spectralizer and metering

- [x] Incorrect `OLDER -> NOW` history visualizer removed
- [x] Main visualizer is a real fixed-frequency OBS-style spectralizer
- [x] Frequency position is fixed from bass on the left to treble on the right
- [x] Protected post-master programme samples remain the preferred visual source
- [x] Music Signature and Voice Signature taps remain recovery sources
- [x] Direct WASAPI PC capture is an independent recovery source before the DSP FIFO
- [x] Direct capture has one producer and one UI consumer using a bounded lock-free queue
- [x] A full DSP FIFO cannot suppress direct WASAPI visual publication
- [x] Source priority is `POST-MASTER -> MUSIC RECOVERY -> PC CAPTURE -> VOICE RECOVERY`
- [x] The visible source label reports which path is actually supplying frames
- [x] `NO FRAMES` is distinguished from real signal silence
- [x] Left and right channels are analysed separately and combined by stereo energy
- [x] Anti-phase audio remains visible instead of cancelling through mono summing
- [x] Lightweight `512`-point FFT
- [x] FFT, smoothing, peak hold, and drawing run on the UI/message thread
- [x] No FFT, windowing, memory allocation, or visual processing is added to the audio callback
- [x] `24` logarithmically spaced bands from approximately `35 Hz` to `16 kHz`
- [x] `15` segmented cells per band, similar to common OBS visualizers
- [x] Bass display receives a small visual contour boost without changing audio
- [x] Mid and upper bands are intentionally calmer than bass
- [x] Fast attack and smooth release ballistics per band
- [x] Independent `500 ms` peak hold for every band
- [x] Approximately `30 fps` visual refresh
- [x] Frequency labels include `40`, `100`, `250`, `1k`, `4k`, and `12k`
- [~] Direct `PC CAPTURE` movement requires confirmation in the Windows executable
- [~] Preferred post-master spectrum movement requires Windows runtime confirmation
- [~] Realtime spectrum movement with SC220 voice requires hardware confirmation
- [x] Stereo input, PC Audio, and Stream level meters
- [x] Integrated loudness and true-peak programme telemetry

## UX and visual identity

- [x] Recording Tech / SC220 Live branding
- [x] Purple removed from the visual identity
- [x] Custom black title bar supports window dragging
- [x] Double-click title bar toggles maximise/restore
- [x] Lucide icons for minimise, maximise, close, settings, enhancer power, and monitor power
- [x] Smart Enhancer power/bypass control
- [x] `FREE 365 DAYS` button removed from the main surface
- [x] License and Activation remain available from Settings
- [x] Details button removed
- [x] Redundant output Ready block removed
- [x] One responsive grid controls SC220, PC Audio, and Stream fader geometry
- [x] Input Trim and Auto Duck use a symmetrical pre-fader row
- [x] Stream and Monitor selectors use separate responsive columns
- [x] Explicit `STREAM OUTPUT` and `MONITOR OUTPUT` labels
- [x] Explicit `VB-CABLE / OBS` and `HEADPHONES / SPEAKERS` routing descriptions
- [x] PC Audio source dropdown represents the selected Windows playback endpoint
- [x] PC Audio status distinguishes active capture, startup, routing blocked, and capture error
- [~] Final layout needs visual confirmation at `1180x720`, `1600x900`, and maximised size
- [ ] Embed the final application typeface instead of relying on Windows fallback
- [ ] Replace remaining legacy typography/assets in the mini controller

## Build, release, and quality assurance

- [x] Proprietary/binary-only release checks
- [x] 365-day licence and checkout contract checks
- [x] Safe muted Stream startup
- [x] Clean build closes the preview executable before removing the build tree
- [x] Bounded retry and lock-owner diagnostics for Windows clean builds
- [x] Normal CMake builds compile the generated PC capture authority and mark the base source header-only
- [x] Repository audits cover ASK-P authority, routing, UX, console geometry, Monitor Bus, PC-only operation, spectral response, anti-phase response, and direct WASAPI publication
- [x] CI verifies direct visual publication occurs before DSP FIFO free-space handling
- [x] CI verifies the FFT exists only in generated UI and not in capture/audio engine sources
- [x] CI verifies that the time-history visualizer is fully removed
- [x] GitHub Actions source audit passes for the direct-capture implementation
- [~] Full Windows/MSVC Release compile must pass locally after the latest commit
- [~] PC Music-only + distinct VB-CABLE source/stream + physical monitor test
- [~] SC220 `MIXER` + PC Music + VB-CABLE + physical monitor end-to-end test
- [ ] One-click 10-minute transport stability test
- [ ] Two-hour zero-dropout transport qualification
- [ ] Eight-hour broadcast soak test
- [ ] Automated audio continuity/drift test
- [ ] Installer and portable release packaging validation for this branch

## Next engineering milestone: Transport v2

- [ ] Independent capture/render clock-domain telemetry
- [ ] Target FIFO-fill controller for PC Audio
- [ ] Adaptive asynchronous sample-rate correction instead of fixed-ratio conversion
- [ ] Bounded correction range with reported ppm drift
- [ ] Declared underrun/overrun recovery counters
- [ ] Emergency frame insert/drop only after a declared transport fault
- [ ] Preserve DSP and endpoint state across device reopen
- [ ] Local endpoint capability dump for sample rate, period, and channel format
- [ ] `48 kHz / 32-bit float` baseline qualification
- [ ] Low mode remains default until Ultra mode passes the local endpoint probe
- [ ] Reconnect recovery target of `<= 2 s`
- [ ] Input-to-VB-CABLE latency validation target of `<= 12 ms`

## Current merge decision

Draft PR #2 should remain unmerged until these minimum checks pass:

1. Windows Release build succeeds from a clean tree.
2. The PC Audio state becomes `PLAYBACK CAPTURE ACTIVE` or shows a specific `CAPTURE ERROR`.
3. With capture active, `PROGRAM SPECTRUM` must show `POST-MASTER`, `MUSIC RECOVERY`, or `PC CAPTURE`, never permanent `NO FRAMES`.
4. Bass, mids, and treble produce visibly different fixed-frequency patterns.
5. Peak markers hold briefly and then fall without sticking.
6. Toggling Smart Enhancer power must audibly change the monitored app path, proving audio is not merely the Windows direct path.
7. A safe virtual source configuration works, for example Cable A as PC Music and Cable B as Stream.
8. Selecting the same cable for PC Music and Stream is blocked with a clear warning.
9. Enhanced PC Music is audible through Program Monitor and Music Cue.
10. SC220 is automatically added when `MIXER` becomes available.
11. A minimum one-hour live soak test completes without dropouts, FIFO growth, routing feedback, or visualizer-induced CPU spikes.
