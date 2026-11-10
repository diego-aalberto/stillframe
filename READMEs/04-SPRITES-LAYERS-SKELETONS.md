# 04 — Sprites, layers and skeletons

A sprite pose is a `.rig` containing joints and independent image layers. The skeleton supplies transforms. Each layer follows one joint, or the rig's world coordinates if unbound. There is no mesh deformation or automatic body-part extraction.

## Artwork to supply

Use separate transparent images for parts that must move independently: body, head, upper arm, forearm, hand, thigh, lower leg, hair sections, accessories, or any other useful division. PNG with transparency is a straightforward choice. The editor also accepts WebP, JPG and BMP; opaque image formats are less useful for overlapping parts.

Keep the same character design, palette and scale across poses. Draw enough overlap beneath adjoining parts to hide gaps when they rotate. Crop unnecessary transparent margins to reduce CPU work, then set the pivot in that cropped image's own pixel coordinates. Image dimensions may differ between layers.

The engine expects each layer's `image` to be a file path to a whole image. It does not interpret atlas rectangles, layered PSD files, animation frame strips or embedded image bundles. A flattened existing sprite can be loaded as one layer, but cannot then move its arm independently of its head.

## Complete rig example

Save as `characters/speaker/standing.rig`; image paths below are relative to that file:

```json
{
  "format": "panel.rig",
  "version": 1,
  "joints": [
    {"id":"root","parent":null,"x":200,"y":300},
    {"id":"neck","parent":"root","x":0,"y":-130},
    {"id":"shoulder","parent":"root","x":65,"y":-100},
    {"id":"elbow","parent":"shoulder","x":0,"y":90}
  ],
  "layers": [
    {"id":"body","image":"images/standing-body.png","joint":"root","x":0,"y":0,"pivot":[120,240],"z":0},
    {"id":"head","image":"images/standing-head.png","joint":"neck","x":0,"y":0,"pivot":[70,110],"z":2},
    {"id":"upper_arm","image":"images/standing-arm.png","joint":"shoulder","x":0,"y":0,"pivot":[25,18],"z":1},
    {"id":"forearm","image":"images/standing-forearm.png","joint":"elbow","x":0,"y":0,"pivot":[22,12],"z":3}
  ]
}
```

Always include `joints` and `layers`, even when either array is empty.

## Joint fields

Each joint needs a unique `id`. `parent` is another joint ID or `null`; omitted parent also means world/root. Joint order in the file need not match hierarchy order. Multiple root joints are allowed. A joint cannot parent itself, form a cycle, or reference a missing parent.

A joint's X/Y, rotation and scale are relative to its parent. Rotating `shoulder` moves `upper_arm`, `elbow` and `forearm`. It does not affect `neck` or `head`, which belong to another branch. Rotating `root` intentionally affects all its descendants.

## Layer fields

| Field | Meaning | Required / default |
|---|---|---|
| `id` | Unique layer ID | Required |
| `image` | Image path relative to the rig | Required |
| `joint` | Bound joint ID, or `null` | Optional; unbound |
| `pivot` | Image anchor in source-image pixels | Optional; `[0,0]` |
| `x`, `y` | Local placement of that anchor | Optional; `0`, `0` |
| `rotation` | Local clockwise angle in degrees | Optional; `0` |
| `sx`, `sy` | Local scale factors | Optional; `1`, `1` |
| `opacity` | Local alpha multiplier | Optional; `1`; author in the range 0–1 |
| `z` | Drawing order offset | Optional; `0` |

Joint IDs and layer IDs each have their own namespace. An animation distinguishes `joint:head` from `layer:head`.

## Coordinates and pivots

Positive X points right, positive Y points down. Rotation is clockwise in degrees. Parent scaling affects child offsets. Image pivots are measured from the source image's top-left corner, before scaling.

For example, an arm image with its shoulder at pixel `[25,18]` uses `pivot: [25,18]`. With layer X/Y both zero, that pixel sits at the shoulder joint. Rotating the joint now swings the arm around the shoulder rather than the image corner.

The local transform order is translation × rotation × scale × negative-pivot translation. The parent joint transform is applied outside that, followed by the actor's scene transform. An unbound layer still follows the actor's scene transform.

Joint opacity multiplies through descendants; layer opacity multiplies that inherited alpha. Higher layer `z` draws above lower `z`. Actor z is added to layer z; layers from different actors can therefore interleave. Give actor z ranges sufficient separation when that is undesirable. Joint z has no drawing-order effect.

## What can animate

Tracks can animate `x`, `y`, `rotation`, `sx`, `sy`, and `opacity` on joints or layers. Pivot, parent binding, image path and drawing order are setup properties, not animation channels. To change artwork, select another declared pose.

There is no inverse kinematics, weighted multi-joint skinning, elastic deformation or automatic limb masking. Each layer is rigid. Use more layers/joints when you need more articulated parts.

Next: [Animation files](05-ANIMATIONS.md).
