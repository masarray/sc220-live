# External VST3 Hosting Roadmap

## Current stage: trusted manual voice VST3 insert

Windows builds now compile JUCE ASIO support and provide one input-channel VST3 slot.
The SC220 input FX menu offers manual selection, a plugin editor, bypass and removal.
The plugin receives stereo voice after noise cleaning and expander, before voice enhancement.
The engine removes the audio callback before plugin replacement and reprepares the loaded
effect on device restart. Reported VST latency is included in the voice-path estimate.

This is **not** a production-ready arbitrary plugin rack. The scanner subprocess,
crash isolation/recovery, persistent plugin state, blacklisting and click-free bypass
in the gate below are outstanding. Users should load only trusted plugins (e.g. their own
ArSonVerb). Input and output on ASIO must share one driver/clock; ASIO-to-WASAPI
independent routing requires additional resampling/drift control.

## Product intent

External plugins extend Sonkupik Live into a compact live rack, but the app must not become a
fragile DAW clone. The first UI reserves one pre-enhancer and one post-enhancer slot. They are
visibly marked as roadmap items until the safety requirements below are complete.

## Required architecture

```text
Input
 -> Native Musical Noise Clean
 -> External VST3 Pre Slot
 -> Native ArSonKuPik Enhancer
 -> External VST3 Post Slot
 -> Native Smooth Expander gain
 -> Locked Native Safety Limiter
 -> VB-CABLE
```

## Release gate

Do not enable external VST loading until all of these exist:

1. Separate scanner executable/process.
2. Dead-man file and persistent blacklist.
3. VST3 x64 only for the first release.
4. Bus-layout validation for mono/stereo effects.
5. Sample-rate and block-size prepare lifecycle.
6. Plugin state save/restore.
7. Safe crossfaded bypass.
8. Per-plugin reported latency and total chain latency.
9. CPU measurement and overload warning.
10. Editor window lifecycle independent from audio processing.
11. Recovery when a plugin fails during load.
12. Session loads without blocking the audio callback.

## Real-time model

Low-latency mode runs approved plugins in-process. This is the lowest-overhead path but a plugin
crash can terminate the app.

A later sandbox mode can host untrusted plugins out of process. It requires IPC audio buffers,
state transport and additional buffering, so it should never be the default low-latency mode.

## Plugin classification shown to users

- Live Safe — zero/low reported latency and stable test history.
- Adds Latency — displays samples and milliseconds.
- High CPU.
- Unsupported I/O.
- Blacklisted after scan/load crash.
- Not validated.

## Scope control

- Start with two slots.
- Do not implement arbitrary graph routing in the first release.
- The safety limiter remains locked at the end.
- Linear-phase, long look-ahead and convolution plugins are allowed only with a clear live-latency warning.
