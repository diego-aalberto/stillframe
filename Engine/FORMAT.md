# Panel format, version 1

Panel is a custom data contract encoded as UTF-8 JSON. The extensions distinguish document types; `format` and `version` are mandatory and checked. No Python, expressions, scripts, shell commands or arbitrary code can be embedded. IDs are case-sensitive strings. Paths are relative to the file containing the path, not the working directory. Absolute paths work but make projects less portable.

| Extension | format | Purpose |
|---|---|---|
| `.panel` | `panel.project` | Entry point, intro, scene paths, canvas, UI defaults, optional inference |
| `.scene` | `panel.scene` | Background, actors, prompts/history, poses, buttons, theme, optional music |
| `.rig` | `panel.rig` | Joint hierarchy and separately transformed image layers |
| `.anim` | `panel.animation` | Duration, looping and independently timed channel tracks |
| `.sessions` | `panel.sessions` | Optional generated session-ID cache; not authored story data |

## Project

```json
{
  "format": "panel.project",
  "version": 1,
  "title": "My project",
  "canvas": [1280, 720],
  "intro": "opening",
  "scenes": {"opening": "scenes/opening.scene", "room": "scenes/room.scene"},
  "theme": {
    "background": "#20232b",
    "font": "fonts/dialogue.ttf",
    "font_size": 18,
    "text": "#ffffff",
    "button_fill": "#39445b",
    "button_hover": "#62728d",
    "button_pressed": "#26334d",
    "button_edge": "#aab6cb",
    "button_text": "#ffffff",
    "button_radius": 6,
    "bubble_fill": "#e6202938",
    "bubble_edge": "#aab6cb",
    "bubble_border": 2,
    "bubble_radius": 12
  },
  "inference": {}
}
```

The intro is an ordinary scene, not hardcoded engine content. Canvas defines logical coordinates; the stage scales proportionally to the window. UI remains window-sized. Theme colors accept Qt color names, `#RRGGBB`, or `#AARRGGBB`. `font` loads a project-supplied font; alternatively `font_family` names an installed font. Omitted UI fields have neutral functional defaults.

Optional inference configuration:

```json
{
  "url": "http://127.0.0.1:8000",
  "key_env": "INFERENCE_API_KEY",
  "max_tokens": 512,
  "temperature": 0.7,
  "timeout": 660,
  "session_store": "state/conversations.sessions"
}
```

Omit `inference`, use `{}`, or omit its `url` for offline operation. Changing startup prompts does not rewrite an existing saved service session; choose a new session store if you intend to restart continuity. The engine does not start or select hardware for inference.

## Scene

```json
{
  "format": "panel.scene",
  "version": 1,
  "background": "../images/room.png",
  "text": "Authored introductory text.",
  "theme": {"button_fill": "#48536c"},
  "music": {"path": "../audio/room.ogg", "volume": 0.4, "loop": true},
  "actors": [{
    "id": "speaker",
    "name": "Display name",
    "x": 400, "y": 20, "sx": 0.5, "sy": 0.5, "z": 0,
    "startup_prompt": "The author's per-scene character prompt.",
    "history": ["An authored continuity note.", "Another earlier event."],
    "bubble": {
      "font": "../fonts/character.ttf",
      "font_size": 20,
      "bubble_fill": "#ed2b3045",
      "bubble_edge": "#dab982",
      "text": "#ffffff"
    },
    "pose": "standing",
    "poses": {
      "standing": {
        "rig": "../rigs/standing.rig",
        "animation": "idle",
        "animations": {"idle": "../clips/idle.anim", "wave": "../clips/wave.anim"}
      },
      "seated": {
        "rig": "../rigs/seated.rig",
        "animations": {"swing": "../clips/swing.anim"}
      }
    }
  }],
  "buttons": [
    {"label": "Continue", "action": {"type": "scene", "scene": "room"}},
    {"label": "Wave", "action": {"type": "animation", "actor": "speaker", "pose": "standing", "animation": "wave"}},
    {"label": "Note", "action": {"type": "text", "text": "Authored text."}}
  ]
}
```

