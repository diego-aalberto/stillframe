# Comic binary formats, version 1

This document specifies the byte order implemented in `src/core.cpp`. Every file starts with a four-byte ASCII magic followed by `u32(1)`. There is no alignment padding, checksum, compressed stream, or native struct serialization. Unknown versions, incorrect magic, truncated input, and trailing bytes are rejected.

## Primitives

| Type | Encoding |
|---|---|
| `u32` | Four-byte unsigned little-endian integer |
| `f32` | Four-byte IEEE-754 float, little-endian; must be finite and absolute value below 1,000,000 |
| `bool` | `u32`, strictly 0 or 1 |
| `string` | `u32` byte length followed by that many UTF-8 bytes; no null terminator; max 65,536 bytes |
| `vec2` | `f32 x`, `f32 y` |
| `color` | Four bytes in R, G, B, A order; straight alpha |
| `array<T>` | `u32 count`, then that many `T` records |

Names must be nonempty. Character IDs are scene-unique; layer IDs are unique per character; animation names are unique per layer. The parser bounds sizes but does not perform strict UTF-8 normalization/validation.

## Image record and CIMG

```
Image := u32 width, u32 height, byte[width * height * 4] rgba
CIMG  := "CIMG", u32 version, Image
```

Pixels run left to right, then top to bottom. Dimensions are 1–4096 inclusive. No row padding. Color space is treated as display RGB; embedded ICC profiles and EXIF orientation are not preserved. Use already-oriented artwork.

## Font record and CFNT

```
Font := string name
        f32 cellWidth, f32 cellHeight, f32 lineHeight
        f32 advance[224]
        Image atlas
CFNT := "CFNT", u32 version, Font
```

Glyph `i` represents Unicode code point `i + 32`. Cells form 16 columns and 14 rows. The cell begins at `(i % 16 * cellWidth, i / 16 * cellHeight)`. All metrics are atlas pixels. Atlas RGB is white and alpha is coverage for Studio-generated fonts, allowing runtime tinting. Cell dimensions and line height must be positive; the full grid must fit inside the atlas. Each advance is between zero and cellWidth. Unsupported characters use the `?` cell. Newlines are handled by layout.

## CSCN scene

```
"CSCN", u32 version
string title
u32 canvasWidth, u32 canvasHeight
f32 poseRate
color clearColor
array<Image> images
array<Font> fonts
array<Background> backgrounds
array<Character> characters
```

Canvas dimensions are 64–8192. `poseRate = 0` means continuous interpolation; 0 < poseRate <= 120 means held poses sampled at that many steps per second. Image and font references below are zero-based indices into the embedded arrays. No extra CIMG/CFNT header is included inside a scene.

```
Background := u32 imageIndex, vec2 position, vec2 size

Character := string id, string dialogue
             vec2 position
             u32 fontIndex
             color bubbleFill, color bubbleEdge, color bubbleText
             f32 bubbleWidth
             vec2 bubbleOffset
             array<Layer> layers

Layer := string id
         u32 imageIndex
         bool visible
         vec2 offset
         f32 scale
         u32 gridCells
         u32 activeClipIndex
         array<Joint> joints
         array<Clip> clips

Joint := string name
         u32 parentPlusOne
         vec2 bindPoint

Clip := string name
        f32 durationSeconds
        bool loop
        u32 keyCount
        Key[keyCount] keys

Key := f32 timeSeconds
       vec2 points[jointCount]
```

Key point counts are implicit from the containing layer's joint count. A joint's stored parent is the in-memory parent index plus one. The root has `parentPlusOne = 0`. Only joint zero can be a root. Every other parent must precede its child, which prevents cycles. All key times must be strictly increasing and inside `[0, duration]`. Empty clips use the bind pose. One-key clips hold that pose.

Coordinates use X right, Y down. Joint bind/key points are absolute **image-local** coordinates, not offsets from the parent. Character position and layer offset are in scene pixels. A deformed vertex maps to scene space as:

```
scenePoint = character.position + layer.offset + layer.scale * deformedImagePoint
bubbleTopLeft = character.position + bubble.offset
```

Backgrounds draw first in array order. Characters and their visible layers follow in array order. All bubbles draw after character artwork. The scene canvas is letterboxed when the window changes aspect ratio.

## Derived mesh and animation semantics

Meshes are rebuilt from the image dimensions, bind joints and `gridCells`; they are not stored. A grid has `(gridCells + 1)^2` vertices and `gridCells^2 * 2` triangles. Vertex UVs remain attached to the original image. For each vertex, choose the two closest parent-to-child bone segments, by point-to-segment distance. Their unnormalized weights are `1 / (distance + 4)^2`, then normalized. A one-bone rig uses that bone alone; a root-only rig translates the image.

Looping clips wrap time by duration. Non-looping clips clamp at duration. When poseRate is nonzero, wrapped/clamped time is quantized down to a pose-rate tick (except a non-looping terminal time). Before the first key or after the last key, the nearest endpoint pose is held. Otherwise joint points interpolate linearly between adjacent keys.

Each bone maps its bind segment onto the posed segment, scaling along the segment and preserving perpendicular thickness. Each vertex blends the two transformed bind positions. A collapsed posed segment remains finite; a zero-length bind segment falls back to translation. This is linear blend skinning of a flat image, not frame swapping, 3D skinning, or inverse kinematics.

## Limits

| Item | Limit |
|---|---|
| Input or output file | 256 MiB |
| Images / fonts / backgrounds / characters | 128 / 32 / 32 / 32 |
| Layers per character | 1–64 |
| Joints per layer | 1–64 |
| Clips per layer | 1–128 |
| Keys per clip | 0–4,096 |
| Grid cells per side | 2–64 |
| Total derived vertices across the scene | 1,000,000 |
| Clip duration | Greater than 0, at most 3,600 seconds |
| Layer scale | Greater than 0, at most 100 |
| Bubble width | 80–4,096 scene pixels |

These are validation ceilings, not recommended scene sizes. Keep texture resolution and layer count much lower for weak hardware. A scene is fully loaded in memory; there is no streaming loader.
