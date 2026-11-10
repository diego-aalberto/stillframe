param([switch]$Debug)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $compiler = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $compiler -and (Test-Path 'C:\msys64\mingw64\bin\g++.exe')) { $compiler = Get-Item 'C:\msys64\mingw64\bin\g++.exe' }
    if (-not $compiler) { throw 'Install a MinGW-w64 C++ compiler, or use CMake with Visual Studio. See README.md.' }
    $compilerPath = if ($compiler.Source) { $compiler.Source } else { $compiler.FullName }
    New-Item -ItemType Directory -Force bin,samples | Out-Null
    $flags = @('-std=c++17','-Wall','-Wextra','-Wpedantic','-Wno-misleading-indentation','-DUNICODE','-D_UNICODE','-static','-static-libgcc','-static-libstdc++')
    if ($Debug) { $flags += @('-O0','-g') } else { $flags += @('-O2','-s') }
    $sources = @('src/core.cpp','src/demo.cpp','src/anime_demo.cpp','src/platform.cpp','src/render.cpp','src/app.cpp')
    $libs = @('-lopengl32','-lglu32','-lgdi32','-lcomdlg32','-lcomctl32','-lole32','-lwindowscodecs','-luuid','-lshell32')
    & $compilerPath @flags @sources '-municode' '-mwindows' @libs '-o' 'bin/ComicPlayer.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Player build failed' }
    & $compilerPath @flags '-DCOMIC_EDITOR' @sources '-municode' '-mwindows' @libs '-o' 'bin/ComicStudio.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Editor build failed' }
    & $compilerPath @flags 'src/core.cpp' 'src/demo.cpp' 'src/platform.cpp' 'src/render.cpp' 'src/story_art.cpp' 'src/story_game.cpp' '-municode' '-mwindows' @libs '-lwinhttp' '-lgdiplus' '-lwinmm' '-o' 'bin/StoryGame.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Story game build failed' }
    & $compilerPath @flags 'src/core.cpp' 'src/demo.cpp' 'src/anime_demo.cpp' 'tests/core_tests.cpp' '-o' 'bin/ComicTests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
    & ./bin/ComicTests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Engine tests failed' }
    $sample = Join-Path $PSScriptRoot 'samples/midnight.cscene'
    if (-not (Test-Path $sample)) {
        $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'bin/ComicPlayer.exe') -ArgumentList '--make-demo',('"' + $sample + '"') -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw 'Sample creation failed' }
    }
    Write-Host 'Built ComicPlayer.exe, ComicStudio.exe, and StoryGame.exe. Engine tests passed.'
} finally { Pop-Location }
