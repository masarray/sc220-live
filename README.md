# SC220 Live

Native Windows audio console for Recording Tech SC220. SC220 Live combines an optional SC220 voice input with a selectable Windows playback endpoint, processes Voice and Music through independent ASK-P Signature engines, and sends the protected programme to VB-CABLE while providing a separate physical Monitor Bus.

![SC220 Live console](docs/SC220_LIVE_UX_LOCKED.png)

## Product signal path

```text
Recording Tech SC220
  -> Input Trim -> Voice Signature -> Voice fader ---------------------+
                                                                        |
Selected Windows playback endpoint                                     +-> Master protection / -1 dB limiter
  -> WASAPI loopback -> Music Signature -> PC fader -> SC220 ducking --+       |                    |
                                                                                |                    +-> physical headphones / speakers
                                                                                +-> VB-CABLE -> OBS / live application
```

SC220 is optional for PC Music operation. The main programme graph can run with PC Audio only, and SC220 is selected automatically when a matching `MIXER`, `SC220`, or `Recording Tech` endpoint becomes available.

## Current implementation

- Native JUCE 8 / C++17 Windows x64 application.
- Pinned private ASK-P v0.5.23 authority at commit `5757119cab7a1a75f09dcf0edea6e3be97975439`.
- Independent Voice Signature and Music Signature processor instances.
- Selectable Windows WASAPI loopback source for PC Audio.
- Event-driven capture with polling fallback, automatic endpoint retry, and honest silence telemetry.
- SC220-keyed PC Audio ducking after Music Signature processing.
- Protected final programme bus with trim and a `-1 dB` limiter.
- Native OBS-style WASAPI shared-mode Monitor Bus with endpoint-native format conversion.
- `PROGRAM` and pre-ducking `MUSIC CUE` monitoring modes.
- Fixed 14-band post-limiter programme analyzer with pink-noise calibration and per-band rhythm activity.
- Integrated LUFS-I, responsive 400 ms LUFS-M needle movement, true peak, and stereo meters.
- Stream and Monitor start muted; explicit user actions enable them.

## Routing rules

A physical playback endpoint cannot be both the captured PC Audio source and the processed Monitor Output. Doing that plays the original application directly and returns a delayed DSP copy to the same endpoint, causing doubling or recursive feedback.

Recommended processed-monitor route:

```text
Music application -> VB-CABLE family A -> PC AUDIO SOURCE
SC220 Live DSP     -> physical headphone -> MONITOR OUTPUT
SC220 Live program -> VB-CABLE family B -> STREAM OUTPUT / OBS
```

Assign the music application to the selected source cable in Windows Volume Mixer or in the application's own output setting. Selecting a source inside SC220 Live chooses what to capture; it cannot redirect another application's Windows playback session.

## Clone the private source authority

```powershell
git clone --recurse-submodules https://github.com/masarray/sc220-live.git
cd sc220-live
```

The ASK-P repository is private. Your GitHub account or CI token must have access before the submodule can initialise.

## One-click Windows build

Double-click [build-sc220-windows.bat](build-sc220-windows.bat), or run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-sc220-one-click.ps1 -Configuration Release -Clean
```

Requirements:

- Windows 10 or 11 x64;
- Visual Studio with **Desktop development with C++** and a Windows SDK;
- Git for Windows;
- internet access on the first build unless a local JUCE source tree is configured.

## Verified public release

Double-click [release-sc220-windows.bat](release-sc220-windows.bat), optionally passing a semantic version:

```bat
release-sc220-windows.bat 0.1.0
```

The release pipeline fails closed. It:

1. verifies repository visibility, proprietary licence, EULA, purchase terms, and production licence-service contract;
2. performs a clean Release build using the pinned private ASK-P authority;
3. packages only the executable and end-user documents;
4. rejects source, debug, secret, certificate, and compiler artifacts;
5. publishes a portable ZIP and SHA-256 checksum to the public `masarray/sc220-download` repository;
6. updates the public landing page and verifies the resulting GitHub Release.

SC220 Live is proprietary closed-source software. Public packages never contain application source, DSP source, CMake files, debug symbols, credentials, or private keys.

## Repository boundary

- `masarray/sc220-live` — private canonical source, release scripts, design authority, and private DSP submodule reference.
- `masarray/sc220-download` — public landing page, checksums, and compiled release assets only.

See [docs/RELEASE_BOUNDARY.md](docs/RELEASE_BOUNDARY.md) and [docs/PUBLIC_RELEASE_GATE.md](docs/PUBLIC_RELEASE_GATE.md).

## Safety notes

- Install VB-CABLE separately from its official publisher; SC220 Live does not bundle or silently install audio drivers.
- Keep Stream and Monitor muted while selecting endpoints and setting gain.
- Never select the same endpoint or virtual-cable family as both PC Audio Source and Stream/Monitor destination.
- Avoid red meter segments and verify the full signal path before a production broadcast.
