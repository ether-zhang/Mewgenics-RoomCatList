@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\apply_update.ps1"
set "ROOMCAT_UPDATE_EXIT=%errorlevel%"
echo.
pause
exit /b %ROOMCAT_UPDATE_EXIT%
