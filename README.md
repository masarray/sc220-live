# SC220 Live

Native Windows audio console for the Recording Tech SC220. SC220 Live combines the physical SC220 input and the default Windows playback mix, applies source-aware mixing and the pinned ASK-P Signature DSP, then sends the finished stream to VB-CABLE.

![Locked SC220 Live UX](docs/SC220_LIVE_UX_LOCKED.png)

## Product signal path

```text
Recording Tech SC220 (stereo or dual mono)
  -> input trim -> noise conditioning -> SC220 fader -----------+
                                                                |
Windows default playback (WASAPI loopback)                       +-> ASK-P Signature v0.5.23
  -> PC fader -> SC220-keyed ducking ---------------------------+   -> master safety/trim
                                                                    -> CABLE Input (VB-CABLE)
                                                                    -> OBS / TikTok Live Studio
```

The main surface only exposes controls backed by the engine. The monitor control is deliberately disabled until an independent physical monitor client is implemented.

## Current implementation

- JUCE 8 / C++17 native desktop application.
- Pinned `masarray/askp-vst` authority at commit `5757119cab7a1a75f09dcf0edea6e3be97975439`.
- One hardware capture path for SC220 and one native WASAPI shared-loopback path for PC Audio.
- Lock-free FIFO transfer from PC loopback capture to the real-time render callback.
- SC220-keyed PC Audio ducking with smoothed gain transitions.
- ASK-P Signature preset bank and five real macro parameters.
- Master mute starts safe; explicit **GO LIVE** enables output.
- Approximate 400 ms BS.1770-style loudness indication and stereo peak meters.
- Output selector verifies VB-CABLE naming and exposes device setup when needed.

## Clone the private source authority

```powershell
git clone --recurse-submodules https://github.com/masarray/sc220-live.git
cd sc220-live
```

The ASK-P repository is private. Your GitHub account must have access before the submodule can initialise.

## One-click Windows build

Double-click [build-sc220-windows.bat](build-sc220-windows.bat). It checks the pinned DSP submodule, detects Visual Studio/MSVC and CMake, obtains JUCE through the configured CMake path when necessary, and builds a Release executable.

For a clean rebuild:

```bat
build-sc220-windows.bat clean
```

Requirements:

- Windows 10 or 11 x64;
- Visual Studio with **Desktop development with C++** and a Windows SDK;
- Git for Windows;
- internet access on the first build unless `JUCE_DIR` or `third_party/JUCE` is available.

## One-click verified release

Double-click [release-sc220-windows.bat](release-sc220-windows.bat), optionally passing a semantic version from Command Prompt:

```bat
release-sc220-windows.bat 0.1.1
```

The release pipeline fails closed. It:

1. confirms valid `gh` authentication and repository visibility;
2. verifies the binary-only distribution approval gate;
3. builds Release and packages only the executable, quick start, and third-party notices;
4. rejects source, debug, secret, certificate, and compiler artifacts;
5. commits and pushes source to the private `masarray/sc220-live` repository;
6. publishes the portable ZIP and SHA-256 file to `masarray/sc220-download`;
7. pushes only the static download surface to the public repository;
8. enables GitHub Pages and reads back the published release assets.

Before the first binary-only public release, confirm that Recording Tech / MasArray has commercial or alternative distribution rights for JUCE and every included component. Then create the ignored local file `.release-license-approval.txt` containing exactly:

```text
I HAVE RIGHTS TO DISTRIBUTE SC220 LIVE AS BINARY ONLY
```

This is intentionally not committed. The existing GPL-3.0 notice must not be treated as permission to withhold corresponding source from binary recipients.

## Repository boundary

- `masarray/sc220-live` — private canonical source, build scripts, design lock, and DSP submodule reference.
- `masarray/sc220-download` — public landing page, checksums, and compiled GitHub Release assets only. No application `.cpp`, headers, CMake files, PDBs, libraries, credentials, or private keys.

See [docs/RELEASE_BOUNDARY.md](docs/RELEASE_BOUNDARY.md) for the release contract.

## Safety notes

- Install VB-CABLE separately from its official publisher; SC220 Live does not bundle or silently install audio drivers.
- Keep **GO LIVE** off while selecting devices and setting gain.
- Avoid red meter segments and validate the complete stream path before a production broadcast.
- This project currently uses independent hardware capture and Windows loopback clocks; long-running drift recovery remains a transport-hardening milestone.
