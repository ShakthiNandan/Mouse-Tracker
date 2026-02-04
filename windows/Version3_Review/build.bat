@echo off
echo Building Review Only Version...
g++ -o MouseTracker_Review.exe main.cpp -mwindows -O2 -s -lgdiplus
if %ERRORLEVEL% EQU 0 ( echo SUCCESS ) ELSE ( echo FAIL )
pause
