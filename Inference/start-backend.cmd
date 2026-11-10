@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo Run the environment setup in README.md first.
  pause
  exit /b 1
)
".venv\Scripts\python.exe" -m inference.manage start %*
if errorlevel 1 pause
