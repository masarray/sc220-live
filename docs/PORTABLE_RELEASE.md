# SC220 Live v0.1.2 portable preview — Linux and macOS

> **Preview / functional limitations.** These packages are for native build and hardware
> acceptance testing. They are **not** feature-equivalent to the Windows 10/11
> v0.1.2 release and are **not** a public production release.

## Supported package formats

| Target | Build | Package |
| --- | --- | --- |
| Ubuntu 24.04 x86_64 (and compatible desktop distributions) | Native JUCE 8, GCC/Ninja | `SC220-Live-v0.1.2-portable-preview.N-linux-x64.tar.gz` |
| macOS 11+ on Intel and Apple Silicon | Universal JUCE 8, Xcode/Ninja | `SC220-Live-v0.1.2-portable-preview.N-macos-universal.dmg` |

The macOS application is **ad-hoc signed only** and **not notarized** by Apple.
Do not advertise it as a notarized/signed production installer. The Linux package
is a portable archive, not a system-installed .deb/AppImage.

## Audio capabilities and limitations

- JUCE's device manager provides native audio I/O and the compiled SC220 voice
  processing path on each host, subject to compatible drivers and devices.
- **PC Audio music capture is not implemented on either platform.** The Windows
  `PcAudioLoopbackCapture` uses WASAPI. On non-Windows it explicitly reports
  unavailable. Do not assume selecting BlackHole, JACK or PipeWire automatically
  enables PC music capture.
- **Independent hardware Monitor Bus is not implemented on either platform.**
  `MonitorOutputClient` uses a Windows WASAPI backend and returns unavailable on
  macOS/Linux.
- The UI and automatic device detection were originally designed for Windows.
  Selected SC220 hardware and all output routing need physical verification.
- Do not route output to the same physical speaker/input path as the microphone,
  since this can cause feedback. Start muted and verify routing at low gain.
- OBS, BlackHole, JACK/PipeWire and other virtual routing arrangements are
  user-installed third-party components. This package installs no drivers.
- The approved private ASK-P v0.5.24 DSP source is still pinned to commit
  `2c764f82b0fc99eaff04d918cef4f839db7509e5`; this pipeline
  does not update its authority.
- A successful compile and headless CTest run **do not** certify real-device
  performance, acceptable live latency, or platform audio parity.

## Installation / acceptance testing

### Linux

Extract the `.tar.gz` archive, then run `./SC220\ Live` from inside its
directory in a desktop graphical session. The host needs compatible ALSA/JACK
libraries and drivers. Select an appropriate output in the application before
enabling audio. Linux test coverage starts with Ubuntu 24.04 x64.

### macOS

Mount the `.dmg` and copy `SC220 Live.app` to Applications. This preview does
not have Apple Developer ID signing/notarization; macOS Gatekeeper may prevent
normal first launch. Only run if you trust the verified source and artifact.
Grant Microphone permission if macOS requests it. Both Intel and Apple Silicon
slices must be checked on the actual machine.

### Acceptance checklist

1. Verify the matching `.sha256` for the archive/disk image.
2. Open the application and enumerate actual audio input/output devices.
3. Select SC220 (or a test USB interface), monitor at low volume and verify
   voice fader, bypass and mute.
4. Exercise sample-rate changes, disconnect/reconnect and shutdown/reopen.
5. Measure end-to-end latency and check dropouts, CPU use and feedback safety.
6. Confirm PC Audio and independent monitor limitations above before release
   approval; do not mark this preview as equivalent to Windows.

## Official public CI and release boundary

The code stays in the **private** `masarray/sc220-live` source repository.
The **only distribution CI and GitHub Releases authority** is the existing
**public** `masarray/sc220-download` repository.

The supported path is:
1. Review and merge the source changes into private `sc220-live/main`.
2. In public `sc220-download`, configure `SC220_SOURCE_TOKEN` with
   read-only access to the private `sc220-live` and `askp-vst` repositories.
3. Run the public `.github/workflows/build-portable-release.yml` with the exact
   immutable private source SHA and unused
   `v0.1.2-portable-preview.N` tag.
4. Check both native build results and compiled CTest audits.
5. On the public CI, explicitly select `publish_release=true` to upload
   **binary-only** assets to **sc220-download Releases** as a prerelease.

No private-repository release is created. No source files, driver installers,
debug symbols, build directories or signing keys enter public distribution.
The public Windows release workflow, current v0.1.2 stable release,
`latest.json`, and production website promotion gates remain unchanged.
Live hardware testing and platform-specific monitor/capture implementation
are still required before calling these versions production-ready.
