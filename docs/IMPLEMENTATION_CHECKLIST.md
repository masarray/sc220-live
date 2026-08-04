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
- [x] `SYSTEM DEFAULT` remains available when safe relative to Stream Output
- [x] Selected PC playback endpoint is persisted and restored before audio starts
- [x] Physical playback endpoints can be selected as PC Music sources
- [x] VB-CABLE and VB-CABLE A/B/C/D can be selected as PC Music sources
- [x] Different virtual-cable families may be used for PC Music and Stream Output
- [x] The same endpoint or same virtual-cable family is blocked to prevent feedback
- [x] Stream selector remains restricted to virtual-cable endpoints
- [x] WASAPI startup failures display `CAPTURE ERROR` with the real error text
- [~] PC endpoint switching, persistence, and anti-loop alerts need Windows validation
- [ ] Adaptive asynchronous sample-rate and clock-drift correction for PC Audio capture

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
- [x] Monitor endpoints exclude VB-CABLE and the active Stream endpoint
- [x] Monitor starts muted for safety
- [x] `PROGRAM` monitoring uses the protected final programme mix
- [x] `MUSIC CUE` monitors enhanced PC Music before ducking
- [x] Monitoring works independently from the `GO LIVE` Stream mute state
- [x] Monitoring works when SC220 is absent and PC Music is the only source
- [x] Monitor device, gain, mute state, and source mode are persisted
- [x] Monitor no longer requires the physical endpoint to match the Stream sample rate
- [x] Monitor first tries the preferred Stream rate, then the endpoint native Windows rate
- [x] Nearest supported endpoint rate is selected when Windows initially reports `0 Hz`
- [x] Fixed preallocated stereo resampling buffers
- [x] Adaptive source/output sample-rate conversion using two Lagrange interpolators
- [x] FIFO-fill clock-drift correction is bounded to ±2500 ppm
- [x] Monitor callback remains lock-free and allocation-free
- [x] Monitor telemetry reports source rate, endpoint rate, resampling state, and correction ppm
- [~] Direct headphone selection without entering Audio Setup needs Windows validation
- [~] 44.1 kHz Stream to 48 kHz headphone and 48 kHz Stream to 44.1 kHz endpoint need listening tests
- [~] Long-duration monitor drift and underrun behaviour needs a soak test

## Programme analyzer and metering

- [x] All failed `ScopeTap`, FFT, snapshot, and recovery visualizers removed
- [x] Analyzer rebuilt as a fixed `14`-band post-limiter filter bank
- [x] Fixed centre frequencies: `50`, `80`, `125`, `200`, `315`, `500`, `800`, `1.25k`, `2k`, `3.15k`, `5k`, `8k`, `12k`, `16k`
- [x] Each band uses a proportional-bandwidth second-order band-pass biquad
- [x] Exact protected post-limiter stereo buffer feeds the analyzer
- [x] Left and right channels are combined by stereo energy, preserving anti-phase content
- [x] Pink-noise calibration integrates 4096 log-frequency response points
- [x] Stable calibrated level uses one identical `180 ms` envelope for all bands
- [x] Separate per-band activity uses fast attack of approximately `6–12 ms`
- [x] Separate per-band activity uses release of approximately `55–110 ms`
- [x] Activity ignores normal stationary ripple below `1.5 dB`
- [x] Activity can add up to approximately three temporary visual segments
- [x] Pink-flat baseline is never modified by transient activity
- [x] UI applies equal dynamic-range expansion to all bands
- [x] Per-band pulse cap and approximately `470 ms` peak hold
- [x] `16` segmented cells per band at approximately `30 fps`
- [x] No FFT, sample queue, lock, wait, allocation, or drawing in the audio callback
- [x] Native pink-noise test spread: `0.348 dB`
- [x] Native rhythm test: 80 Hz burst rises and releases on the correct band
- [~] Musical rhythm response needs visual confirmation in the Windows executable
- [~] Realtime analyzer response with SC220 voice requires hardware confirmation
- [x] Stereo input, PC Audio, and Stream level meters
- [x] Integrated LUFS-I and true-peak telemetry
- [x] Needle and LED ring use responsive 400 ms LUFS-M
- [x] Centre number remains gated LUFS-I
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
- [x] Normal CMake builds compile the generated engine and final generated UI
- [x] CI validates pink-flat response, rhythm response, loudness movement, endpoint routing, and monitor-rate conversion contracts
- [x] CI run #320 passes all repository checks
- [~] Full Windows/MSVC Release compile must pass locally after the latest commit
- [~] PC Music-only + distinct VB-CABLE source/stream + physical monitor test
- [~] SC220 `MIXER` + PC Music + VB-CABLE + physical monitor end-to-end test
- [ ] One-click 10-minute transport stability test
- [ ] Two-hour zero-dropout transport qualification
- [ ] Eight-hour broadcast soak test
- [ ] Installer and portable release packaging validation for this branch

## Current merge decision

Draft PR #2 should remain unmerged until these checks pass locally:

1. Windows Release build succeeds from a clean tree.
2. A 44.1 kHz headphone endpoint can be selected while Stream runs at 48 kHz, without opening Audio Setup.
3. A 48 kHz endpoint can monitor a 44.1 kHz Stream configuration.
4. Program Monitor and Music Cue both produce continuous audio without periodic clicks or FIFO resets.
5. Music produces visibly stronger per-frequency attacks than the previous analyzer version.
6. Pink noise remains visually flat after settling.
7. LUFS-M drives the needle/ring while LUFS-I remains the central value.
8. A one-hour live test completes without monitor underflow growth, routing feedback, or analyzer-induced CPU spikes.
