# Current game format

Stillframe **engine 2** reads **game format 1**. A game is a directory with `game.json` at its root or a ZIP with the same layout and a `.stillgame` extension. The library discovers both under `games/`. The original Panel engine and its project formats are on branch `v1`.

## Smallest playable game

Create `games/hello/game.json`:

```json
{"format":"stillgame","version":1,"id":"hello","title":"Hello"}
```

Create `games/hello/scripts/main.sf`:

```text
label start:
    narrator "Hello from Stillframe."
    end
```

Run `stillframe check games/hello` with your built executable. The loader requires a package ID and a supported format version. The entry label defaults to `start`. Use `game_version` for your own release number. The optional `engine` field checks the required major version.

## What authors can ship

| Area | Current behavior |
| --- | --- |
| Story | Indented `.sf` scripts compile together. Labels, menus, expressions, calls, conditional branches, and conversations can share game variables. |
| Characters | Character JSON declares identity, memory and named PNG poses. Pose paths resolve relative to the character file. Conditions can restrict which poses a conversation may select. |
| Stage and layout | Backgrounds, whole-sprite poses, built-in motions and transitions use a logical canvas. Games can set colors, fonts, text speed and layout. The player supports non-interlaced PNG and TrueType fonts with glyph outlines. Layered rigs, timeline clips and font kerning are outside the current implementation. |
| Audio | Scripts can reference music and sound. Validation checks asset references. The player has no playback backend yet. |
| Minigames | A manifest declares an external process, its outcomes and a fallback. The game and process exchange JSON lines. Include a runnable command for each target platform. |
| Saves | The player stores progress outside the game package. Loading a save made during a conversation restarts its turn counters. |

The validator checks static references such as speakers, poses, backgrounds, positions and minigames. Playtesting covers behavior that depends on a model reply, a running minigame or player input.

See the [manifest and asset reference](docs/GAME_FORMAT.md), [script reference](docs/SCRIPT.md), [minigame protocol](docs/MINIGAMES.md), and [shipped game](games/the-light-we-leave-on/game.json).
