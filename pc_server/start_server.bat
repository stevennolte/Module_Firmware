@echo off
REM PC Module Management Server Launcher
REM This script activates the virtual environment and starts the Flask server

echo Starting PC Module Management Server...
echo.

REM Activate the virtual environment (using .bat which bypasses PowerShell restrictions)
call "%~dp0.venv\Scripts\activate.bat"

REM Start the server
python "%~dp0server.py"

REM Keep the window open if there's an error
if errorlevel 1 (
    echo.
    echo Error occurred. Press any key to exit...
    pause > nul
)
