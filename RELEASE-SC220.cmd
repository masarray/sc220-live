@echo off
setlocal
cd /d "%~dp0"
title SC220 Live - Local Build and Public Release

fltmc >nul 2>&1
if not "%ERRORLEVEL%"=="0" (
  echo Requesting Administrator access for the release-only runner session...
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
  exit /b 0
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-sc220-local-release.ps1"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%EXIT_CODE%"=="0" (
  echo Release berhenti dengan kode %EXIT_CODE%.
  echo Tidak ada release baru yang dianggap berhasil.
) else (
  echo Proses release selesai dan runner sudah dikembalikan offline.
)
echo.
pause
exit /b %EXIT_CODE%
