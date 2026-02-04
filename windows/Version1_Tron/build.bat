@echo off
echo Building Tron Version...
g++ -o MouseTracker_Tron.exe main.cpp tron_game.cpp -mwindows -O2 -s -lgdiplus
if %ERRORLEVEL% EQU 0 ( echo SUCCESS ) ELSE ( echo FAIL )
pause
