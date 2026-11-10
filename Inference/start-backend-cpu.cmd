@echo off
setlocal
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo Run the environment setup in README.md first.
  pause
  exit /b 1
)
echo Switching this project's backend to CPU-only inference...
".venv\Scripts\python.exe" -m inference.manage stop
if errorlevel 1 goto failed
".venv\Scripts\python.exe" -m inference.manage start --cpu %*
if errorlevel 1 goto failed
echo CPU backend is running. Open any start-chat launcher to chat.
echo Use end-backend.cmd when you want to stop it.
exit /b 0
:failed
echo CPU backend could not start. See inference\data\backend.log.
pause
exit /b 1
