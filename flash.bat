@echo off
chcp 65001 >nul
cd /d "%~dp0"

set "PY=C:\Users\zouyu\.espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe"

if not exist "%PY%" (
    echo [ERROR] ESP-IDF Python env not found: %PY%
    pause
    exit /b 1
)

"%PY%" "%~dp0tools\flash_device.py" %*

echo.
pause
