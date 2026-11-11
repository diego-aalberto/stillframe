# Game format

A game is a folder, or the same folder packed into one `.stillgame` file (`stillframe pack <folder>`; a `.stillgame` is a ZIP
the engine reads directly). Put games in the engine's `games/` folder (or point `STILLFRAME_GAMES` / `--games` elsewhere); the
library screen lists them. `stillframe check <game>` validates everything and is the first thing to run when something is wrong.

```
my-game/
  game.json
  scripts/*.sf
  characters/<id>.json            file name must equal "id"
  backgrounds/<id>.png            referenced by `scene <id>`
  sprites/...                     any layout; characters point at their files
  audio/ ...                      music and sounds
  minigames/<id>/minigame.json    see MINIGAMES.md
```

## game.json

```json
{
  "format": "stillgame", "version": 1,
  "id": "the-light-we-leave-on", "title": "The Light We Leave On", "author": "...", "game_version": "2.0.0",
  "engine": ">=2",
  "start": "start",                       // label to begin at
  "cover": "backgrounds/cafe.png",        // shown on the library card
  "canvas": [1260, 840],                  // logical size; the window scales it with letterboxing
  "variables": { "player": "You", "rel": { "hana": { "trust": 1 } } },   // initial state; saved with the game
  "positions": { "far_left": 0.14 },      // extra named stage positions (0..1 across the screen)
  "strings": { "continue": "Next" },      // override any UI text (translations)
  "llm": {
    "system": "World premise, tone, content rules - one paragraph. Shared by every character.",
    "server": "http://127.0.0.1:8080",    // llama-server; the player can also start it themselves
    "template": "chatml",                 // or "plain"
    "max_tokens": 320, "temperature": 0.8,
    "slots": 4,                           // llama-server --parallel N; characters are spread across them
    "interject_limit": 2, "interject_cooldown": 2,
    "repair_words": ["sorry", "lo siento"],
    "language": "",                       // force a reply language; empty = the player's
    "enabled": true
  },
  "theme": {
    "colors": { "accent": "#f0c987", "box": "#0d0f1aea", "text": "#f2f2f8" },
    "layout": { "box_height": 0.27, "text_size": 0.034, "radius": 14, "sprite_height": 0.94 },
    "font": "fonts/Mine.ttf", "font_bold": "fonts/Mine-Bold.ttf",   // optional; the engine ships Poppins + DejaVu fallback
    "chars_per_second": 52, "idle_motion": false
  }
}
```

Colour keys: `text text_dim narration box box_border accent button button_hover button_border input overlay good warn`.
Layout sizes are fractions of the canvas (`radius` is in pixels), so a theme survives a canvas change. Colours are `#rgb`, `#rrggbb` or `#rrggbbaa`.

## characters/<id>.json

```json
{
  "id": "hana", "name": "Hana", "color": "#67b3aa",
  "immutable": [
    { "label": "personality", "text": "Observant, quietly stubborn ..." },
    "Hums when nervous."
  ],
  "sprites": {
    "poses": { "neutral": "../sprites/hana-neutral.png", "shy": "../sprites/hana-shy.png" },
    "default_pose": "neutral", "scale": 1.0, "anchor": [0.5, 1.0], "y_offset": 0.0
  },
  "pose_requires": { "shy": "rel.hana.trust >= 2" }
}
```

* `immutable` is her fixed identity slot (see ARCHITECTURE.md). Everything the model must never get wrong goes here.
* The model may only choose among `poses`. `pose_requires` removes a pose from what the model is offered while its expression is
  false (evaluated when a talk starts) - the ported game uses it so romantic poses only exist once a relationship does.
* Sprite paths are relative to the character file. PNG only (8/16-bit, all colour types; interlaced PNG is not supported).

## Assets

* Backgrounds: any size; they are scaled to *cover* the canvas.
* Fonts: TrueType (`glyf` outlines) with a fallback chain; no kerning/GPOS yet.
* Audio: referenced by script (`play music "audio/theme.ogg"`). The engine decides *what* plays; actual playback needs an
  audio backend, which does not exist yet (see README, "Not done").
