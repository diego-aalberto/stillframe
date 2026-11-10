@echo off
setlocal
cd /d "%~dp0"
if not exist "Engine\.venv\Scripts\python.exe" (
  echo Run setup-engine.cmd first.
  pause
  exit /b 1
)
"Engine\.venv\Scripts\python.exe" "Engine\run.py" %*
if errorlevel 1 pause
