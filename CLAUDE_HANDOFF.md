# Claude handoff: Panel / The Light We Leave On

Prepared 10 November 2026 from the files currently on disk. Start here, then read
`README.md`, `Engine/README.md`, `Engine/FORMAT.md`, `Game/README.md` and
`READMEs/README.md`. The handoff stays in this existing root folder: no separate
dev folder or ZIP was created. The local LLM weights and installed environments
remain on disk. Weight files are excluded from Git through the root `.gitignore`
and `Inference/.gitignore`; a normal ZIP tool does not honor Git ignore rules.

## Current implementation

The active engine is **Engine/**: Python with PySide6 6.8.3. The player uses Qt
Quick's software scenegraph; the editor uses raster widgets. Keep rendering on
the CPU. Inference is a separate optional local service and may use a GPU.

- `Engine/comic/core.py`: versioned JSON project/scene/rig/animation loading,
  validation and transform/keyframe evaluation. File extensions are `.panel`,
  `.scene`, `.rig` and `.anim`; see FORMAT.md for the actual contract.
- `Engine/comic/runtime.py`, `Player.qml`: generic project playback and streaming.
- `Engine/comic/editor.py`: image layers, joints, rest transforms, animation keys,
  timelines, scrubbing and undo/redo. Project/scenes are edited as JSON documents.
- `Game/story.py`, `runtime.py`, `conversation.py`, `encounter.py`, `hints.py`:
  story catalog, progression, saves, relationship gates, persistent memories,
  encounter refusal/repair and model response interpretation.
- `Game/panel_player.py`, `StoryPlayer.qml`: current story controller adapter/UI.
  Launch the actual game through this adapter, not the generic project preview.
- `Game/panel/`: already exported 35 story scenes plus four repair scenes,
  48 pose sprites/rigs and ten reusable rigid animation clips. Keep relative
  paths to `Game/assets/backgrounds` intact. Original atlases/backgrounds and
  their prompt metadata are included in `Game/assets/`.
- `Inference/inference/`: local streaming API, llama.cpp process management,
  SQLite conversation storage, retries, context management and download recipes.
  The service commits the response and releases the session lock before `done`;
  the UI unlocks on completion rather than waiting for socket teardown.
- `Story/`: story bible, scene graph and LaTeX story map.

The story currently uses a single joint speech/decision response per addressed
character. Save and memory continuity, idempotent pending-request recovery,
private versus witnessed exchanges, relationship prerequisites and refusal
handling are implemented. Read the controller/tests before changing these rules.

## Legacy material and misleading older docs

`ComicEngine/` is the retained C++/OpenGL predecessor. Its source, build scripts,
samples and tests are included for reference and because the generic engine
example references `ComicEngine/samples/anime/standing.png`. Its compiled binaries
remain local. It is not the active Python engine. `Game/server.py`, native smoke
scripts and some exporters support this older integration.

`Game/BUILD_STATUS.md` records the older native build. Its separate director
sessions, skeletal motion and verification counts must not be read as a report
on the current Python port. The root README and Game README describe the newer
architecture. The old `app/` and `app.zip` portable export remain in the root and have
not been rebuilt for the Python rewrite.

## Set up on the receiving Windows machine

The existing environments are installed locally. On another machine, use CPython
3.12 with pip and venv. Run these from this project's root folder:

```bat
python -m venv Engine\.venv
Engine\.venv\Scripts\python.exe -m pip install -r Engine\requirements.txt
Engine\.venv\Scripts\python.exe -m pip install "pytest>=8,<10"
start-engine.cmd Engine\examples\project.panel
start-editor.cmd Engine\examples\project.panel
```

The generic player and editor do not require inference. On a fresh machine, to
enable the story's local inference backend, create its separate environment and
restore weights/binaries if they were not transferred:

```bat
python -m venv Inference\.venv
Inference\.venv\Scripts\python.exe -m pip install -r Inference\inference\requirements-dev.txt
cd Inference
.venv\Scripts\python.exe -m inference.download --model --windows-engine
cd ..
start-game.cmd
```

The download is several GB and requires network access. The supplied Windows
engine recipe targets CUDA 13.3; consult `Inference/inference/README.md` for
compatible alternatives. The model is expected at `Inference/models/model.gguf`
and llama-server/runtime libraries at `Inference/inference/bin/`. CPU mode is
available through `Inference/start-backend-cpu.cmd` once compatible binaries and
weights exist. Stop the managed service with `Inference/end-backend.cmd`.

`setup-engine.cmd` is also provided as an engine setup convenience. Recreate
virtual environments after moving; their installed paths may not be portable.
The `.cmd` launchers target Windows; other platforms need direct Python commands
and an appropriate llama.cpp build.

## Checks and useful commands

```bat
Engine\.venv\Scripts\python.exe Engine\run.py Engine\examples\project.panel --validate
Engine\.venv\Scripts\python.exe Engine\run.py Game\panel\story.panel --validate
Engine\.venv\Scripts\python.exe -m pytest Engine\tests Game\test_panel_port.py Game\test_runtime.py Game\test_encounter.py -q
Engine\.venv\Scripts\python.exe Engine\run.py Engine\examples\project.panel --scene stage --smoke-seconds 5
cd Inference
.venv\Scripts\python.exe -m pytest inference\tests -q
cd ..
```

The story/engine tests mock inference; a passing suite does not establish real
model behavior, CUDA support or target-device performance. GUI tests need Qt to
initialize. See `HANDOFF_VERIFICATION.md` for checks actually rerun during this
handoff rather than treating older documentation as fresh evidence.

`Game/export_panel.py` rebuilds the port from the included original artwork;
it overwrites generated panel files, so preserve manual changes first.
`Game/export_story.py` rebuilds the story guide/LaTeX. No regeneration is needed
to open the existing project.

## Limits and next-work context

No further feature request has been specified; the task here was to prepare this
handoff. Do not assume that any of the following limitations are new assignments.

The current story poses are flattened images animated as single rigid layers.
Independent limb/head movement requires separately authored transparent layers.
There is no automatic segmentation, mesh deformation, IK or painting tool in the
new engine. Arbitrary key times and channel interpolation are supported; clips
are reused across poses, not hundreds of separately drawn animations.

Model-directed animation events are an integration extension: the generic player
can accept declared events, but the current service emits text events. Story pose
selection is handled by the story response adapter. Outcome classification is
probabilistic. One addressed character speaks per turn. Music mappings are
included but actual WAV tracks have not been supplied; missing tracks are silent.

## Root layout and transfer notes

All source, artwork, local weights, environments, saves and inference history
remain in their original locations. No existing project assets or user state
were removed or moved. The root `.gitignore` excludes model weights, installed
environments, caches and the obsolete portable export from Git. The existing
Inference rules also exclude downloaded binaries, databases and session files.

For a source-only transfer later, omit `app/`, `app.zip`, `.venv/` directories,
`Inference/models/model.gguf`, `Inference/inference/bin/`,
`Inference/inference/downloads/`, generated test/cache folders, `Game/data/` and
`Inference/inference/data/`. Keep all source, `Game/assets/`, `Game/panel/`,
`Game/music/`, `Engine/examples/` and `ComicEngine/samples/` together to preserve
relative paths. Include saves/databases separately only if continuity is wanted.
Git ignore rules do not make a root-folder ZIP small automatically.

This workspace inherits Git metadata from its parent Coding folder. Scope Git
commands to this project; the parent's unrelated changes are not this handoff.
