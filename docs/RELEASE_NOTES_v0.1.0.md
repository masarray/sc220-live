# SC220 Live v0.1.0

Initial public Windows 10/11 x64 release candidate for Recording Tech SC220.

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
- Windows x64 Inno Setup installer with optional standard VB-Audio VB-CABLE donationware installation.
- Installer SHA-256 and supply-chain manifest recording the exact source commit and VB-CABLE package/signature identity.
- Optional portable binary-only ZIP with SHA-256 checksum, EULA, purchase terms, quick start, and third-party notices.

## Quick Stream routing

The bundled standard VB-CABLE is used only as the processed Stream Output:

```text
Music / browser / player
  -> normal Windows speaker or headphone
  -> SC220 Live captures that playback endpoint
  -> ASK-P DSP, ducking, mix, and protection
  -> CABLE Input
  -> OBS or live application selects CABLE Output
```

The user hears normal Windows playback directly while the audience receives the processed SC220 Live programme. Keep the SC220 Monitor Bus off unless a genuinely different physical endpoint is selected. Returning processed audio to the same endpoint being captured would create direct-plus-DSP doubling or recursive feedback.

## Installer and VB-CABLE

- Windows 10/11 x64 only.
- VB-CABLE is developed and owned by VB-Audio Software and remains a separate donationware product.
- The driver task is optional and can be deselected when VB-CABLE is already installed.
- The build pipeline downloads only the official `VBCABLE_Driver_Pack45.zip`, verifies a reviewed SHA-256, validates the Authenticode setup signature and Windows driver catalogues, and embeds `VBCABLE_Setup_x64.exe` unchanged.
- The bundle does not contain VB-CABLE A+B or C+D.
- Administrator permission, Windows driver consent, and a restart are required when installing VB-CABLE.
- Uninstalling SC220 Live does not silently uninstall VB-CABLE.

## Release qualification

Publication is allowed only after the exact release commit passes every mandatory check in `docs/PUBLIC_RELEASE_GATE.md`, including:

- clean full build with the pinned private ASK-P authority;
- verified installer, checksum, and supply-chain manifest;
- application-only installer smoke test;
- physical Quick Stream routing test through standard VB-CABLE;
- safe Monitor test on a different endpoint;
- ten-minute playback, switching, silence-decay, and reconnect tests;
- final current UI screenshot review.
