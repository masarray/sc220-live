# SC220 Live public release gate

The first public binary must not be published merely because source audits pass. A release is approved only when every mandatory item below is complete for the exact commit being packaged.

## Automated requirements

- [x] Source and generated-code audits pass.
- [x] Windows MSVC WASAPI monitor/capture smoke target compiles and runs.
- [x] Proprietary/binary-only package boundary checks exist.
- [x] Production licence-service and product-contract checks exist in the release script.
- [x] Public repository is separate from the private source authority.
- [x] Inno Setup installer contract enforces Windows 10/11 x64 and Administrator elevation.
- [x] Standard VB-CABLE is an optional visible donationware task; A+B and C+D are rejected.
- [x] Installer workflow requires the official Pack45 URL, reviewed SHA-256, valid Authenticode setup signature, and valid driver catalogues.
- [x] CI smoke-installs and uninstalls the application payload without modifying audio drivers.
- [ ] Repository variable `VBCABLE_PACK45_SHA256` contains the reviewed current official Pack45 hash.
- [ ] Full `SC220 Live.exe` clean Release build succeeds with the pinned private ASK-P v0.5.23 authority.
- [ ] Installer EXE, `.sha256`, and supply-chain manifest are produced from the same commit.
- [ ] Portable ZIP and `.sha256` are produced from the same commit when a portable package is also published.
- [ ] Public packages contain only compiled binaries, approved end-user documents, and the unchanged verified standard VB-CABLE setup.

## Mandatory Quick Stream listening test

The bundled standard VB-CABLE is used only for processed Stream Output:

```text
Music/player -> normal Windows speaker/headphone -> PC AUDIO SOURCE loopback
SC220 Live protected programme -> CABLE Input
OBS/live application -> CABLE Output
```

- [ ] PC Audio reaches the analyzer and programme without SC220 connected.
- [ ] OBS or the target live application receives the processed programme from `CABLE Output`.
- [ ] The normal Windows speaker/headphone continues to provide direct local listening without SC220 Monitor doubling.
- [ ] SC220 Monitor is blocked or kept off when it would return the processed signal to the same captured physical endpoint.
- [ ] `PROGRAM` Monitor produces one processed signal when a genuinely different physical endpoint is selected.
- [ ] `MUSIC CUE` produces enhanced PC Music before ducking on a safe separate endpoint.
- [ ] Changing PC Audio Source reaches `PLAYBACK CAPTURE ACTIVE` without opening Audio Setup.
- [ ] When the source stops or changes, PC meters and analyzer return to silence instead of freezing.
- [ ] 44.1 kHz source to 48 kHz stream/monitor, or the inverse, plays without a sample-rate error.
- [ ] Unplug/reconnect of the selected source or safe monitor endpoint recovers automatically.
- [ ] Ten-minute playback completes without clicks, recursive feedback, growing underflows, or dropouts.

## Installer qualification

- [ ] Installer refuses Windows older than Windows 10 and all 32-bit Windows installations.
- [ ] VB-CABLE task is visible, optional, selected only by user choice, and identifies VB-Audio ownership and donationware status.
- [ ] Windows driver-security consent remains visible; no automation attempts to bypass it.
- [ ] Successful VB-CABLE installation requests a restart and does not launch SC220 Live before that restart.
- [ ] Existing VB-CABLE users can deselect the driver task and install only SC220 Live.
- [ ] After restart, `CABLE Input` and `CABLE Output` appear and the Quick Stream route works.
- [ ] Uninstalling SC220 Live does not silently uninstall or modify VB-CABLE.
- [ ] Installer manifest records exact source commit, installer SHA-256, official VB-CABLE URL/hash, and signer identity.

## Release presentation

- [ ] Latest refined UI is visually checked at `1180x720`, `1600x900`, and maximised size.
- [ ] No engineering copy remains on the main surface (`PINK-FLAT`, generation counters, FPS, social target legend, or endpoint debug text).
- [ ] Final current screenshot replaces `docs/SC220_LIVE_UX_LOCKED.png` before publishing.
- [x] README describes Quick Stream with one standard VB-CABLE and the anti-echo boundary.
- [x] Installer shows the VB-Audio donationware notice and official support/donation links.
- [x] Public landing page describes Stream Output and safe local monitoring.
- [ ] Release notes match the exact version, commit, installer hash, and bundled VB-CABLE hash.

## Publication rule

Do not merge the release-candidate PR and do not publish the installer until all mandatory unchecked items above have been validated against the exact release commit.

Build the installer candidate locally with:

```powershell
$env:VBCABLE_PACK45_SHA256 = '<reviewed official Pack45 SHA-256>'
.\scripts\package-sc220-installer.ps1 -Version 0.1.0 -Clean
```

Or run the GitHub Actions workflow:

```text
SC220 Live installer bundle
```

After exact-commit validation, create the public validation receipt and run the public release process. The release script must publish compiled assets only, update the public landing page, and read back the final GitHub Release for verification.
