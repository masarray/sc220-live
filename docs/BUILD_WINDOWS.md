# Build Sonkupik Live v0.13.1 on Windows

Requirements:

- Visual Studio Community 2026
- Desktop development with C++
- MSVC x64 toolchain
- Windows SDK
- CMake tools for Windows

## Single click

Double-click `build-live-windows.bat`.

Clean build:

```bat
build-live-windows.bat clean
```

The BAT builds Release x64, creates
`dist\Sonkupik-Live-v0.13.1-win-x64-portable.zip`, and opens the `dist` folder.

## PowerShell

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -Clean
```

The ArSonKuPik v0.5.19 engine is embedded. No separate ASK-P checkout is needed.
Use `-JuceSourceDir` for an offline/local JUCE 8.0.14 tree.
