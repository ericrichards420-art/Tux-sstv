@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0packaging\bootstrap-windows.ps1" (
  echo Extract the complete Windows build-kit ZIP first, then run this file from that folder.
  pause
  exit /b 1
)
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0packaging\bootstrap-windows.ps1"
set "tux_result=%errorlevel%"
if not "%tux_result%"=="0" echo Keep the build-windows-logs folder so we can troubleshoot the error.
pause
exit /b %tux_result%
