# Architecture

Stillframe 2 is one engine program and any number of games. A game is *data* (a folder, or a single `.stillgame`
file): scripts, characters, art, sound, optional minigames. The engine never contains story content.

```
games/<game>/               engine (this repo)
  game.json                   app/        command line, SDL3 window shell
  scripts/*.sf                render/     software renderer, stage, UI, player, library screen
  characters/*.json           core/       script compiler + VM, memory, LLM layer, minigames, saves, VFS
  backgrounds/ sprites/ ...   tests/      one binary, `sf_tests [filter]`
  minigames/<id>/
```

C++20, no third-party dependencies (own JSON, inflate/ZIP, PNG, TrueType rasteriser, HTTP/SSE client). SDL3 is the only
optional dependency, used for the window. Everything except the window is testable headless, and is.

## Layers

| Layer | Responsibility | Where |
|---|---|---|
| Script | `.sf` -> compiled program; a VM that runs until it needs the player and emits JSON events | `core/script`, `core/vm`, `core/expr` |
| Game | loads and *validates* a package (`stillframe check`), strict about everything it can check | `core/game`, `core/vfs` |
| Memory | the three-slot character memory and the turn counter | `core/memory` |
| Conversation | one `talk` block: prompts, per-speaker JSON schema, memory ops, refusals, interjections, outcomes | `core/conversation` |
| LLM | llama.cpp client (`/completion`, SSE), priority scheduler | `core/llm`, `core/http` |
| Minigames | separate programs, JSON lines over stdio | `core/minigame`, `core/process` |
| Renderer | CPU canvas, PNG/TTF, transitions, stage with motions, UI widgets | `render/*` |
| Player | ties VM, conversation, renderer, saves and audio together; modes (say/menu/talk/minigame/...) | `render/player` |
| App | library screen <-> game switching; the window backend only feeds input and shows the canvas | `render/app`, `render/launcher`, `app/sdl_app.cpp` |

## Why a rewrite in C++ - and what actually fixes the GPU problem

The old engine drew with the CPU already, so a faster renderer alone does not free the GPU. What clogged it was **inference**:

* every turn re-sent a large JSON blob (state, journals, duplicated history), so the prompt changed at the front and
  llama.cpp had to reprocess it from scratch (no KV-cache reuse);
* one session per character and two model calls per turn;
* nothing was prioritised, so background work competed with the line the player was waiting for;
* TTS will add a second GPU consumer.

What v2 does about it:

* **Stable prompt prefix.** System prompt = world premise + the character's immutable slot (never changes). Then contextual
  memory, then an append-only scene log, and only then the changing goal/notes. Requests send `cache_prompt` so the server
  keeps the shared prefix in its KV cache; each turn pays only for the new tail.
* **Slot affinity.** Each character is pinned to a llama-server slot (`llm.slots`) so her prefix stays warm.
* **One call per reply.** Reply, pose, memory operations, outcome and interjection nomination come back in one JSON object,
  constrained by a per-speaker JSON schema (`json_schema` in the request), so there is no repair pass and no second call.
* **Priorities.** `LlmScheduler` is a single worker with three classes: Player < Interject < Background. A player-facing job
  cancels a running background job; superseded jobs are cancelled instead of finishing for nobody.
* **Frame pacing.** The renderer only redraws when something changed (`needs_frame()`); an idle screen costs ~0 CPU, so the
  CPU/GPU budget goes to the model and, later, TTS.

What is *not* fixed here: there is no GPU renderer. At 1260x840 the software renderer is fast enough for a visual novel, but a
GPU path would be a sensible later step if you want heavy effects.

## Three-slot memory (per character)

1. **Immutable** - `characters/<id>.json` -> `immutable`. Names, personality, quirks, boundaries. Loaded once; the type has
   no mutators, so nothing at runtime (including the model) can alter it.
2. **Contextual** - what happened *between* characters (or with the player): interactions, tensions, promises, grudges.
   Only the character herself changes it, through validated `memory_ops` in her own reply (`add | update | resolve | forget`).
   Every op needs `evidence` that is checked against what was actually said (so a model cannot invent a grievance), has a
   severity, and a grudge may set `refuses: true` with a `refusal_line` and a `repair_hint`.
   A refusing character does not call the model: she answers from her `refusal_line` until the player says something that looks
   like a repair (keyword list in `llm.repair_words`, English/Spanish by default), and then she judges it herself.
3. **Temporal** - per character and per scene. Everything she witnessed in this scene (lines said while she was on stage,
   narration, her own speech). Flushed on every `scene` change.

Witnessing is explicit: a line only enters the scene slot of characters who were present. Absent characters learn nothing.

## Interjections

During a `talk` with several characters the model may nominate someone to cut in ("you told Mira about Nia -> Nia reacts").
Because the reply schema carries `interject: {who, about}`, no extra call is needed to *decide*; the nominated character then
gets one small call (priority Interject) and may decline (`speak: false`). Guard rails, all enforced by the engine:

* a budget per talk (`interject_limit`, default 3; a nomination spends one even if she declines);
* a cooldown measured in player turns (`interject_cooldown`);
* nobody interjects twice in a row, interjections never chain, refusing characters don't interject, and only people in the
  cast of the current talk can be nominated;
* scripts can switch it off per talk (`interject: off`).

## Saves

6 slots + autosave + quicksave (F5/F9) under the user data directory, never inside a game. A save stores the VM state, variables,
all memory slots, the turn counter and the script position; a game update that changes scripts restores as closely as it can
and tells the player. Saves during a `talk` restart that conversation's counters (limit: see README).
