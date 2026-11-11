# Stillframe 2

A visual-novel engine where characters can actually talk. One engine install, many games: a game is a folder (or a single
`.stillgame` file) of scripts, characters and art, and the engine runs whatever is in its `games/` folder.

* **Ren'Py-style scripts** (`.sf`) with live LLM conversations as a first-class statement: `talk with hana, mira:`
* **Three memory slots per character**: immutable, contextual (changed only by the character herself, grudges included), per-scene
* **Characters can interject**, chosen by the model, rate-limited by the engine
* **Minigames are separate programs** the story jumps out to and gets an outcome back from
* **C++20, no dependencies** besides an optional SDL3 for the window. Software renderer, own PNG/TTF/ZIP/JSON/HTTP.

Docs: [ARCHITECTURE](docs/ARCHITECTURE.md) (design, the GPU diagnosis, memory, interjections) -
[SCRIPT](docs/SCRIPT.md) - [GAME_FORMAT](docs/GAME_FORMAT.md) - [MINIGAMES](docs/MINIGAMES.md)

## Build

```sh
cmake -S . -B build            # SDL3 is used automatically if found; otherwise you get the CLI tools only
cmake --build build
./build/sf_tests               # 100+ tests; `./build/sf_tests memory_` runs a subset
```

## Use

```sh
stillframe                                   # library screen (needs the SDL build)
stillframe games/the-light-we-leave-on       # open one game directly
stillframe check games/the-light-we-leave-on # validate scripts, characters, assets
stillframe pack  games/the-light-we-leave-on # -> the-light-we-leave-on.stillgame
stillframe list games
stillframe screenshot <game> --steps "wait:2;click;wait:1;shot:out.png" [--fake-llm replies.txt]
```

`screenshot` renders headlessly (no window, no GPU), which is how the UI was developed and is what you want in CI.
Conversations need a llama.cpp `llama-server` (default `http://127.0.0.1:8080`, change it in the game's `llm.server`).
Without one the game shows a "model isn't reachable" panel with Retry/Skip instead of failing silently.

## Performance

Measured in a 2-core container (no GPU): a four-character scene with the text box at 1260x840 renders in about 11 ms per frame
(`stillframe screenshot <game> --steps "wait:6;bench:60"`), and idle screens are not redrawn at all. The big wins were
pre-scaling sprites once (cached) instead of resampling each frame, caching soft shadows as masks, and span-filling panels.
Treat this as an order of magnitude, not a guarantee on your hardware.

## Status - read this before relying on it

Verified by automated tests on Linux (headless): script compiler + VM, game loading/validation, saves, memory slots, the LLM
client and scheduler (against a fake llama-server), conversations (interjection limits, refusal, outcomes), minigame protocol,
renderer, the player UI flow, library switching, and full playthroughs of the shipped game (exclusive route, repair, the shared
four-way ending).

**Not verified / not done:**

* **The SDL window (`app/sdl_app.cpp`) has never been compiled or run** - there was no SDL and no display where it was written.
  Expect to fix a compile error or two on first build. Everything it calls into is tested; it only does windowing and input mapping.
* **Windows and macOS builds** have not been tried. The CI job for them is informational.
* **No audio.** The player emits music/sound requests through an `AudioSink`; no implementation exists yet (SDL3_mixer is the plan).
  The ported game has no music wired in: the legacy soundtrack (one WAV per scene, listed in `Game/music/tracks.json`) is not in the repo.
* **A real llama-server has not been used** (only a fake that speaks the same API). The request shape follows llama.cpp's
  `/completion` documentation; field-level differences between server versions are possible.
* **No GPU renderer.** Software rendering at ~1260x840 is fine for a visual novel but is not free.
* Known gaps: no rollback; no kerning; interlaced PNGs rejected; PNG decode happens synchronously on scene change (a ~50 ms
  hitch, prefetching is the fix); saving *during* a talk restarts that talk's turn counters; no `move` transition; idle sprite
  motion is off by default.
* Poppins is bundled but its OFL license text is not in `assets/fonts/` yet (DejaVu's is). Add upstream's `OFL.txt` before release.
* The legacy `Engine/`, `Game/`, `Inference/`, `ComicEngine/` folders of the old Python/Qt version are untouched on purpose.
  `tools/port_legacy_story.py` reads from `Game/`; remove those folders only after you are happy with v2.
