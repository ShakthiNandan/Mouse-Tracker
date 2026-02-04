@echo off
echo Attempting to build with MinGW (g++)...
g++ -o MouseTracker.exe main.cpp tron_game.cpp -mwindows -O2 -s -lgdiplus
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ---------------------------------------
    echo BUILD SUCCESSFUL!
    echo Run MouseTracker.exe to start.
    echo ---------------------------------------
) else (
    echo.
    echo Build failed. Make sure MinGW/g++ is installed and in your PATH.
)
pause
