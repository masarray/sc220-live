# Build Sonkupik Live on Windows

## Supported toolchain

- Visual Studio Community 2026 with **Desktop development with C++**.
- MSVC x64 toolset and Windows SDK.
- CMake bundled with Visual Studio or a newer CMake on PATH.
- Git for Windows with access to the private `masarray/askp-vst` repository.

## Single-click build

Double-click the root file:

```text
build-live-windows.bat
```

The BAT runs the Release build, creates `dist\Sonkupik-Live-v0.10.1-win-x64-portable.zip`, and opens the `dist` folder after success. The console remains open on failure so the complete compiler/linker message is visible.

For a forced clean build:

```bat
build-live-windows.bat clean
```

## PowerShell build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-live-windows.ps1 -Clean
```

The script detects Visual Studio, selects `Visual Studio 18 2026` when supported, or falls back to Ninja using the same MSVC compiler.

It then searches for the ArSonKuPik authority in this order:

1. `-AskpSourceDir`;
2. `ASKP_SOURCE_DIR` environment variable;
3. sibling `askp-vstvisualizer` or `askp-vst` repository;
4. `.deps\askp-vst`;
5. `third_party\askp-vst`.

When none exists, it automatically runs `scripts\sync-askp-authority.ps1` and checks out the exact commit recorded in `ASKP_AUTHORITY.lock`. Git Credential Manager may request authentication.

## Explicit local sources

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -AskpSourceDir D:\Git\askp-vst `
  -JuceSourceDir D:\SDK\JUCE-8.0.14 `
  -Clean
```

## Output

The executable is normally under:

```text
build-live\SonkupikLive_artefacts\Release\Sonkupik Live.exe
```

## Clean after generator or source changes

Always use `-Clean` after changing Visual Studio generation, JUCE source, or the authority checkout. The script also removes a cached build tree automatically when its CMake generator differs from the selected one.

## Common failures

### Private authority cannot be cloned

Authenticate Git Credential Manager or GitHub CLI, then run:

```powershell
.\scripts\sync-askp-authority.ps1 -Force
```

### CMake cannot find Visual Studio 2026

Install/repair:

- Desktop development with C++;
- MSVC v145 x64/x86 build tools;
- Windows SDK;
- C++ CMake tools for Windows.

The automatic Ninja fallback still uses MSVC and is a valid build path.

## Linker output locked (LNK1104)

The build script closes any running `Sonkupik Live.exe`, removes stale output with bounded retry, and retries the linker once. This prevents the common case where a preview instance, Windows Security, or Explorer temporarily locks the Release executable.

If Windows still holds the file, close Sonkupik Live and run:

```bat
build-live-windows.bat clean
```

Avoid browsing the build artefact folder with Explorer Preview enabled during linking.
