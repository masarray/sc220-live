# SC220 Live Smart Installer

## Purpose

The Windows installer must prevent first-launch failures caused by missing Microsoft runtime files, stopped Windows Audio services, or incomplete VB-CABLE installation.

The installer now treats these as setup states instead of allowing Windows to show an unexplained missing-DLL dialog.

## System check

Before installation, `SC220PrerequisiteProbe.ps1` checks:

- Microsoft Visual C++ v14 x64 runtime registry state.
- `AudioEndpointBuilder` and `Audiosrv` service state.
- Present Windows PnP devices for the VB-Audio driver.
- The `CABLE Input` playback endpoint.
- The `CABLE Output` recording endpoint.

The result controls the default installer action:

| Detected state | Installer behavior |
| --- | --- |
| VC runtime present | Skip Microsoft runtime setup |
| VC runtime missing | Install the verified x64 redistributable |
| Windows Audio service stopped | Attempt to start the service, then verify again |
| VB-CABLE healthy | Leave the existing driver untouched and deselect its task |
| VB-CABLE partial | Select repair/install and explain the incomplete state |
| VB-CABLE absent | Select installation of the reviewed standard package |

## Launch gate

SC220 Live is launched automatically only when:

1. The Microsoft runtime is confirmed after installation.
2. Windows Audio and Audio Endpoint Builder are running.
3. No runtime or driver operation requires a Windows restart.

A failed core verification leaves the application installed but disables automatic launch. The user receives an actionable message instead of a generic `MSVCP140.dll` error.

## Supply-chain controls

### Microsoft Visual C++ Runtime

`VC_REDIST_AUTHORITY.lock` pins:

- The official Microsoft permalink for review context.
- An immutable `download.visualstudio.microsoft.com` package URL.
- The reviewed SHA-256.
- A valid Authenticode requirement.
- The expected `Microsoft Corporation` signer identity.

The packaging script verifies the hash before checking the signature and before compiling the installer.

### VB-Audio VB-CABLE

The existing VB-CABLE controls remain in force:

- Standard Pack45 only.
- Reviewed SHA-256.
- Valid signed setup executable and catalogue files.
- Required INF, SYS, and CAT support files kept beside the setup executable.
- A/B/C/D cable variants rejected from the standard bundle.
- Donationware ownership and product separation retained.

## Diagnostics

The installer preserves the probe at:

```text
SC220 Live\Tools\SC220PrerequisiteProbe.ps1
```

Manual summary:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File ".\Tools\SC220PrerequisiteProbe.ps1" -Mode Summary
```

Core-only exit codes:

- `0`: runtime and audio services ready.
- `11`: Microsoft Visual C++ runtime missing.
- `12`: Windows Audio service not running.

VB-CABLE exit codes:

- `0`: driver and both endpoints ready.
- `21`: partial installation detected.
- `22`: VB-CABLE not detected.

## Release validation

`qa/audit_sc220_installer_bundle.py` locks the installer contract so future changes cannot silently remove runtime installation, prerequisite detection, launch gating, or supply-chain verification.
