@echo off
setlocal
cd /d "%~dp0"
title SC220 Live - Setup Local GitHub Runner

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\setup-sc220-local-runner.ps1"
set "EXIT_CODE=%ERRORLEVEL%"

if "%EXIT_CODE%"=="0" (
  echo.
  echo Verifying tools visible to the Windows runner service...
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\verify-sc220-runner-host.ps1"
  set "EXIT_CODE=%ERRORLEVEL%"
)

echo.
if not "%EXIT_CODE%"=="0" (
  echo Setup berhenti dengan kode %EXIT_CODE%.
  echo Baca pesan di atas sebelum mencoba lagi.
) else (
  echo Local runner SC220 sudah siap.
)
echo.
pause
exit /b %EXIT_CODE%
