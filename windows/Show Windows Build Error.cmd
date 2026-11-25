@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
echo Reading the newest Tux-sstv Windows build logs...
echo This does not rebuild, install anything, or run the radio program.
if not exist "%~dp0packaging\show-latest-build.ps1" (
    echo Extract the entire diagnostic ZIP before running this helper.
    pause
    exit /b 1
)
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0packaging\show-latest-build.ps1"
echo.
pause
endlocal
