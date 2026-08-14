# SC220 Live v0.1.2 - Public Release Receipt

Published: **2026-08-14**

## Product

- Product: SC220 Live
- Version: `0.1.2`
- Channel: `stable`
- Platform: `windows-x64`
- Public distribution repository: `masarray/sc220-download`
- Public tag: `v0.1.2`

## Private source provenance

- SC220 Live source commit: `91c33377573b891b99c64aada9b63c4e10517fb3`
- ASK-P engine: `0.5.24`
- ASK-P approved product source: `2c764f82b0fc99eaff04d918cef4f839db7509e5`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- Gain Match in SC220 UI: not exposed
- Gain Match default: off

## Public artifact

Installer: `SC220-Live-v0.1.2-Setup-win-x64.exe`

- Size: `24,279,209` bytes
- SHA-256: `ba6c61617d8c796dd953234e50f6d2e179c5efbe5e281892da7e7ba977e55693`
- VC++ Runtime SHA-256: `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`
- VB-CABLE Pack45 SHA-256: `b950e39f01af1d04ea623c8f6d8eb9b6ea5c477c637295fabf20631c85116bfb`

Additional published integrity material includes the `.sha256` file, manifest, build-commit receipt, VC++ Runtime SHA-256 receipt, and VB-CABLE Pack45 SHA-256 receipt.

## Validation evidence

- Full local QA: 29 Python gates, fixed FFT verification, and 3/3 compiled CTest targets passed.
- Local extracted installer scan: 39 files, 0 forbidden source/debug/key files, and 0 private ASK-P markers.
- Public workflow: `Publish verified SC220 draft release`, run `31773444418`, conclusion `success`.
- Public installer inspection and publication jobs both completed successfully.
- Public `latest.json` was read back with version `0.1.2`, the source commit above, and the same installer SHA-256.

This receipt records release provenance and public asset verification. It does not replace separate physical-device, listening, latency, or long-duration playback qualification records.
