# SC220 Live public release gate

This document defines the release policy and records the currently published public release. A GitHub tag existing is not, by itself, proof that every listening or runtime qualification item passed; evidence must remain tied to the exact source commit and artifact hash.

## Current published receipt — v0.1.1

Published: **10 August 2026**

- Public repository: `masarray/sc220-download`
- Public tag: `v0.1.1`
- Private source commit recorded by the release: `994b983a7f02b6a2285c41781ecb77dc07e3b217`
- ASK-P authority: v0.5.24 @ `2c764f82b0fc99eaff04d918cef4f839db7509e5`
- Installer: `SC220-Live-v0.1.1-Setup-win-x64.exe`
- Installer size: `24,043,506` bytes
- Installer SHA-256: `589b174357cb36c62611d7a0e89bfa6a7d25251fc27015b5dd5b25fb54c04686`
- VC++ Runtime SHA-256: `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`
- VB-CABLE Pack45 SHA-256: `b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb`
- Public publish/verification workflow: `31346714396` — success

The earlier private self-hosted `local-release.yml` run `31342102770` completed with failure and must **not** be cited as the successful v0.1.1 release-validation run. The published v0.1.1 release was subsequently verified and published by the public distribution workflow.

See `docs/RELEASE_V0.1.1_RECEIPT.md` for the immutable release-facing summary.

## Automated requirements for future releases

Before publication, the release path must ensure:

- [ ] source ref is clean, pushed, and resolved to an exact immutable commit;
- [ ] pinned ASK-P source matches `ASKP_AUTHORITY.lock`;
- [ ] upstream ASK-P production source audit passes;
- [ ] P0/P0.1/P0.2/P0.3 generated DSP is materialised from the approved source;
- [ ] SC220 source and installer contract audits pass;
- [ ] Windows x64 Release executable is produced from the same source commit;
- [ ] installer payload is produced from that executable;
- [ ] installer smoke-install / prerequisite probe / uninstall completes on the qualifying Windows path;
- [ ] installer SHA-256 and manifest are generated from the final artifact;
- [ ] public package boundary rejects application source, DSP source, build trees, debug symbols, credentials, and signing material;
- [ ] published GitHub Release contains the expected artifact set;
- [ ] release metadata and landing pages point to the same version and checksum.

## Mandatory Quick Stream listening test

The normal route is:

```text
Music/player -> normal Windows speaker/headphone -> PC AUDIO SOURCE loopback
SC220 Live protected programme -> CABLE Input
OBS/live application -> CABLE Output
```

For each release candidate record results for:

- [ ] PC Audio reaches the analyzer and programme without SC220 connected;
- [ ] OBS or the target live application receives the processed programme from `CABLE Output`;
- [ ] direct local Windows playback remains free of SC220 Monitor doubling;
- [ ] Monitor is blocked or kept off when it would return processed audio to the captured physical endpoint;
- [ ] `PROGRAM` Monitor produces one processed signal on a genuinely different physical endpoint;
- [ ] `MUSIC CUE` produces enhanced PC Music before ducking on a safe endpoint;
- [ ] changing PC Audio Source reaches active capture without opening Audio Setup;
- [ ] stopping/changing the source returns PC meters/analyzer to silence rather than freezing;
- [ ] mixed sample-rate paths operate without sample-rate errors;
- [ ] unplug/reconnect recovers the selected source/monitor endpoint;
- [ ] extended playback completes without clicks, recursive feedback, growing underflows, or dropouts.

## Installer qualification

For each candidate verify:

- [ ] Windows 10/11 x64 boundary and Administrator requirements;
- [ ] prerequisite probe handles Microsoft VC++ Runtime and Windows Audio service state;
- [ ] standard VB-CABLE task remains visible/optional and identifies VB-Audio ownership/donationware status;
- [ ] Windows driver-security consent remains visible;
- [ ] restart is requested when a runtime/driver install requires it;
- [ ] existing VB-CABLE users can install SC220 Live without changing the driver;
- [ ] uninstalling SC220 Live does not silently remove or modify VB-CABLE;
- [ ] manifest records the exact source commit and package hashes.

## Release presentation

Before a new public tag:

- [ ] screenshot and UI copy match the release candidate;
- [ ] README and public landing pages show the new version, date and installer hash;
- [ ] English and Indonesian download pages agree;
- [ ] `latest.json` points to the exact published installer;
- [ ] release notes match version, source commit, installer hash, runtime hash and VB-CABLE hash;
- [ ] no internal engineering/debug text is presented as end-user product copy.

## Publication rule

Do not call a release complete until the public tag/assets can be read back and the public metadata/landing pages are synchronized.

For the normal local launcher:

```powershell
.\RELEASE-SC220.cmd
```

The launcher should bring the release-only runner online only for the release session, avoid duplicate dispatches, and return the runner offline after completion/failure.
