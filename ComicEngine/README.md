# Comic Engine

**Connected story game:** run `../start-game.cmd`. `bin/StoryGame.exe` uses this engine's renderer and scene model with the local inference service and the authored story in `../Game`. See [the game guide](../Game/README.md). The standalone Player and Studio described below remain independent of inference.

Two standalone C++17 Windows applications for image-based, skeletal animated comics:

- **Comic Player** loads a scene and plays its characters and dialogue.
- **Comic Studio** edits scenes, image rigs, pose keyframes, dialogue, fonts, and speech bubbles, with a live preview.

Run **`start-editor.cmd`** or **`start-player.cmd`**. The executables are in `bin/`. Both open `samples/midnight.cscene` by default. Pass a different `.cscene` path to either launcher to open it. No Python, game engine, downloaded runtime libraries, network service, or inference backend is needed to run them.

The standalone Player and Studio make no network requests. StoryGame connects to the local story director in `../Game`, which manages inference sessions.

**Anime animation test:** run `start-anime-test.cmd` for the generated character's standing, seated and kneeling poses, with three skeletal animations per pose. The hardcoded sequence runs each animation for three seconds, each pose for nine seconds, and repeats after 27 seconds. See [the test guide](samples/anime/README.md); `edit-anime-test.cmd` opens its scene in Studio.

## Editing a scene

The included original robot scene contains `breathe`, `wave`, and `think` clips. Select a clip in Studio and press **Play** to preview it. Drag the slider below the canvas to scrub. The bottom of the preview lists the current time and keyframe times.

1. Use **File → New empty scene** to start fresh, or **Open** to edit an existing scene. Use **Save as** to keep the original sample intact.
2. Use **Assets → Import background** to embed a PNG, JPEG, BMP, or CIMG background. Backgrounds draw in import order, scaled to the scene canvas. PNG transparency is retained.
3. Use **+ Image** beside Character to create a character from an image. **+ Layer** adds another independently rigged image to the selected character. Visible layers draw in list order. Separate transparent images for body parts give better results when limbs overlap.
4. Each new image starts with root, spine, and head joints. Enable **Edit bind rig**, select a parent joint, and use **+ Joint** to add a child. Drag the joint markers onto the artwork. The mesh weights rebuild automatically. Bind edits adjust the same joint in existing keys by the bind-position delta.
5. Turn **Edit bind rig** off. Add or select a clip, pause, and scrub to the desired time. Drag a joint to pose the image. Its descendants move with it. Press **Set key** to store the pose. Repeat at other times, then play the clip.
6. **Delete key** removes a key within 1/24 second of the playhead. The duration field and **Apply time** scale existing key times proportionally. The **Loop** checkbox is applied with that button. Match the first and last poses for a seamless loop.
7. Edit character/image/clip IDs and press **Apply names**. IDs are unique within their scope and become the future animation command names.
8. Change the character position, image scale, or bubble width and offset, then **Apply placement & bubble size**. Enter dialogue and **Apply text**. Empty text hides the bubble. Choose the character font and bubble fill, edge, and text colors using their buttons.
9. **Save** writes one self-contained `.cscene`. A pending dragged pose is stored as a key automatically on save. Other inspector fields use their explicit Apply buttons. Switching away from a pending pose asks whether to keep it; Escape discards it.

The inspector scrolls on smaller displays. Skeleton and mesh overlays are editor-only. **Visible** can hide an image layer without deleting its rig or clips. Remove images or characters through the Assets menu. Images and fonts can also be exported individually in the custom formats.

### Controls

| Action | Player | Studio (canvas focused) |
|---|---|---|
| Pause / resume | Space | Space or Play/Pause button |
| Restart | Home | Home or Restart button |
| Trigger clip | 1–9 selects that clip index on every layer that has it | Animation dropdown |
| Step time | Left / Right, 1/12 second | Left / Right or timeline |
| Save / open | — | Ctrl+S / Ctrl+O |
| Discard pending pose | — | Escape |

The **12 fps poses** checkbox holds each sampled pose for a comic-like cadence. Disable it for smoothly interpolated motion. The renderer and input remain independent of this pose rate. Studio uses the selected clip's timeline to preview the scene; Player samples each layer's own loop/hold behavior against elapsed time.

## Architecture and performance

The platform layer uses Win32 for windows/input/dialogs and Windows Imaging Component only for importing source images. Rendering uses OpenGL's fixed-function textured triangles and client vertex arrays. There is no shader compiler, scripting VM, physics engine, or general-purpose game framework. It is a small 2D scene window and C++ engine, not a Python/Pygame API compatibility layer.

At load/rig-edit time, each image gets a regular triangulated grid. Each vertex receives normalized weights for its two nearest bone segments. Every pose is a list of absolute joint points in **image-local pixel coordinates**. The hierarchy defines bones and editor subtree movement. Playback linearly interpolates those points, optionally quantizes time, derives a transform per bone, and blends two transforms per vertex. Bone rotation and longitudinal scaling follow the points while preserving perpendicular thickness.

