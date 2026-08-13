@echo off
setlocal
cd /d "%~dp0"
title SC220 Live - Local QA and Public Draft Handoff

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start-sc220-local-release.ps1"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%EXIT_CODE%"=="0" (
  echo Release berhenti dengan kode %EXIT_CODE%.
  echo Tidak ada release baru yang dianggap berhasil.
) else (
  echo Local QA selesai dan public sc220-download workflow sudah terverifikasi.
)
echo.
pause
exit /b %EXIT_CODE%
