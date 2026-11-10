# Stillframe: current game structure and file formats

This document describes the implementation on disk as of 10 November 2026.
Stillframe is the chosen engine name; existing code and file identifiers still
use Panel. This document does not introduce new formats or change the engine.

## Current approach

A game currently consists of a folder of related documents, artwork and code.
There is no single compressed, self-contained game file. The active engine is
Python/PySide6 with CPU software rendering. Local inference runs as a separate
service backed by llama.cpp; its model weights are separate from game content.

The engine understands generic projects, scenes, actors, rigs and animations.
The current game, The Light We Leave On, adds its own Python story controller
for progression, relationships, conversation outcomes and persistent memory.
Opening its project in the generic player previews content; it does not supply
the complete game's progression rules.

## Custom formats already implemented

These are UTF-8 JSON documents, not compressed archives or binary containers.
Each document has a checked `format` identifier and `version` number. Current
documents use version 1. Paths resolve relative to the document containing them;
keeping the folder structure intact is necessary when moving a project.

| Extension | Format identifier | Contents |
|---|---|---|
| `.panel` | `panel.project` | Entry point, title, canvas, initial scene, scene-path index, theme and optional inference configuration |
| `.scene` | `panel.scene` | Background reference, actors, opening text, startup prompts/history, poses, buttons, theme and optional music |
| `.rig` | `panel.rig` | Joint hierarchy, independent image layers, binding, pivots and local transforms |
| `.anim` | `panel.animation` | Duration, looping and independently timed transform-channel keyframes |
| `.sessions` | `panel.sessions` | Optional generated inference session-ID cache; this is runtime state rather than authored story content |

Images, fonts and audio remain external assets referenced by these documents.
There is no arbitrary embedded Python or script execution in the format. Generic
scene buttons support scene changes, animation selection and text changes.
See `Engine/FORMAT.md` for the complete schema and supported behavior.

## Where the current game's content lives

```text
Engine/                         Generic player, editor, formats and tests
Game/
  story.py                      Character definitions, story nodes and routes
  runtime.py                    Progression, relationship gates and save handling
  conversation.py               Character-local memory and streamed reply decoding
  encounter.py                  Conversation lifecycle, refusal and repair rules
  panel_player.py               Story controller adapter for the generic engine
  StoryPlayer.qml               Game-specific interface
  assets/                       Original character atlases and backgrounds
  panel/
    story.panel                 Exported visual project entry point
    scenes/                     Exported scene documents
    rigs/                       Pose rigs
    animations/                 Reusable motion clips
    sprites/                    Extracted pose images
  music/                        Music mappings and any supplied tracks
  data/save.json                Default player save, created during play
Inference/
  models/model.gguf             Local model weights; excluded from Git
  inference/
    data/memory.sqlite3          Default persistent conversation database
    bin/                        Local llama.cpp executables and libraries
Story/                          Human-readable story guides and scene graph
READMEs/                        Authoring documentation organized by topic
```

The exported project contains 35 story scenes plus four character-specific repair
scenes, 48 pose sprites/rigs and ten reusable motion clips. The story graph and
rules are not entirely represented by those scene files: `story.py` and the
controller are still required to play the complete game.

`Game/export_panel.py` regenerates the visual project from the existing story
catalog and artwork. Regeneration overwrites generated files; manual changes in
the export need to be preserved or incorporated into the exporter first.

## Sprites and animation style

The generic engine supports separately authored image layers bound to joints,
with position, rotation, scale and opacity animation. Key times are arbitrary;
channels support the interpolation modes documented in the format guide.

The current game's artwork uses flattened illustrated poses. Each exported pose
is one rigid image layer with subtle motion. It has no independently movable
arms, legs or head. The ten motion clips are reused across poses; the combinations
are not hundreds of separately illustrated animations. Independent body-part
motion requires separate transparent layers authored for that purpose. Automatic
segmentation, mesh deformation and inverse kinematics are not implemented in
the active engine.

## Starting knowledge versus accumulated memory

“Original memory” can refer to two different kinds of information:

| Information | Current storage and behavior |
|---|---|
| Character identity, personality and starting knowledge | Authored character definitions and prompts in `Game/story.py`; the generic scene format also supports actor `startup_prompt` and authored `history` |
| Progress, relationships, personal facts, emotional incidents and witnessed events | Game save state, normally `Game/data/save.json`, managed by the story controller |
| Model conversation transcripts and persisted session context | Inference SQLite database, normally `Inference/inference/data/memory.sqlite3`; configurable through `MEMORY_DB` |
| Generic project session references | Optional `.sessions` cache identifying backend sessions; it does not contain their complete transcripts |

The story controller maintains character-specific sessions and controls which
events each character witnesses. Its local memories supplement backend context
management; they are not simply the entire transcript inserted into every turn.
Pending requests retain information needed for idempotent retries.

Changing an authored prompt does not automatically replace the system prompt of
an existing backend session. A fresh session is needed when starting with revised
character initialization. Preserving full player continuity requires the related
game save and inference database, not just the project file or session IDs.

## What is and is not portable today

The project documents and assets can be compressed with an ordinary archive tool
and extracted elsewhere with their relative paths preserved. The archive itself
is not a recognized game format: the engine currently opens a `.panel` file on
disk, and the full story still needs its Python controller and dependencies.

For this game, run `start-game.cmd` from the root. It starts or reuses the local
inference backend and launches `Game/panel_player.py`. `start-engine.cmd` opens
generic projects; `start-editor.cmd` opens the authoring editor. Recreate Python
virtual environments after moving the project. Inference additionally needs
compatible llama.cpp binaries and model weights, or an appropriately configured
running service.

The root currently retains local weights, environments, saves and the legacy
`app/` and `app.zip` export. Those old exports do not represent the current Python
rewrite. Git ignore rules exclude weights and other local material from Git;
ordinary ZIP tools do not automatically follow those rules.

## Legacy embedded formats

`ComicEngine/` contains the older C++/OpenGL implementation. Its binary `.cscene`
format embeds scene images and font data; `.cimg` and `.cfont` are its related
image/font formats. They do not package the complete current story controller,
story progression and memory database. They are not the active engine's formats.
See `ComicEngine/FORMAT.md` for that separate legacy contract.

## Possible future single-file game package

A `.stillgame` archive was suggested as a future packaging direction. It is
**not implemented**, and its schema, loader and export workflow are not defined.
An eventual package could contain the project documents, assets, story graph,
character definitions and authored starting memories. Supporting the complete
current game would also require a defined way to represent its controller rules
or distribute a compatible game adapter.

Player saves and evolving conversations should normally remain outside an
immutable game package so game updates can preserve progress. Bundling a saved
playthrough would be a separate snapshot/export feature. No such single-file
game package or continuity snapshot exporter exists in the current engine.
