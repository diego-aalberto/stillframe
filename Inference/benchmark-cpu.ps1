param([int]$Context = 4096, [int]$MaxTokens = 128)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Test-Path '.venv\Scripts\python.exe')) { throw 'Run the environment setup in README.md first.' }
Write-Host 'Starting CPU-only backend. The NVIDIA GPU will not be used.'
& '.venv\Scripts\python.exe' -m inference.manage start --cpu --context $Context --batch-size 256 --ubatch-size 64
if ($LASTEXITCODE -ne 0) { throw 'Backend failed to start.' }
try {
  & '.venv\Scripts\python.exe' -m inference.smoke_test --output inference\data\cpu-smoke-results.json
  if ($LASTEXITCODE -ne 0) { throw 'CPU smoke test failed.' }
} finally {
  & '.venv\Scripts\python.exe' -m inference.manage stop
}
