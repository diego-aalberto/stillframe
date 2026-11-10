# 03 — Characters and poses

A character appears as an object in a scene's `actors` array. Its artwork and clips live in reusable files. There is no automatically loaded character manifest: a `character.json` beside your images will not be read unless future engine support is added.

## Character declaration

This example belongs inside `scenes/room.scene` → `actors`:

```json
{
  "id": "speaker",
  "name": "Display name",
  "x": 500,
  "y": 40,
  "rotation": 0,
  "sx": 0.5,
  "sy": 0.5,
  "z": 0,
  "startup_prompt": "You are the character described by the author. Speak directly to the player. Use the supplied scene history as continuity.",
  "history": [
    "The character has already met the player.",
    "They agreed to meet in this room."
  ],
  "bubble": {
    "font_family": "Segoe UI",
    "font_size": 20,
    "bubble_fill": "#ed2b3045",
    "bubble_edge": "#dab982",
    "text": "#ffffff"
  },
  "pose": "standing",
  "poses": {
    "standing": {
      "rig": "../characters/speaker/standing.rig",
      "animation": "idle",
      "animations": {
        "idle": "../characters/speaker/animations/idle.anim",
        "wave": "../characters/speaker/animations/wave.anim"
      }
    },
    "seated": {
      "rig": "../characters/speaker/seated.rig",
      "animations": {
        "leg_swing": "../characters/speaker/animations/leg-swing.anim"
      }
    }
  }
}
```

The example describes structure; create each referenced rig, animation and image before running it.

## Character fields

| Field | Meaning | Required / default |
|---|---|---|
| `id` | Actor identifier used by buttons and inference sessions | Required; unique within the scene |
| `name` | Display name in the character selector | Optional; uses `id` |
| `x`, `y` | Placement of the rig in scene coordinates | Optional; `0`, `0` |
| `rotation` | Whole-rig clockwise rotation in degrees | Optional; `0` |
| `sx`, `sy` | Whole-rig scale | Optional; `1`, `1` |
| `pivot` | Whole-rig anchor `[x,y]` | Optional; `[0,0]` |
| `z` | Added to every layer's drawing order | Optional; `0` |
| `startup_prompt` | Author's per-scene character instructions | Optional; empty string |
| `history` | Author's continuity notes; use an array of strings | Optional; empty array |
| `bubble` | Selected character's theme overrides | Optional; inherits scene/project |
| `pose` | Initial pose ID, present in `poses` | Required |
| `poses` | Object mapping pose IDs to definitions | Required |

Actor-level `opacity` is not applied by the current renderer. To fade the entire character, animate a root joint's opacity and bind its parts beneath that root.

## What a pose contains

Every pose needs a `rig` path. Its `animations` object maps names to `.anim` paths. Its optional `animation` selects the clip started when entering the scene in that pose. Without it, the rig displays at rest.

A pose is a distinct arrangement of layers and joints, such as standing or seated. A clip moves that arrangement over time. A leg-swing clip authored for a seated rig is not automatically compatible with a standing rig: all target IDs must exist, and their local coordinates must have the intended meaning.

The same `.anim` can be reused across rigs only when their named targets and coordinate conventions are compatible. The engine validates target existence, not artistic suitability.

## Reusing a character across scenes

Repeat the actor declaration in each scene, using the same rig and clip paths. Change its scene placement, initial pose, startup prompt, authored history or bubble settings as needed. Keep its actor ID stable for clarity, but note that inference sessions are keyed by **both scene ID and actor ID**; the same actor in another scene gets a separate session.

The engine does not automatically import biography, discover images, copy history from another scene, or infer pose names from filenames. Put personality, knowledge and speaking style into `startup_prompt`. Keep visual capabilities in `poses`, where they can be validated.

## Character handoff checklist

Provide the actor declaration, all registered pose rigs, all referenced layer images, all animation files, and any custom fonts. Include a short note describing which joint each layer should follow and where each image's pivot lies. Do not deliver only a contact sheet or a flattened character image when independent body-part motion is expected.

Next: [Sprites, layers and skeletons](04-SPRITES-LAYERS-SKELETONS.md).
