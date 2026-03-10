@echo off
REM PC Module Management Server Launcher
REM This script activates the virtual environment and starts the Flask server

echo Starting PC Module Management Server...
echo.
REM Set environment variables
set GDRIVE_CREDENTIALS_FILE=%~dp0credentials.json
set GDRIVE_SPREADSHEET_ID=1CnxWag_hlq1Kgf459uiMorvHfMR-KPu-fC-3zhrMj0Y
set DATA_LOG_INTERVAL=15

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
