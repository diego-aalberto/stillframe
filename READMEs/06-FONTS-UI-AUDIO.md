# 06 — Fonts, interface and audio

## Theme placement and precedence

Put defaults in the project's `theme`, scene overrides in a scene's `theme`, and character overrides in its `bubble` object. The current player merges them in that order: project → scene → selected actor. The first actor is initially selected.

There is one shared dialogue area. Selecting another character changes its theme; the engine does not draw a separate positioned bubble over every character. The merged theme also supplies the window font and authored scene-button styling, so actor overrides can affect those shared elements.

```json
{
  "font_family": "Segoe UI",
  "font_size": 18,
  "background": "#20232b",
  "text": "#ffffff",
  "bubble_fill": "#ee202938",
  "bubble_edge": "#aebbd0",
  "bubble_border": 2,
  "bubble_radius": 12,
  "button_fill": "#3d4860",
  "button_hover": "#626d87",
  "button_pressed": "#46506a",
  "button_text": "#ffffff",
  "button_edge": "#9aa8c0",
  "button_radius": 6
}
```

This is a theme object to insert into the relevant document, not a standalone engine file.

## Supported theme fields

| Field | Purpose | Default |
|---|---|---|
| `font` | Custom font file path, loaded at runtime | None |
| `font_family` | Installed font family if no file is supplied at that level | `Sans Serif` |
| `font_size` | Font pixel size | `18` |
| `background` | Window/stage backing color | `#20232b` |
| `text` | Dialogue text color | `#ffffff` |
| `bubble_fill` | Dialogue panel fill | `#ee202938` |
| `bubble_edge` | Dialogue panel border | `#aebbd0` |
| `bubble_border` | Border width | `2` |
| `bubble_radius` | Corner radius | `12` |
| `button_fill` | Authored scene button normal fill | `#3d4860` |
| `button_hover` | Authored scene button hover fill | `#626d87` |
| `button_pressed` | Authored scene button pressed fill | `#46506a` |
| `button_text` | Authored scene button label color | `white` |
| `button_edge` | Authored scene button border color | `#9aa8c0` |
| `button_radius` | Authored scene button corner radius | `6` |

Colors accept named colors, `#RRGGBB`, and `#AARRGGBB`. In the eight-digit form alpha comes **first**, not last. Numeric sizes/radii should be nonnegative; use a positive font size.

The listed button colors currently apply to buttons authored in `scene.buttons`. The Send button, message field and character selector still use the Qt control style. There are no per-button color overrides, bubble-image fields, arbitrary UI layouts, or author-configurable control labels in this version.

## Custom fonts

Use a font file supported by Qt, commonly `.ttf` or `.otf`. A project theme's font path is relative to the `.panel`; a scene theme or character bubble font path is relative to the `.scene`.

Example character override inside `scenes/room.scene`:

```json
{
  "font": "../characters/speaker/fonts/dialogue.ttf",
  "font_size": 20,
  "bubble_fill": "#ed2b3045",
  "bubble_edge": "#dab982"
}
```

Use this as the actor's `bubble` object. Supply the real file; missing/unreadable font files fail when loaded. `font_family` alone depends on fonts installed on the player's machine. A valid `font` at a theme level resolves to the loaded family and takes precedence over `font_family` at that same level.

## Background images

Set scene `background` to an image path or null. Backgrounds fill the stage while preserving aspect ratio and may be cropped at the edges. Omit the image to use the theme background color. A missing referenced background is a project-load error. There is no background animation track, parallax layer definition or per-scene crop configuration.

## Optional music

```json
{
  "path": "../music/room.ogg",
  "volume": 0.4,
  "loop": true
}
```

Use this as a scene's `music` object. `path` is relative to the scene. `volume` is 0–1 and defaults to 0.5; `loop` defaults to true. Decoding uses Qt Multimedia, so usable compressed formats depend on its backend. A supported PCM WAV is a straightforward authoring option.

Omit `music`, use null, or use an empty object for silence. Scene entry stops the previous music. A new scene with a music path starts its track; there is no crossfade or automatic continuation across scenes. Missing/unplayable music reports a playback error while the scene remains usable. No music is required to run a game.

There is currently one scene music player, without a separate sound-effects or voice-over track system.
