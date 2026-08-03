# SC220 Live implementation checklist

Updated: 2026-08-03  
Branch: `agent/dual-source-broadcast-engine`

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
- [x] Dual Mono transport no longer discards SC220 channel 2
- [ ] Independent gain, mute, pan, and meter controls for both Dual Mono channels

## Input and routing

- [x] SC220 presence is reported from the real endpoint name, not a fake connected state
- [x] Device names matching `MIXER`, `SC220`, or `Recording Tech` are recognised as SC220
- [~] Automatic selection of the SC220 `MIXER` input needs hardware validation
- [x] PC System Mix is captured through Windows loopback
- [x] Stream selector is restricted to virtual-cable endpoints
- [x] An older saved physical Stream output is recovered toward VB-CABLE routing
- [ ] Selectable PC playback/loopback endpoint
- [ ] Self-loop validation between the selected PC loopback endpoint and Stream endpoint
- [ ] Adaptive asynchronous sample-rate and clock-drift correction for long broadcasts

## Music, ducking, and broadcast workflow

- [x] PC Music receives ASK-P enhancement
- [x] Ducking is applied after Music Signature enhancement
- [x] Ducking is keyed from the SC220 Voice lane
- [x] Ducking amount is user-adjustable
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
- [x] Monitor device, gain, mute state, and source mode are persisted
- [~] Physical monitoring requires listening tests with headphones/speakers
- [ ] Asynchronous clock correction for the physical Monitor Bus

## Spectrum and metering

- [x] Static response line removed
- [x] Spectrum uses a real FFT implementation
- [x] Spectrum source is the protected post-master programme bus
- [x] Analyzer publication is lock-free and allocation-free on the audio callback
- [x] Lightweight `1024`-point FFT
- [x] Approximately `15 fps` visual refresh
- [x] `72` display bins
- [~] Realtime spectrum movement still requires confirmation in the Windows executable with actual PC audio and SC220 input
- [x] Stereo input, PC Audio, and Stream level meters
- [x] Loudness estimate and output peak display
- [ ] Standards-compliant Integrated LUFS, Short-Term LUFS, LRA, and oversampled true peak

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
- [~] Final layout needs visual confirmation at `1180x720`, `1600x900`, and maximised size
- [~] Inter is requested by the UI but is not embedded in the executable
- [ ] Replace remaining Plus Jakarta assets/usages in the mini controller

## Build, release, and quality assurance

- [x] Proprietary/binary-only release checks
- [x] 365-day licence and checkout contract checks
- [x] Safe muted Stream startup
- [x] Clean build closes the preview executable before removing the build tree
- [x] Bounded retry and lock-owner diagnostics for Windows clean builds
- [x] Repository source audits for ASK-P authority, dual-source routing, premium UX, console geometry, Monitor Bus, and clean build
- [~] Full Windows/MSVC Release compile must pass on the local machine after the latest commit
- [~] SC220 `MIXER` + VB-CABLE + physical monitor end-to-end test
- [ ] Eight-hour broadcast soak test
- [ ] Automated audio continuity/drift test
- [ ] Installer and portable release packaging validation for this branch

## Current merge decision

The branch should remain a Draft PR until these minimum checks pass:

1. Windows Release build succeeds from a clean tree.
2. Spectrum moves with PC Music and SC220 input.
3. Stream output is VB-CABLE and Monitor output is a physical endpoint.
4. Program and Music Cue monitoring are heard correctly.
5. Layout is visually inspected at minimum, default, and maximised window sizes.
6. A minimum one-hour live soak test completes without dropouts, FIFO growth, or routing feedback.
