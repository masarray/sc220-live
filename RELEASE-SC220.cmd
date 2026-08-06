@echo off
setlocal
cd /d "%~dp0"
title SC220 Live - Local Build and Public Release

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-sc220-local-release.ps1"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%EXIT_CODE%"=="0" (
  echo Release berhenti dengan kode %EXIT_CODE%.
  echo Tidak ada release baru yang dianggap berhasil.
) else (
  echo Proses release selesai.
)
echo.
pause
exit /b %EXIT_CODE%
