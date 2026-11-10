$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force test-output | Out-Null
    foreach ($application in @('ComicPlayer', 'ComicStudio')) {
        $exePath = Join-Path $PSScriptRoot "bin/$application.exe"
        if (-not (Test-Path $exePath)) { throw 'Run build.ps1 first.' }
        $renderPath = Join-Path $PSScriptRoot "test-output/$application.ppm"
        $samplePath = Join-Path $PSScriptRoot 'samples/midnight.cscene'
        $process = Start-Process -FilePath $exePath -ArgumentList '--smoke',('"' + $renderPath + '"'),('"' + $samplePath + '"') -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(30000)) { $process.Kill(); throw "$application smoke test timed out." }
        if ($process.ExitCode -ne 0) { throw "$application smoke test failed. Check test-output for diagnostics." }
        if (-not (Test-Path $renderPath)) { throw "$application did not produce a render." }
        Write-Host "${application}: render and application smoke test passed."
    }
} finally { Pop-Location }
