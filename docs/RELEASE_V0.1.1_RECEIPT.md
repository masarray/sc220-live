# SC220 Live v0.1.1 — Public Release Receipt

Published: **2026-08-10**

## Product

- Product: SC220 Live
- Version: `0.1.1`
- Channel: `stable`
- Platform: `windows-x64`
- Public distribution repository: `masarray/sc220-download`
- Public tag: `v0.1.1`

## Private source provenance

- SC220 Live source commit: `994b983a7f02b6a2285c41781ecb77dc07e3b217`
- ASK-P engine: `0.5.24`
- ASK-P approved product source: `2c764f82b0fc99eaff04d918cef4f839db7509e5`
- ASK-P latest audited release-line HEAD: `44b7d1657fbbd2c13cd490e251e8867734238623`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- Gain Match in SC220 UI: not exposed
- Gain Match default: off

## Public artifact

Installer:

```text
SC220-Live-v0.1.1-Setup-win-x64.exe
```

Size:

```text
24,043,506 bytes
```

SHA-256:

```text
589b174357cb36c62611d7a0e89bfa6a7d25251fc27015b5dd5b25fb54c04686
```

Additional published integrity material includes the `.sha256` file, manifest, build-commit receipt, VC++ Runtime SHA-256 receipt, and VB-CABLE Pack45 SHA-256 receipt.

## Prerequisite identities

- Microsoft VC++ Runtime SHA-256: `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`
- VB-CABLE Pack45 SHA-256: `b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb`

## Public publication verification

`masarray/sc220-download` workflow:

```text
Publish verified SC220 draft release
run 31346714396
conclusion: success
```

The public GitHub Release was read back as stable `v0.1.1` with the expected installer asset and SHA-256.

## Important validation distinction

This receipt records **release provenance and public asset verification**. It does not replace separate listening, realtime audio-device, latency, long-duration playback, or hardware-specific qualification records.

The earlier private self-hosted `local-release.yml` run `31342102770` ended in failure and is not the successful publication authority for v0.1.1.
