$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $exe = Join-Path $PSScriptRoot 'bin/ComicPlayer.exe'
    if (-not (Test-Path $exe)) { throw 'Run build.ps1 first.' }
    New-Item -ItemType Directory -Force test-output/anime | Out-Null
    $render = Join-Path $PSScriptRoot 'test-output/anime/sequence.ppm'
    $process = Start-Process -FilePath $exe -ArgumentList '--anime-test','--smoke',('"' + $render + '"') -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Anime render test timed out.' }
    if ($process.ExitCode -ne 0) { throw 'Anime render test failed; see test-output/anime diagnostics.' }
    $rows = Import-Csv ($render + '.timeline.csv')
    if ($rows.Count -ne 9) { throw 'Expected nine timeline segments.' }
    for ($i = 0; $i -lt 9; $i++) {
        if ([int]$rows[$i].start_seconds -ne ($i * 3) -or [int]$rows[$i].end_seconds -ne (($i + 1) * 3) -or [int]$rows[$i].visible_layers -ne 1) { throw 'Incorrect demo schedule.' }
    }
    Write-Host 'PASS: all nine animations rendered; 3 seconds per animation, 9 seconds per pose; loop boundary at 27 seconds.'
} finally { Pop-Location }
