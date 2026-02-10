@echo off
echo Building AutoClicker...
g++ -o AutoClicker.exe main.cpp -mwindows -O2 -s -lcomctl32
if %ERRORLEVEL% EQU 0 ( echo SUCCESS ) ELSE ( echo FAIL )
pause
