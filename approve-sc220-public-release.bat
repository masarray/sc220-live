@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title SC220 Live - Public Release Validation

echo.
echo ============================================================
echo   SC220 LIVE - PUBLIC RELEASE VALIDATION
echo ============================================================
echo   Complete every mandatory item in:
echo   docs\PUBLIC_RELEASE_GATE.md
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 goto :no_powershell

set "VERSION_ARG="
if not "%~1"=="" set "VERSION_ARG=-Version %~1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0scripts\approve-sc220-public-release.ps1" %VERSION_ARG%
set "APPROVE_RC=%ERRORLEVEL%"
if not "%APPROVE_RC%"=="0" goto :failed

echo.
echo Validation receipt berhasil dibuat untuk commit ini.
pause
exit /b 0

:no_powershell
echo [ERROR] Windows PowerShell tidak ditemukan.
goto :failed

:failed
echo.
echo [ERROR] Validation receipt tidak dibuat.
pause
exit /b 1
