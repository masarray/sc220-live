@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
title SC220 Live - Setup Local GitHub Runner

fltmc >nul 2>&1
if not "!ERRORLEVEL!"=="0" (
  echo Requesting Administrator access for the complete runner setup...
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
  exit /b 0
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup-sc220-local-runner.ps1"
set "EXIT_CODE=!ERRORLEVEL!"

if "!EXIT_CODE!"=="0" (
  echo.
  echo Applying on-demand runner service security mode...
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\configure-sc220-runner-service.ps1"
  set "EXIT_CODE=!ERRORLEVEL!"
)

if "!EXIT_CODE!"=="0" (
  echo.
  echo Verifying tools visible to the Windows runner service...
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\verify-sc220-runner-host.ps1"
  set "EXIT_CODE=!ERRORLEVEL!"
)

echo.
if not "!EXIT_CODE!"=="0" (
  echo SETUP BELUM SELESAI - kode !EXIT_CODE!.
  echo Perbaiki pesan merah di atas lalu jalankan SETUP-LOCAL-RUNNER.cmd lagi.
) else (
  echo LOCAL RUNNER SC220 READY.
  echo Runner sudah diverifikasi dan dikembalikan offline sampai release dimulai.
)
echo.
pause
exit /b !EXIT_CODE!
