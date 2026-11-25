@echo off
setlocal
cd /d "%~dp0"
if not defined TUX_MSYS2_ROOT if exist "%LOCALAPPDATA%\TuxSSTVBuild\msys64\msys2_shell.cmd" set "TUX_MSYS2_ROOT=%LOCALAPPDATA%\TuxSSTVBuild\msys64"
if not defined TUX_MSYS2_ROOT if exist "%SystemDrive%\TuxSSTVBuild\msys64\msys2_shell.cmd" set "TUX_MSYS2_ROOT=%SystemDrive%\TuxSSTVBuild\msys64"
if not defined TUX_MSYS2_ROOT set "TUX_MSYS2_ROOT=C:\msys64"
if not exist "%TUX_MSYS2_ROOT%\msys2_shell.cmd" (
  echo Install MSYS2 from https://www.msys2.org/ first.
  echo Then read WINDOWS-START-HERE.txt and run Install Windows Build Tools.cmd.
  echo For a custom install, set TUX_MSYS2_ROOT to its directory.
  pause
  exit /b 1
)
call "%TUX_MSYS2_ROOT%\msys2_shell.cmd" -defterm -no-start -ucrt64 -here -c "bash packaging/build-windows.sh"
set "tux_result=%errorlevel%"
if not "%tux_result%"=="0" echo Build failed. Keep the build logs for troubleshooting.
pause
exit /b %tux_result%
