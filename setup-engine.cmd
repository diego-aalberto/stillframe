@echo off
setlocal
cd /d "%~dp0"
if exist "Inference\.venv\Scripts\python.exe" (
  set "BASE=Inference\.venv\Scripts\python.exe"
) else (
  set "BASE=python"
)
if not exist "Engine\.venv\Scripts\python.exe" "%BASE%" -m venv Engine\.venv
if not exist "Engine\.venv\Scripts\python.exe" exit /b 1
"%BASE%" -m pip --python "Engine\.venv\Scripts\python.exe" install -r Engine\requirements.txt
if errorlevel 1 pause
