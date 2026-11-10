@echo off
cd /d "%~dp0"
if not exist ".venv\Scripts\python.exe" (
  echo Run the environment setup in README.md first.
  pause
  exit /b 1
)
echo Starting CPU-only inference benchmark backend. The NVIDIA GPU will not be used.
".venv\Scripts\python.exe" -m inference.manage start --cpu --context 4096 --parallel 1 --batch-size 256 --ubatch-size 64
if errorlevel 1 goto failed
".venv\Scripts\python.exe" -m inference.smoke_test --output inference\data\cpu-smoke-results.json
set TEST_RESULT=%ERRORLEVEL%
".venv\Scripts\python.exe" -m inference.manage stop
if not "%TEST_RESULT%"=="0" goto failed
echo CPU benchmark complete. Results: inference\data\cpu-smoke-results.json
exit /b 0
:failed
echo CPU benchmark failed. Check inference\data\backend.log and engine.log.
pause
exit /b 1
