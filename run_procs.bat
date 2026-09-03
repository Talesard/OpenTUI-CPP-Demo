@echo off
setlocal
cd /d "%~dp0"
if not exist "build\opentui_procs.exe" (
  echo Build first:
  echo   cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
  echo   cmake --build build
  pause
  exit /b 1
)
"build\opentui_procs.exe"
set ERR=%ERRORLEVEL%
if not "%ERR%"=="0" (
  echo.
  echo Exited with code %ERR%
  pause
)
endlocal
