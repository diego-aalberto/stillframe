@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo Install the Python environment first. See README.md.
  pause
  exit /b 1
)
".venv\Scripts\python.exe" chat.py --system-file inference\prompts\roleplay.txt --session-file inference\data\sessions\roleplay.session %*
if errorlevel 1 pause
