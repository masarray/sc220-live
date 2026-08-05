# SC220 Live v0.1.0

Initial public Windows x64 release candidate for Recording Tech SC220.

## Highlights

- Independent Voice Signature and Music Signature processing using the pinned ASK-P v0.5.23 authority.
- Optional SC220 Voice input with automatic recognition of `MIXER`, `SC220`, and `Recording Tech` endpoints.
- Selectable Windows PC Audio source through resilient WASAPI loopback capture.
- SC220-keyed PC Audio ducking after Music Signature enhancement.
- Protected programme bus with final trim and `-1 dB` limiter.
- OBS-style native WASAPI Monitor Bus with `PROGRAM` and `MUSIC CUE` modes.
- Physical monitor sample-rate conversion using the endpoint native Windows mix format.
- Fixed 14-band rhythmic programme analyzer with calibrated pink-noise baseline and peak hold.
- Integrated LUFS-I, responsive LUFS-M movement, true peak, and stereo metering.
- Premium lean release UI with engineering/debug copy removed from the main surface.
- Portable binary-only ZIP with SHA-256 checksum, EULA, purchase terms, quick start, and third-party notices.

## Required routing

For processed PC Music monitoring without echo, use distinct endpoint families:

```text
Music application -> VB-CABLE family A -> PC AUDIO SOURCE
SC220 Live DSP     -> physical headphone -> MONITOR OUTPUT
SC220 Live program -> VB-CABLE family B -> STREAM OUTPUT / OBS
```

Do not select the same physical endpoint as both PC Audio Source and Monitor Output. The application blocks unsafe same-endpoint and same-cable-family routes where possible.

## Platform

- Windows 10/11 x64
- Portable executable
- VB-CABLE installed separately from the official VB-Audio publisher
- No audio driver is installed or bundled by SC220 Live

## Release qualification

Publication is allowed only after the exact release commit passes the mandatory checks in `docs/PUBLIC_RELEASE_GATE.md`, including a clean full build with the pinned private ASK-P authority and a physical ten-minute routing/monitoring test.
