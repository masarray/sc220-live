@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title SC220 Live - One Click Build

echo.
echo ============================================================
echo   SC220 LIVE - ONE CLICK WINDOWS BUILD
echo ============================================================
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell

set "CLEAN_SWITCH="
if /I "%~1"=="clean" set "CLEAN_SWITCH=-Clean"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0scripts\build-sc220-one-click.ps1" %CLEAN_SWITCH%
set "BUILD_RC=%ERRORLEVEL%"
if not "%BUILD_RC%"=="0" goto :failed

echo.
echo Build berhasil. Tekan tombol apa saja untuk menutup jendela.
pause >nul
exit /b 0

:no_powershell
echo [ERROR] Windows PowerShell tidak ditemukan.
goto :failed

:failed
echo.
echo [ERROR] Build gagal. Baca pesan di atas untuk penyebab tepatnya.
echo Jalankan "build-sc220-windows.bat clean" untuk clean build.
pause
exit /b 1
