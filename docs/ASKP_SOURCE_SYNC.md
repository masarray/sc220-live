# Authoritative Smart Enhancer source

Sonkupik Live v0.8.0 intentionally refuses to build with its old vendored Smart
Enhancer snapshot. The approved engine and all 40 presets come directly from the
private ArSonKuPik VST repository.

## Preferred repository layout

```text
D:\Git\askp-vst
D:\Git\sonkupik-live
```

Then run:

```powershell
.\scripts\build-live-windows.ps1 -Clean
```

The build script detects the sibling repository automatically. When no authority
checkout is found, it also runs the locked sync automatically and creates
`.deps\askp-vst`; Git Credential Manager may request authentication.

## Explicit path

```powershell
.\scripts\build-live-windows.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -Clean
```

## Locked checkout under this project

```powershell
.\scripts\sync-askp-authority.ps1
.\scripts\build-live-windows.ps1 -Clean
```

The sync command checks out the exact commit recorded in `ASKP_AUTHORITY.lock`
under `.deps\askp-vst`. Git Credential Manager may request authentication because
the repository is private.

A newer `askp-vst` checkout may compile, but CMake prints a warning because its
sound and performance have not yet been frozen by the v0.8.0 QA lock.


## Font authority

Plus Jakarta Sans binaries are not duplicated in the Sonkupik Live delivery. CMake
embeds the Regular and SemiBold files directly from the authenticated `askp-vst`
checkout, alongside the exact DSP, presets and sphere gloss authority.
