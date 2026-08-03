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
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell

set "VERSION_ARG="
if not "%~1"=="" set "VERSION_ARG=-Version %~1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0scripts\release-sc220-windows.ps1" %VERSION_ARG%
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
echo Baca pesan di atas, perbaiki penyebabnya, lalu jalankan lagi.
pause
exit /b 1
