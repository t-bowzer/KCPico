@echo off
setlocal
cd /d "%~dp0"

where python >nul 2>nul
if errorlevel 1 (
    echo Python was not found on PATH. Install Python 3.9 or newer and try again.
    pause
    exit /b 1
)

python -c "import PySide6" >nul 2>nul
if errorlevel 1 (
    echo Installing PySide6...
    python -m pip install PySide6
    if errorlevel 1 (
        echo Failed to install PySide6. Try: python -m pip install PySide6
        pause
        exit /b 1
    )
)

python run.py
if errorlevel 1 pause
endlocal
