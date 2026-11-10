# Authoring a game for Panel

Start here when preparing characters, artwork, animation or a complete game for the Python CPU engine. These guides describe the implemented version 1 format. Examples with asset paths require your own files at those paths; the existing runnable fixture is in [Engine/examples](../Engine/examples).

| Guide | What it explains |
|---|---|
| [01 — Game project](01-GAME-PROJECT.md) | Folder layout, entry point, intro scene, paths, required fields |
| [02 — Scenes and story](02-SCENES-AND-STORY.md) | Scene contents, story flow, choices, transitions, current limitations |
| [03 — Characters and poses](03-CHARACTERS-AND-POSES.md) | Character identity, per-scene prompts/history, pose and clip registration |
| [04 — Sprites, layers and skeletons](04-SPRITES-LAYERS-SKELETONS.md) | Artwork preparation, rig structure, joints, pivots, coordinates, binding |
| [05 — Animations](05-ANIMATIONS.md) | Timing, per-part tracks, interpolation, loops, complete clip example |
| [06 — Fonts, interface and audio](06-FONTS-UI-AUDIO.md) | Theme fields, character bubbles, backgrounds, optional music |
| [07 — Inference and continuity](07-INFERENCE-AND-CONTINUITY.md) | Connection settings, sessions, history, animation events, saved data |
| [08 — Editor workflow and troubleshooting](08-EDITOR-AND-VALIDATION.md) | Step-by-step authoring, validation, common mistakes, CPU asset guidance |

## How the files connect

```text
project.panel
  └─ scenes: scene ID → .scene path
       ├─ background and music paths
       ├─ text, buttons, scene theme
       └─ actors[]: character instance in this scene
            ├─ startup_prompt, history, bubble settings
            └─ poses: pose ID → pose definition
                 ├─ rig → .rig file
                 │    ├─ joints[] → parent hierarchy
                 │    └─ layers[] → image paths and joint bindings
                 └─ animations: animation ID → .anim file
                      └─ duration, loop, timed tracks
```

There is currently **no separate `.character` or `.story` file loader**. Character declarations live in each scene's `actors` array. Story flow lives in scene text and button actions. Rig, image and animation files can be reused across scenes. A character folder is an organizational convention, not an automatically imported package.

## Rules shared by every document

- Use UTF-8 JSON: double-quoted keys/strings, `true`/`false`, `null`, no comments, no trailing commas.
- Each engine document has `format` and `version: 1`; see the individual guides for its exact format name.
- IDs are case-sensitive. Prefer stable names such as `main_actor`, `standing`, `right_elbow`, `wave`.
- Paths resolve relative to the **file containing the path**, not the engine or launcher folder. Forward slashes are convenient on Windows too.
- These files contain data, not executable Python or expressions. Adding an undocumented field does not implement a new behavior; some extra fields may simply be ignored.
- There is no fixed number of characters, poses, layers, joints or clips, and no hardcoded three-second animation schedule. Available memory and CPU time remain practical limits.

Use the new root launchers: [start-engine.cmd](../start-engine.cmd) and [start-editor.cmd](../start-editor.cmd). The C++ binary formats and old `app.zip` are legacy and do not describe this engine.
