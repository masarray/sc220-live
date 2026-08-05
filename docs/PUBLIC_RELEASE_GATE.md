# SC220 Live public release gate

The first public binary must not be published merely because source audits pass. A release is approved only when every mandatory item below is complete for the exact commit being packaged.

## Automated requirements

- [x] Source and generated-code audits pass.
- [x] Windows MSVC WASAPI monitor/capture smoke target compiles and runs.
- [x] Proprietary/binary-only package boundary checks exist.
- [x] Production licence-service and product-contract checks exist in the release script.
- [x] Public repository is separate from the private source authority.
- [ ] Full `SC220 Live.exe` clean Release build succeeds with the pinned private ASK-P v0.5.23 authority.
- [ ] Portable ZIP and `.sha256` are produced from the same commit.
- [ ] Packaged ZIP contains only the executable and approved end-user documents.

## Mandatory Windows listening test

Use distinct endpoint families:

```text
Music/player -> VB-CABLE family A -> PC AUDIO SOURCE
SC220 Live   -> physical headphone -> MONITOR OUTPUT
SC220 Live   -> VB-CABLE family B -> STREAM OUTPUT / OBS
```

- [ ] PC Audio reaches the analyzer and programme without SC220 connected.
- [ ] `PROGRAM` Monitor produces one processed signal with no direct-plus-DSP doubling.
- [ ] `MUSIC CUE` produces enhanced PC Music before ducking.
- [ ] Changing PC Audio Source reaches `PLAYBACK CAPTURE ACTIVE` without opening Audio Setup.
- [ ] When the source stops or changes, PC meters and analyzer return to silence instead of freezing.
- [ ] 44.1 kHz source to 48 kHz monitor, or the inverse, plays without a sample-rate error.
- [ ] Unplug/reconnect of the selected source or monitor recovers automatically.
- [ ] Ten-minute playback completes without clicks, recursive feedback, growing underflows, or dropouts.

## Release presentation

- [ ] Latest refined UI is visually checked at `1180x720`, `1600x900`, and maximised size.
- [ ] No engineering copy remains on the main surface (`PINK-FLAT`, generation counters, FPS, social target legend, or endpoint debug text).
- [ ] Final current screenshot replaces `docs/SC220_LIVE_UX_LOCKED.png` before publishing.
- [x] README describes selectable PC Audio, OBS-style Monitor Bus, and anti-echo routing.
- [x] Portable `QUICK_START.txt` describes distinct source/stream cable families.
- [x] Public landing page describes the current Stream and Monitor workflow.
- [ ] Release notes match the exact version and commit.

## Publication rule

Do not merge the release-candidate PR and do not run `release-sc220-windows.bat` until all mandatory unchecked items above have been validated against the exact release commit.

After validation:

```powershell
release-sc220-windows.bat 0.1.0 clean
```

The release script then builds, packages, hashes, updates the public download repository, creates the binary-only GitHub Release, enables Pages when necessary, and reads the published assets back for verification.