There are no fixed numbers of scenes, actors, poses, animations, joints or layers. Each pose references its own rig. `pose` is the initial pose; a pose's optional `animation` is its initial clip. Without one, its rest pose displays. Animations referenced by a pose must validate against that pose's rig.

Background and music are optional; omit them or use null. A scene with no music stops the previous scene's music. Music decoding is provided by Qt Multimedia; WAV and common compressed formats depend on the installed Qt backend. Missing/unplayable music reports an error without preventing scene use. `volume` is 0–1. No audio is bundled by the engine.

Theme precedence is project → scene → selected actor's bubble settings. Actors can have different font files and bubble colors. Empty scenes and intro scenes with only text/buttons are valid. Built-in action types are only `scene`, `animation`, and `text`. Animation actions are checked against the declared actor/pose/clip manifest.

## Rig and coordinates

```json
{
  "format": "panel.rig",
  "version": 1,
  "joints": [
    {"id": "torso", "parent": null, "x": 200, "y": 300},
    {"id": "shoulder", "parent": "torso", "x": 65, "y": -100}
  ],
  "layers": [
    {"id": "body", "image": "../parts/body.png", "joint": "torso", "x": 0, "y": 0, "pivot": [120, 240], "z": 0},
    {"id": "arm", "image": "../parts/arm.png", "joint": "shoulder", "x": 0, "y": 0, "pivot": [25, 18], "z": 1}
  ]
}
```

Positive X is right, positive Y is down. Positive rotation is clockwise in degrees. A joint's transform is local to its parent. A layer's transform is local to its bound joint; null/missing binding means rig world coordinates. Layers are drawn by `z`; actor z is added to layer z. Images support transparency. A head layer bound to the torso is unaffected by shoulder rotation. Child joints intentionally inherit ancestor transformations.

Joint/layer transform channels are `x`, `y`, `rotation`, `sx`, `sy`, `opacity`. Defaults are 0, 0, 0, 1, 1, 1 respectively. Layer opacity multiplies inherited joint opacity; joint opacity cascades through its descendants and bound layers. `pivot: [x,y]` identifies the local image anchor. Matrix order is translation × rotation × scale × negative-pivot translation. Layer geometry is transformed as a unit; it is never used to deform another image. Parent cycles, duplicate IDs, invalid references and nonfinite transforms are rejected.

## Animation

```json
{
  "format": "panel.animation",
  "version": 1,
  "duration": 2.7,
  "loop": true,
  "tracks": [
    {"target": "joint:shoulder", "channel": "rotation", "keys": [
      {"time": 0, "value": 0, "ease": "smooth"},
      {"time": 0.25, "value": 12, "ease": "linear"},
      {"time": 1.73, "value": -8, "ease": "smooth"},
      {"time": 2.7, "value": 0}
    ]},
    {"target": "layer:arm", "channel": "opacity", "keys": [
      {"time": 0, "value": 1, "ease": "hold"},
      {"time": 2, "value": 0.8}
    ]}
  ]
}
```

All times are seconds. Duration must be positive. Each `(target, channel)` can have one track, and each track has independently chosen timestamps, strictly increasing from 0 through duration. There is no implicit FPS, equal spacing, three-second duration, or nine-second pose schedule. Track values replace the local rest values; untracked channels retain rest values.

Before the first key, its value is held; after the final key, its value is held. `ease` applies from its key to the next: `linear`, cubic smoothstep `smooth`, or `hold`. Rotation interpolates authored degree values directly, permitting explicit multiple revolutions. Looping uses time modulo duration; nonlooping clips clamp to their final value. Author matching first/last values for seamless loops. The editor can scrub the exact endpoint without wrapping.

## Inference wire events

The optional client consumes newline-delimited JSON events from the existing `/v1/sessions/{sid}/chat` API: `token`, `done`, `error`. It also recognizes this optional extension:

```json
{"type":"animation","actor":"speaker","pose":"standing","animation":"wave"}
```

Only animations already declared in the current scene can execute. Invalid commands are ignored. There is no executable text, file write, scene change or network action in a model animation command. The current service does not generate this extension by itself. Text generation, animation clocks and input admission are independent.