- Default image grid: 16 × 16 cells, 289 vertices, 512 triangles. The sample uses 24 × 24 cells, 625 vertices, 1,152 triangles.
- Weights, UVs, indices, and textures are reused; there is no image decoding or weight calculation during playback.
- Unreferenced images and fonts are discarded on editor load/save so removed artwork does not inflate saved scenes.
- Scene images and font atlases use raw RGBA8. This favors straightforward loading over small files; transparent PNG artwork becomes uncompressed inside the scene.
- Text uses an embedded bitmap atlas. Speech bubbles are drawn geometry with configurable colors, width, and offset.
- Rendering stops while minimized; paused previews redraw in response to changes. Animated playback requests frames at approximately 60 Hz.
- Old OpenGL implementations that reject non-power-of-two textures get a one-time power-of-two resampling fallback. Very old texture limits can reduce image quality.

`ComicTests.exe` reports a CPU-only pose sampling + skinning benchmark. The final release build measured approximately **0.006 ms/update** on this development machine for the sample (10,000 updates in 60.5 ms). This is **not** a low-end-machine, end-to-end frame-rate, or load-time guarantee. Texture size, visible layers, graphics driver, and fill rate still matter. Prefer small source images and modest grids on weak hardware.

## Custom data

| Extension | Magic | Contents |
|---|---|---|
| `.cscene` | `CSCN` | Canvas settings, embedded image/font arrays, backgrounds, characters, per-image rigs and clips, dialogue and bubble styles |
| `.cimg` | `CIMG` | Width, height, top-down RGBA8 pixels |
| `.cfont` | `CFNT` | Font name, layout metrics, 224 glyph advances and an RGBA8 atlas |

All use versioned, explicitly little-endian binary serialization. They do not serialize native C++ struct layouts or pointers. A scene has no external asset paths. See [FORMAT.md](FORMAT.md) for the complete wire layout and coordinate rules.

Fonts support U+0020–U+00FF, including common Western European accented letters. Unsupported characters display `?`; complex-script shaping, emoji, and CJK are not implemented. Font selection rasterizes the selected installed font into the custom atlas at a fixed 24-pixel source size; playback uses the embedded result. Bubble text wraps within its width and displays up to ten lines. Longer dialogue remains stored but should be split into shorter messages for presentation.

The current editor uses automatic weights. It does not include weight painting, a mesh-topology editor, inverse kinematics, undo history, joint deletion/reparenting, or character/image list reordering. Layer offsets and grid density are available through the C++ scene model; Studio exposes character position and image scale. Moving crossing limbs in one flattened image can bend neighboring artwork; independent image layers help. Saves replace the destination through a temporary file after validation.

## Scene command boundary

`core.hpp` exposes `play(scene, characterId, layerId, clipName)` and `say(scene, characterId, text)`. `play` selects a clip; the host controls elapsed time and resets its playback clock when appropriate. `manifest(scene)` returns JSON containing character IDs, layer IDs, and clip names. Studio can export this using **File → Export capability manifest**. StoryGame uses this scene model and animation API; its authenticated loopback bridge and model calls live in the game integration.

For example, a future host can call:

```cpp
auto scene = comic::loadScene("scene.cscene");
comic::say(scene, "atlas", "I found something.");
if (comic::play(scene, "atlas", "body", "wave")) {
    elapsedSeconds = 0;
}
// Each frame: sample(layer, elapsedSeconds, scene.poseRate), then deform(layer, pose).
```

## Build and test

On Windows with MinGW-w64 available, run:

```powershell
./build.ps1
```

The script uses `g++` on PATH or `C:\msys64\mingw64\bin\g++.exe`, builds both applications and the core tests, runs the tests, and generates the sample only if it is missing. The executables statically link the C++ runtime and depend on Windows system DLLs. `./build.ps1 -Debug` builds with debugging symbols. No package downloads occur.

Alternatively, with CMake and a C++17 compiler:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake builds the platform-independent core and tests on other platforms, but the two GUI applications currently target Windows. The provided binaries were built and tested with MinGW-w64; the CMake/Visual Studio path has not been verified here.

Run `./test-smoke.ps1` after building for hidden-window graphics and editor checks. It creates render snapshots and a saved/reloaded test scene under `test-output/`, without changing the sample. Core tests cover bind identity, translated/rotated/stretched bones, interpolation, looping, held poses, asset/scene roundtrips, invalid hierarchies/references, malformed files, and the named animation/dialogue API.

Source map:

- `src/core.*`: data model, serializer, validation, mesh generation, skinning, command helpers.
- `src/platform.*`: Windows image import, font rasterization, file dialogs.
- `src/render.*`: texture upload, triangle rendering, fonts and bubbles.
- `src/app.cpp`: player/editor entry points, input, inspector, timeline and pose editing.
- `src/demo.cpp`: original procedural sample artwork and clips.
- `tests/core_tests.cpp`: behavior, serialization, malformed-input tests and CPU benchmark.
