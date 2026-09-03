@echo off
setlocal
cd /d "%~dp0"
if not exist "build\opentui_demo.exe" (
  echo Build the demo first:
  echo   cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
  echo   cmake --build build
  pause
  exit /b 1
)
echo Starting OpenTUI demo...
echo Press Q or Esc to quit.
echo.
"build\opentui_demo.exe"
set ERR=%ERRORLEVEL%
if not "%ERR%"=="0" (
  echo.
  echo Demo exited with code %ERR%
  pause
)
endlocal
