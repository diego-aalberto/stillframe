# 05 — Animations

An `.anim` is a timed set of local transform values. The file owns its duration, looping behavior and keyframe timestamps. Different parts and channels can use different, uneven spacing.

## Complete clip for the rig in guide 04

```json
{
  "format": "panel.animation",
  "version": 1,
  "duration": 2.7,
  "loop": true,
  "tracks": [
    {
      "target": "joint:shoulder",
      "channel": "rotation",
      "keys": [
        {"time":0,"value":0,"ease":"smooth"},
        {"time":0.25,"value":12,"ease":"linear"},
        {"time":1.73,"value":-8,"ease":"smooth"},
        {"time":2.7,"value":0}
      ]
    },
    {
      "target": "joint:neck",
      "channel": "rotation",
      "keys": [
        {"time":0,"value":0,"ease":"smooth"},
        {"time":1.1,"value":3,"ease":"smooth"},
        {"time":2.7,"value":0}
      ]
    },
    {
      "target": "layer:forearm",
      "channel": "opacity",
      "keys": [
        {"time":0,"value":1,"ease":"hold"},
        {"time":1.8,"value":0.8,"ease":"linear"},
        {"time":2.7,"value":1}
      ]
    }
  ]
}
```

## Clip, track and key fields

| Location | Field | Expected value |
|---|---|---|
| Clip | `format`, `version` | `"panel.animation"`, `1` |
| Clip | `duration` | Required finite number greater than zero, in seconds |
| Clip | `loop` | Boolean; optional, defaults to `true` |
| Clip | `tracks` | Array; use `[]` for a motionless clip |
| Track | `target` | `joint:ID` or `layer:ID`, present in the pose's rig |
| Track | `channel` | `x`, `y`, `rotation`, `sx`, `sy`, or `opacity` |
| Track | `keys` | Array of timed values |
| Key | `time` | Finite number from 0 through clip duration |
| Key | `value` | Finite numeric value in that channel's units |
| Key | `ease` | `linear`, `smooth`, or `hold`; optional, defaults to `linear` |

Only one track may exist for a given target/channel pair. Key times in a track must be strictly increasing: duplicate times and unsorted keys are invalid. Different tracks can freely share timestamps. Fractions such as `0.125` or `1.73` are supported; there is no required frame rate or key interval.

## Local values, not offsets

A key replaces the object's local rest value for that channel. If a shoulder's rest X is `65`, an X key of `5` puts it at local X `5`; it does not move it five pixels from rest. Use `70` for that result.

Untracked channels retain the rig's rest values. Before a track's first key, the first key's value is held. After its last key, the last value is held until looping or stopping. An empty track leaves its channel at rest. A single key holds that value for the clip.

## Interpolation

`ease` belongs to the segment **starting at that key** and ending at the next one:

- `linear`: constant rate of value change.
- `smooth`: cubic smoothstep, slowing near the segment's start and end.
- `hold`: keep this key's value until the next timestamp, then jump.

The final key's easing has no following segment to affect. Rotations interpolate the authored numbers directly; a transition from 350 to 10 travels backward by 340 degrees. Use 350 to 370 when you want a forward 20-degree rotation.

## End and loop behavior

A nonlooping clip stops on its last evaluated pose at `duration`. A looping clip uses elapsed time modulo duration. At the loop boundary it returns directly to the beginning; the engine does not add an extra blend segment. Match first and endpoint values when you want a seamless loop.

Editor scrubbing can display the exact endpoint without wrapping. Player playback wraps at that endpoint when looping. The player samples elapsed time, so a late rendered frame does not change the animation's authored timing.

## Register and call the clip

Put the `.anim` path in the pose's `animations` map inside the scene. Paths there resolve from the scene file. Set the pose's `animation` to the registered ID for automatic playback on scene entry, or use an animation button/event.

Only one clip runs per actor at a time. Starting another clip replaces it and resets playback time. There is currently no clip blending, additive mixing, queue, automatic nine-second pose cycle or keyframe event track. Author longer clips if you need several phases within one animation.

Next: [Editor workflow](08-EDITOR-AND-VALIDATION.md).
