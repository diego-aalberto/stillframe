# Minigames

A minigame is a **separate program**. The engine starts it, tells it what the story knows, and waits for one result. The engine
never links against a minigame, so it can be written in anything and a crash cannot take the story down.

```
games/<game>/minigames/<id>/minigame.json
{
  "id": "duel",
  "title": "Rooftop duel",
  "outcomes": ["win", "lose", "draw"],
  "fallback_outcome": "lose",              // used if it crashes, never starts or is cancelled
  "timeout_s": 600,                        // 0 = no limit
  "launch": {
    "type": "process",
    "command": ["{dir}/duel"],             // {dir} = the minigame's folder (extracted once from a .stillgame)
    "windows": ["{dir}/duel.exe"],         // optional per-platform overrides: windows | linux | macos
    "startup_timeout_s": 20
  }
}
```

In a script:

```
minigame duel(difficulty=2, opponent="mira") -> r
if r.outcome == "win":
    jump celebrate
```

`r.outcome` is what the program reported; `r.data` is whatever else it sent. If the minigame fails, the player sees an error panel
with Retry/Skip, and a skipped minigame yields `fallback_outcome`.

## Protocol (version 1)

UTF-8 JSON, one object per line, over the child's stdin/stdout. Stdout lines that do not start with `{` are treated as the
program's own logging and ignored.

| Direction | Message |
|---|---|
| engine -> game | `{"type":"start","protocol":1,"game":"<id>","args":{...},"context":{...}}` |
| engine -> game | `{"type":"say_result","id":N,"text":"..."}` - answer to a `say` request |
| engine -> game | `{"type":"cancel"}` - stop; the result is no longer needed |
| game -> engine | `{"type":"ready"}` - once the program is up |
| game -> engine | `{"type":"progress","value":0..1,"label":"..."}` - optional |
| game -> engine | `{"type":"event","name":"...","data":{...}}` - optional, forwarded to the UI |
| game -> engine | `{"type":"say","id":N,"character":"hana","prompt":"..."}` - ask a story character for one in-character line |
| game -> engine | `{"type":"result","outcome":"win","data":{...}}` - the end |

`tools/sample_minigame.py` is a complete, 60-line reference implementation.

## Ideas that fit this design

All of these are "scene flow jumps out, plays something with full freedom, comes back with an outcome":

* **Turn-based RPG fight** (own Godot/Unity/Love2D build). Pass the character's relationship state in `args` so trust changes her
  moves; outcome `win|lose|flee` picks the story path, `data` carries HP left for the aftermath scene.
* **Cooking / café service** - timed orders; `data.score` decides whether Hana is impressed. Pairs naturally with `say`: the
  game asks the character for a one-line reaction to *this* dish.
* **Rhythm game** to a track from the soundtrack; outcome by grade.
* **Photography minigame** (Leila): frame a shot in a scene; the result is a picture the story can show later.
* **Bike repair puzzle** (Mira): assemble parts under a timer.
* **Carpentry / shelf fitting** (Tessa): a physics puzzle.
* **Dialogue-driven mini-scenes**: a card-based "argument" where each card is a line the character answers through `say`.
* **Web or pygame toys** behind a tiny launcher script - anything that can read a line from stdin.
* **Non-game utilities**: a character sheet viewer, a map, a photo gallery.

Design tips: keep the minigame replayable (the player can Retry), return a small fixed set of outcomes (the script can then branch
on all of them), and always declare a `fallback_outcome`.
