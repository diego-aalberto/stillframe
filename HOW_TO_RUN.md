# Build and run Stillframe 2

Run these commands from the repository root, where `CMakeLists.txt` and `games/` live.

## Play on Windows

If this checkout already has the player, open the included game from PowerShell:

```powershell
.\build\stillframe.exe games/the-light-we-leave-on
```

Run the executable without a game argument to open the library. A fresh clone contains source and game assets. Build the player first using the steps below.

## Build from source

Install CMake 3.20 or newer and a C++20 compiler. For windowed play, install the SDL3 development package for the same compiler and architecture. If CMake cannot locate it, pass `-DSDL3_DIR=<directory containing SDL3Config.cmake>` when configuring. On Windows, put the matching SDL3 runtime DLL beside the executable or on PATH.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Look for `window support enabled` in the configure output. A CLI-only build supports validation, packaging, and screenshots. With Visual Studio generators the player is typically `build/Release/stillframe.exe`. On Linux with a single-configuration generator, run `./build/stillframe games/the-light-we-leave-on`.

## Check a game or capture a frame

From PowerShell, adjust the executable path for your generator:

```powershell
$player = ".\build\stillframe.exe"
& $player list games
& $player check games/the-light-we-leave-on
& $player screenshot games/the-light-we-leave-on --steps "wait:2;shot:build/opening.png"
& $player pack games/the-light-we-leave-on build/story.stillgame
& $player check build/story.stillgame
```

For live conversations, run a compatible llama.cpp `llama-server` with a loaded model. The game uses `llm.server` in `game.json` (default `http://127.0.0.1:8080`). Scripted scenes work offline. If the server is unavailable during a conversation, use Retry or Skip.

## Run the tests

The integration fixtures use `python3` on PATH and Pillow for PNG generation. If Pillow is installed in another interpreter, set `SF_TEST_PYTHON` to its executable for those PNG tests. After building, run:

```sh
ctest --test-dir build -C Release --output-on-failure
```
