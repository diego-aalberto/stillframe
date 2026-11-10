@echo off
setlocal
cd /d "%~dp0"
if not exist "Engine\.venv\Scripts\python.exe" (
  echo Run setup-engine.cmd first.
  pause
  exit /b 1
)
pushd Inference
".venv\Scripts\python.exe" -m inference.manage start
if errorlevel 1 (
  popd
  pause
  exit /b 1
)
popd
"Engine\.venv\Scripts\python.exe" "Game\panel_player.py" %*
if errorlevel 1 pause
