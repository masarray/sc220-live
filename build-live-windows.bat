@echo off
rem Compatibility entrypoint. The product build authority is build-sc220-windows.bat.
call "%~dp0build-sc220-windows.bat" %*
exit /b %ERRORLEVEL%
