# Sonkupik Live v0.13.2 — CMake Hash-Scope Fix

## Failure

CMake rejected a clean build because the v0.13.1 patch legitimately changed
`LicenseCheckoutClient.cpp` and `LiveLicensePanel.cpp`, while v0.13.0 SHA-256
values were still treated as immutable embedded-authority hashes.

## Correction

Hard SHA-256 configure gates now cover only vendored immutable authorities:

- ArSonKuPik DSP source/header and preset bank;
- embedded headless processor and licensing public-key implementation;
- approved Sonkupik sphere renderer.

Mutable Sonkupik application integration source is checked by QA rather than
being frozen in CMake. This preserves deterministic engine builds without
blocking normal fixes to checkout, trial and licence-panel code.

## Windows rebuild

```powershell
.\build-live-windows.bat clean
```
