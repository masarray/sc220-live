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

## Programme analyzer and metering

- [x] All previous spectrum implementations were deleted from production
- [x] Removed `ScopeTap`, FFT, snapshot, direct-WASAPI visual queue, and recovery fallbacks
- [x] New analyzer was built from zero as a `14`-band fixed filter bank
- [x] Fixed centre frequencies: `50`, `80`, `125`, `200`, `315`, `500`, `800`, `1.25k`, `2k`, `3.15k`, `5k`, `8k`, `12k`, `16k`
- [x] Each band uses a proportional logarithmic bandwidth derived from adjacent centre frequencies
- [x] Each band uses a real second-order band-pass biquad
- [x] The exact protected post-limiter stereo buffer feeds the analyzer
- [x] Left and right channels are filtered separately and combined by stereo energy
- [x] Anti-phase stereo remains visible
- [x] Pink-noise compensation is calculated from the actual digital filter response at the active sample rate
- [x] Pink calibration integrates `4096` log-frequency response points
- [x] Cosmetic bass and treble display contour removed
- [x] Analyzer applies `180 ms` power-domain smoothing independent of callback block size
- [x] Native C++ synthetic pink-noise test measures `0.348 dB` maximum band spread at `48 kHz`
- [x] Audio callback publishes only atomic dB values and one generation counter
- [x] No sample queue, copied frame, FFT, lock, wait, allocation, or drawing in the callback
- [x] UI reads atomic band values directly at approximately `30 fps`
- [x] Fast attack and smooth release visual ballistics
- [x] Independent `600 ms` peak hold per band
- [x] `14` segmented bars with fixed bass-to-treble positions
- [x] Visible status reports `PINK-FLAT BANDS #<generation>`
- [~] Pink-noise flatness requires confirmation using the actual Windows pink-noise source and executable
- [~] Realtime analyzer response with SC220 voice requires hardware confirmation
- [x] Stereo input, PC Audio, and Stream level meters
- [x] BS.1770 K-weighted integrated loudness (`LUFS-I`)
- [x] Responsive `400 ms` momentary loudness (`LUFS-M`) for the moving needle and LED ring
- [x] Main loudness number remains gated `LUFS-I`
- [x] Lower loudness readout shows `M <LUFS-M> / TP <dBTP>`
- [x] Native test verifies LUFS-M moves from `-30` to `-12` and LUFS-I settles near `-12.2`
- [~] Loudness needle/ring movement requires confirmation in the Windows executable

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

## System-wide Windows processing options

- [x] Current application and VB-CABLE route do not require an SC220-owned kernel driver
- [x] Microsoft APO architecture assessed as the correct official user-mode system-effect path
- [ ] Clean-room SC220 SFX/MFX/EFX APO implementation
- [ ] Signed Windows `AudioProcessingObject` / `SoftwareComponent` INF package
- [ ] Hardware Support App or desktop control bridge for APO parameters
- [ ] HLK and endpoint compatibility qualification
- [ ] Decide whether system-wide APO mode is a separate optional product component
- [ ] Do not reuse Equalizer APO GPLv2 source in proprietary SC220 binaries without a deliberate licensing decision

## Build, release, and quality assurance

- [x] Proprietary/binary-only release checks
- [x] 365-day licence and checkout contract checks
- [x] Safe muted Stream startup
- [x] Clean build closes the preview executable before removing the build tree
- [x] Bounded retry and lock-owner diagnostics for Windows clean builds
- [x] Normal CMake builds compile the generated engine and final generated UI
- [x] Repository audits cover ASK-P authority, routing, UX, console geometry, Monitor Bus, PC-only operation, calibrated filter response, anti-phase response, atomic publication, and momentary loudness
- [x] CI verifies analyzer processing occurs after the master limiter
- [x] CI verifies production engine no longer publishes analyzer sample queues
- [x] CI verifies FFT, snapshot, and recovery paths are absent
- [x] CI verifies proportional band centres and synthetic bass/mid/treble localisation
- [x] CI compiles and executes native pink-flat and moving-loudness tests
- [x] GitHub Actions source audit run `#274` passes
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
2. Pink noise produces a visually flat analyzer within approximately `±0.5 dB` after settling.
3. `PINK-FLAT BANDS #<generation>` continues increasing while programme audio runs.
4. Loudness needle and LED ring visibly follow `LUFS-M`, while the main number remains `LUFS-I`.
5. Bass, mids, and treble produce visibly different fixed-band patterns with programme material.
6. Peak markers hold briefly and then fall without sticking.
7. Toggling Smart Enhancer power audibly changes the monitored app path.
8. A safe virtual source configuration works, for example Cable A as PC Music and Cable B as Stream.
9. Selecting the same cable for PC Music and Stream is blocked with a clear warning.
10. Enhanced PC Music is audible through Program Monitor and Music Cue.
11. SC220 is automatically added when `MIXER` becomes available.
12. A minimum one-hour live soak test completes without dropouts, routing feedback, or analyzer-induced CPU spikes.
