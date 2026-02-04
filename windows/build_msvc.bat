@echo off
echo Attempting to build with MSVC (cl.exe)...
cl.exe /nologo /O1 main.cpp user32.lib gdi32.lib gdiplus.lib /Fe:MouseTracker.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ---------------------------------------
    echo BUILD SUCCESSFUL!
    echo Run MouseTracker.exe to start.
    echo ---------------------------------------
) else (
    echo.
    echo Build failed. Make sure you are running this from the 
    echo "Developer Command Prompt for VS".
)
pause
