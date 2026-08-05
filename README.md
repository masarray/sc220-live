# SC220 Live

Native Windows audio console for Recording Tech SC220. SC220 Live combines an optional SC220 voice input with a selectable Windows playback endpoint, processes Voice and Music through independent ASK-P Signature engines, and sends the protected programme to VB-CABLE for OBS or another live application.

![SC220 Live console](docs/SC220_LIVE_UX_LOCKED.png)

## Product signal path

```text
Recording Tech SC220
  -> Input Trim -> Voice Signature -> Voice fader ---------------------+
                                                                        |
Selected Windows playback endpoint                                     +-> Master protection / -1 dB limiter
  -> WASAPI loopback -> Music Signature -> PC fader -> SC220 ducking --+       |
                                                                                +-> standard VB-CABLE -> OBS / live application
```

SC220 is optional for PC Music operation. The main programme graph can run with PC Audio only, and SC220 is selected automatically when a matching `MIXER`, `SC220`, or `Recording Tech` endpoint becomes available.

## Current implementation

- Native JUCE 8 / C++17 Windows 10/11 x64 application.
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

## Quick Stream routing

The public installer can optionally install the standard VB-Audio VB-CABLE donationware driver. The default release route uses that single cable only for the processed Stream Output:

```text
Music / browser / player
  -> normal Windows speaker or headphone
  -> SC220 Live PC AUDIO SOURCE captures that playback endpoint
  -> Voice + Music DSP and protection
  -> CABLE Input (VB-Audio Virtual Cable)
  -> OBS / live application selects CABLE Output
```

In this mode the user hears the normal Windows playback directly while the audience receives the processed SC220 Live programme. Keep the SC220 Live Monitor Bus off unless a different physical endpoint is available.

A physical playback endpoint cannot be both the captured PC Audio source and the processed Monitor Output. Doing that plays the original application directly and returns a delayed DSP copy to the same endpoint, causing doubling or recursive feedback. Processed-only monitoring requires a second independent endpoint or virtual cable that is not bundled with the public installer.

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

## Verified installer bundle

The installer pipeline downloads only the official standard `VBCABLE_Driver_Pack45.zip`, verifies its reviewed SHA-256 and Authenticode signatures, then embeds `VBCABLE_Setup_x64.exe` unchanged as an optional task.

Local build:

```bat
set VBCABLE_PACK45_SHA256=<reviewed 64-character SHA-256>
build-sc220-installer.bat 0.1.0 clean
```

GitHub Actions workflow:

```text
SC220 Live installer bundle
```

Required repository configuration:

- secret `ASKP_REPO_TOKEN` with read access to `masarray/askp-vst`;
- repository variable `VBCABLE_PACK45_SHA256` containing the reviewed official Pack45 hash.

The workflow:

1. checks out the pinned private ASK-P authority;
2. installs pinned Inno Setup 6.7.1;
3. downloads VB-CABLE only from the official VB-Audio URL;
4. rejects a hash mismatch, invalid setup signature, invalid driver catalogue, or A/B/C/D package material;
5. builds the SC220 Live Release executable;
6. creates a Windows 10/11 x64 installer with optional VB-CABLE installation;
7. smoke-installs and uninstalls the application payload without changing drivers on the CI runner;
8. publishes the installer, SHA-256, and supply-chain manifest as workflow artifacts.

The installer presents VB-Audio ownership and donationware terms, keeps the driver task optional, allows the Windows security prompt to remain visible, and requests a restart after successful VB-CABLE installation.

## Verified public release

Public release remains fail-closed. It must originate from a clean reviewed `main` commit, pass the physical listening test, and publish only compiled assets and approved end-user documents to `masarray/sc220-download`.

SC220 Live is proprietary closed-source software. Public packages never contain application source, DSP source, CMake files, debug symbols, credentials, or private keys.

## Repository boundary

- `masarray/sc220-live` — private canonical source, release scripts, design authority, installer workflow, and private DSP submodule reference.
- `masarray/sc220-download` — public landing page, checksums, manifests, and compiled release assets only.

See [docs/RELEASE_BOUNDARY.md](docs/RELEASE_BOUNDARY.md) and [docs/PUBLIC_RELEASE_GATE.md](docs/PUBLIC_RELEASE_GATE.md).

## Safety notes

- VB-CABLE is a separate VB-Audio donationware product and is installed only when the user selects that installer task.
- Keep Stream and Monitor muted while selecting endpoints and setting gain.
- Never select the same endpoint or virtual-cable family as both PC Audio Source and Stream/Monitor destination.
- Restart Windows after installing or uninstalling VB-CABLE.
- Avoid red meter segments and verify the full signal path before a production broadcast.
