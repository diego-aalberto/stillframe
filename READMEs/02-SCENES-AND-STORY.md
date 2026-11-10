# 02 — Scenes and story

A `.scene` describes one location or presentation state: background, actors, opening text, buttons, theme and optional music. Every intro or ending is an ordinary scene registered in the project's `scenes` object.

## Complete scene without characters

```json
{
  "format": "panel.scene",
  "version": 1,
  "background": null,
  "text": "Choose where to go.",
  "music": null,
  "theme": {"button_fill": "#39445b"},
  "actors": [],
  "buttons": [
    {"label": "Enter the room", "action": {"type": "scene", "scene": "room"}},
    {"label": "Read a note", "action": {"type": "text", "text": "An authored note."}}
  ]
}
```

The project must register `room` before that transition is valid.

| Field | Expected value | Required / default |
|---|---|---|
| `format`, `version` | `"panel.scene"`, `1` | Required |
| `background` | Image path or `null` | Optional; theme background color |
| `text` | Initial visible dialogue/intro string | Optional; empty |
| `actors` | Array of character declarations | Optional; empty |
| `buttons` | Array of `{label, action}` objects | Optional; empty |
| `theme` | Overrides to project UI settings | Optional |
| `music` | Audio settings or `null` | Optional; silent |

See [Characters and poses](03-CHARACTERS-AND-POSES.md) for the `actors` contents. See [UI and audio](06-FONTS-UI-AUDIO.md) for theme and music.

## The three supported actions

Each button has a display `label` and exactly one `action` object.

**Switch scene:** the `scene` value is a registered ID, not a filename.

```json
{"label":"Continue","action":{"type":"scene","scene":"room"}}
```

**Play a clip:** actor, pose and animation must exist in the current scene. If the actor is in another pose, this switches its rig and starts the named clip from time zero. Replaying the same clip restarts it.

```json
{"label":"Wave","action":{"type":"animation","actor":"speaker","pose":"standing","animation":"wave"}}
```

**Display authored text:** replaces the currently displayed dialogue. It does not append to a transcript or send anything to inference.

```json
{"label":"Inspect","action":{"type":"text","text":"The door is open."}}
```

There is no pose-only action. To switch to a static pose through a button, declare an animation with positive duration, `loop: false`, and `tracks: []`, and reference that animation.

## Representing a story today

Register an opening, intermediate scenes and endings. Connect them with scene buttons. Put scene-specific character knowledge into `startup_prompt` and `history`. Multiple buttons can lead to different scenes, so authored branching navigation is possible.

On entering a scene, the engine rebuilds its declared actors, starts their default clips, displays its `text`, and replaces the previous scene's music. Returning to the scene starts its visual setup again. Inference session continuity follows separate rules described in [guide 07](07-INFERENCE-AND-CONTINUITY.md).

## Current boundaries

The engine does **not** currently implement a story-variable store, relationship scores, inventory, conditional button visibility, scripted sequences of actions, automatic timed scene transitions, cross-scene knowledge propagation, or full game-progress save/load. A `.story` file or fields such as `conditions`, `on_enter`, `next_scene_after`, and `relationship` are not interpreted.

Buttons do not apply hidden story consequences. If a player chooses one branch, that choice is not automatically copied into a later character's prompt. Author a distinct destination scene/history for the branch when using the existing format.

While generation is active, scene/action buttons and character selection are disabled. The player can draft a message; sending becomes available at the completion event. There is one visible dialogue area, not a separate speech bubble over every character.
