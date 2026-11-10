@echo off
cd /d "%~dp0"
".venv\Scripts\python.exe" chat.py --system-file inference\prompts\roleplay.txt --session-file inference\data\sessions\summary.session --summary-every 4 %*
if errorlevel 1 pause
