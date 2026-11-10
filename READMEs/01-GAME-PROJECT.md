# 01 — Game project

A `.panel` file is the game's entry point. It registers scenes and chooses the intro. The player reads this file; it does not scan folders to discover characters or story content.

## Suggested game folder

```text
my-game/
  project.panel
  scenes/
    opening.scene
    room.scene
  characters/
    speaker/
      standing.rig
      seated.rig
      images/
        standing-body.png
        standing-head.png
        standing-arm.png
      animations/
        idle.anim
        wave.anim
      fonts/
        dialogue.ttf
  backgrounds/
    room.png
  music/
    room.ogg
  fonts/
    ui.ttf
  state/
    conversations.sessions       (optional, generated at runtime)
```

Only referenced files are used. The folder names above are conventions; you can choose others. Keep authored assets separate from generated state.

## Minimal complete project

Save this as `my-game/project.panel`:

```json
{
  "format": "panel.project",
  "version": 1,
  "title": "My game",
  "canvas": [1280, 720],
  "intro": "opening",
  "scenes": {
    "opening": "scenes/opening.scene"
  },
  "theme": {},
  "inference": {}
}
```

Save this as `my-game/scenes/opening.scene`:

```json
{
  "format": "panel.scene",
  "version": 1,
  "text": "Welcome.",
  "actors": [],
  "buttons": []
}
```

These two files run without artwork, audio or inference. Open the `.panel` with `start-engine.cmd`, or pass its path as an argument.

## Project fields

| Field | Expected value | Required / default |
|---|---|---|
| `format` | `"panel.project"` | Required |
| `version` | Integer `1` | Required |
| `title` | Window title string | Optional; `Panel` |
| `canvas` | `[width, height]`, positive integers | Optional; `[1280, 720]` |
| `intro` | A key in `scenes` | Required |
| `scenes` | Object mapping scene IDs to `.scene` paths | Required |
| `theme` | UI defaults object | Optional; neutral defaults |
| `inference` | Connection settings object | Optional; no URL means offline |

Canvas dimensions define stage coordinates. The stage scales proportionally to the window. The dialogue interface is laid out in window space, rather than being authored at sprite coordinates.

## Path resolution examples

| Reference location | Value | Resolves to |
|---|---|---|
| `my-game/project.panel`, `scenes.room` | `scenes/room.scene` | `my-game/scenes/room.scene` |
| `my-game/scenes/room.scene`, pose `rig` | `../characters/speaker/standing.rig` | `my-game/characters/speaker/standing.rig` |
| `my-game/scenes/room.scene`, animation path | `../characters/speaker/animations/wave.anim` | `my-game/characters/speaker/animations/wave.anim` |
| `my-game/characters/speaker/standing.rig`, layer `image` | `images/standing-arm.png` | `my-game/characters/speaker/images/standing-arm.png` |

Animation paths in a pose are relative to the **scene**, even though the rig is somewhere else. Images inside the rig are relative to the **rig**. This distinction is a common source of broken imports.

The loader currently visits all registered scenes, rigs and clips at project load. A broken reference in a later scene can therefore prevent project loading. Images are separate standard image files; the custom format does not embed them in an archive.

Next: [Scenes and story](02-SCENES-AND-STORY.md).
