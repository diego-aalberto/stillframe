@echo off
cd /d "%~dp0"
".venv\Scripts\python.exe" chat.py --scratch --session-file inference\data\sessions\fresh.session %*
if errorlevel 1 pause
