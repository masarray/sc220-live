# Sonkupik Live — Product Strategy

## Product definition

Sonkupik Live is a lightweight, low-latency live audio rack for streamers, singers,
karaoke operators, podcasters, live sellers, houses of worship and small studios that
currently open a full DAW only to enhance audio before OBS or TikTok Live Studio.

It is intentionally **not a DAW**. There is no timeline, multitrack editor, MIDI editor or
large mixer. The product promise is:

> Select the audio interface, learn the static noise, choose a signature sound, and send a
> polished, protected signal to VB-CABLE with minimal setup and CPU load.

## Primary signal path

```text
Audio Interface Input
  -> Musical Noise Clean
     -> static learned spectral reduction
     -> pre-enhancer activity detector
  -> ArSonKuPik Smart Enhancer
  -> reserved external VST3 post slot
  -> smooth downward expander gain
  -> locked final safety limiter
  -> CABLE Input (VB-Audio Virtual Cable)
  -> CABLE Output selected in OBS / TikTok Live Studio
```

## Core user outcome

1. Audio sounds clearly better than raw input.
2. Static hiss is reduced while music remains natural.
3. Silent gaps become truly quiet without a gate-like open/close sound.
4. The app continues running when the main window is hidden to the tray.
5. Device state, preset and noise profile workflow are obvious to a non-technical user.
6. The product remains substantially lighter and faster than opening a DAW.

## Product boundaries

### Included

- Stereo and dual-mono live processing.
- 48 kHz first-class workflow.
- WASAPI/JUCE device bridge in the first release.
- VB-CABLE output detection and readiness state.
- Learned static-noise profile.
- Musical spectral reduction.
- Smooth stereo-linked downward expansion.
- Shared ArSonKuPik DSP engine.
- Locked final output protection.
- Main control panel, compact controller and system tray lifecycle.
- Reserved rack architecture for external VST3 plugins.

### Explicitly excluded

- Timeline and arrangement.
- Recording/editing workstation features.
- Large multibus mixer.
- Custom kernel audio driver.
- Public driver signing costs.
- Automatic microphone replacement or OS-wide recording-device changes.
- Unlimited plugin slots.

## Differentiation

- Faster to understand than a DAW.
- Lower visual and operational complexity.
- Built-in signature enhancement rather than an empty plugin host.
- Music-safe profile denoise rather than speech-only AI suppression.
- Smooth expander completes the cleanup without hard gating.
- Beginner mode remains compact; advanced controls stay available but secondary.
- Safety limiter cannot be accidentally moved behind an unsafe plugin.

## Release priorities

1. Stability for multi-hour live use.
2. No clicks on enable, bypass, device restart or parameter changes.
3. Predictable latency and dropout reporting.
4. Musical noise cleanup.
5. Device recovery after unplug, sleep and resume.
6. External VST3 hosting only after scanning and crash safety are mature.

## Target experience

```text
Install app + VB-CABLE
Open Sonkupik Live
Choose audio interface
Output automatically prefers CABLE Input when available
Press Learn Noise while the source is silent
Select Mas Ari Signature
OBS/TikTok receives CABLE Output
Close the window; engine remains active in tray
```

## Commercial and licensing direction

The project does not create or sign a new kernel audio driver. VB-CABLE is treated as a
third-party dependency. Before public packaging, verify the current VB-Audio redistribution
terms, preserve the VB-Audio product identity and donation/licensing notice, and do not bundle
paid cable variants without explicit permission.
