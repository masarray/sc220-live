@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title SC220 Live - Installer Bundle

echo.
echo ============================================================
echo   SC220 LIVE - VERIFIED INSTALLER BUNDLE
echo ============================================================
echo   Windows 10/11 x64

echo   Optional bundled component: VB-Audio VB-CABLE donationware

echo.

where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell

set "VERSION=%~1"
if "%VERSION%"=="" set "VERSION=0.1.0"
set "CLEAN_SWITCH="
if /I "%~2"=="clean" set "CLEAN_SWITCH=-Clean"

if "%VBCABLE_PACK45_SHA256%"=="" (
  echo [ERROR] Environment variable VBCABLE_PACK45_SHA256 is not set.
  echo Review the official VBCABLE_Driver_Pack45.zip, then set its SHA-256.
  echo Example:
  echo   set VBCABLE_PACK45_SHA256=64_HEX_CHARACTERS
  goto :failed
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0scripts\package-sc220-installer.ps1" ^
  -Version "%VERSION%" ^
  -VBCableSha256 "%VBCABLE_PACK45_SHA256%" ^
  %CLEAN_SWITCH%
set "INSTALLER_RC=%ERRORLEVEL%"
if not "%INSTALLER_RC%"=="0" goto :failed

echo.
echo Installer bundle selesai di folder dist.
pause
exit /b 0

:no_powershell
echo [ERROR] Windows PowerShell tidak ditemukan.

:failed
echo.
echo [ERROR] Installer bundle dihentikan aman.
echo Tidak ada driver tanpa hash atau signature yang valid akan dibundel.
pause
exit /b 1
