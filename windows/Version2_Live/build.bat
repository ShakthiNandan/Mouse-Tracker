@echo off
echo Building Live Only Version...
g++ -o MouseTracker_Live.exe main.cpp -mwindows -O2 -s -lgdiplus
if %ERRORLEVEL% EQU 0 ( echo SUCCESS ) ELSE ( echo FAIL )
pause
