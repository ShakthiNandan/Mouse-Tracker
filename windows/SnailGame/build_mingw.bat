@echo off
echo Attempting to build Snail Game with MinGW (g++)...
g++ -o SnailGame.exe main.cpp snail_game.cpp -lgdi32 -lgdiplus -mwindows
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ---------------------------------------
    echo BUILD SUCCESSFUL!
    echo Run SnailGame.exe to start.
    echo ---------------------------------------
) else (
    echo.
    echo Build failed. Make sure g++ is in your PATH.
)
pause
