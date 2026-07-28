@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title Sonkupik Live - Release Build

echo.
echo ============================================================
echo   SONKUPIK LIVE - SINGLE CLICK RELEASE BUILD
echo ============================================================
echo   Source : %CD%
echo   Mode   : Release x64 + portable ZIP
echo.
echo Closing any running Sonkupik Live preview...
taskkill /F /IM "Sonkupik Live.exe" >nul 2>nul

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Windows PowerShell was not found.
    echo Install or repair Windows PowerShell, then run this file again.
    goto :failed
)

if /I "%~1"=="clean" (
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
      -File "%~dp0scripts\package-live-windows.ps1" ^
      -Configuration Release ^
      -Version 0.13.1 ^
      -Clean
) else (
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
      -File "%~dp0scripts\package-live-windows.ps1" ^
      -Configuration Release ^
      -Version 0.13.1
)

set "BUILD_RC=%ERRORLEVEL%"
if not "%BUILD_RC%"=="0" (
    echo.
    echo [ERROR] Build failed with exit code %BUILD_RC%.
    echo Review the error above. The window will remain open.
    goto :failed
)

echo.
echo ============================================================
echo   BUILD SUCCESS
echo ============================================================
echo   Executable and portable ZIP are available in:
echo   %CD%\dist
echo.

if exist "%CD%\dist" start "" explorer.exe "%CD%\dist"
pause
exit /b 0

:failed
echo.
echo Tip: run this from Command Prompt as:
echo   build-live-windows.bat clean
echo to force a completely clean rebuild.
echo.
pause
exit /b 1
