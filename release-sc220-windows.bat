@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title SC220 Live - One Click Release

echo.
echo ============================================================
echo   SC220 LIVE - ONE CLICK VERIFIED RELEASE
echo ============================================================
echo   Private source : masarray/sc220-live
echo   Public download: masarray/sc220-download
echo   Requires       : exact-commit validation receipt
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell

set "VERSION_ARG="
if not "%~1"=="" set "VERSION_ARG=-Version %~1"
set "CLEAN_ARG="
if /I "%~2"=="clean" set "CLEAN_ARG=-Clean"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0scripts\release-sc220-windows.ps1" %VERSION_ARG% %CLEAN_ARG%
set "RELEASE_RC=%ERRORLEVEL%"
if not "%RELEASE_RC%"=="0" goto :failed

echo.
echo Release selesai dan sudah diverifikasi.
pause
exit /b 0

:no_powershell
echo [ERROR] Windows PowerShell tidak ditemukan.
goto :failed

:failed
echo.
echo [ERROR] Release dihentikan aman. Tidak ada sukses palsu.
echo Selesaikan docs\PUBLIC_RELEASE_GATE.md dan buat validation receipt terlebih dahulu.
echo Baca pesan di atas, perbaiki penyebabnya, lalu jalankan lagi.
pause
exit /b 1
