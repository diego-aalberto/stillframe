# Panel engine and editor

For the authoring guide organized by topic, see [the READMEs folder](../READMEs/README.md). It covers the expected files for each character and for a complete game, including current format limitations.

Python owns formats, scene state, animation evaluation, editing and inference communication. PySide6/Qt provides CPU raster drawing, windowing, fonts and optional audio playback. The engine has no story imports, named characters, relationship rules, fixed pose counts, or fixed animation times.

## Run

From the repository root:

```bat
setup-engine.cmd
start-engine.cmd
start-editor.cmd
```

The environment is already installed in this checkout. Both programs work without inference. `start-engine.cmd path\project.panel` opens a project directly. `--scene ID` starts at a named scene for authoring. No renderer selection exists: software is mandatory, even when an inherited environment requests a GPU backend.

The editor uses standard raster widgets; the player uses the Qt Quick software scenegraph. Window composition by the OS is outside the renderer. No engine GPU work is submitted. No compute or rendering GPU is selected or enumerated.

## Create and animate a layered sprite

1. Open the editor and choose **New rig**. Import each existing transparent body-part image with **Import image layer**. Importing references the file; it does not generate, cut out or modify artwork.
2. Select **Rest / skeleton setup**. Click **Add joint**, then click the canvas. Select a joint first to make it the new joint's parent. Add as many joints and layers as the project needs. Wheel zooms, empty-space dragging pans, and **Fit canvas** recenters the sprite.
3. Select a layer and set **Parent / bound joint**. Set its local X/Y, pivot, rotation, scale and layer order. Select a joint to set its parent or drag its position. Layer positions are relative to the bound joint, so changing binding may require adjusting the local placement. Dragging a layer moves that layer independently.
4. Save the `.rig`. **New clip** switches to **Animation keys**. Set the clip duration and looping behavior; neither is chosen by the engine.
5. Choose a joint or layer, set the cursor time in seconds, then change its transform. In animation mode each edit creates/replaces a key on that channel. **Key all transform channels** records the current values; **Add / replace channel key** records only the selected channel.
6. Use arbitrary times such as `0`, `0.25`, `1.73`, `2.7`. Each channel has its own timeline row. Drag a key marker to retime it. Click empty timeline space to scrub. Choose linear, smooth or hold interpolation and apply it to a key at the cursor. Delete the selected channel's key using the delete-key button.
7. **Play** previews using the same evaluation code as the player. Save the `.anim` independently of the rig. Undo and redo cover rig and animation edits. Joint deletion requires unbinding layers and reparenting children first.
8. **New project** creates an empty project in an empty folder. The **Project & scenes** tab edits and validates project/scene documents. This tab is a JSON document editor, not a visual scene-layout designer. Declare each actor's poses, rig paths and animation paths there. **Open project** lets you choose the pose to edit.

Rest/setup changes modify the underlying rig; animation changes write channel keys. Tracks use absolute local values, not additive offsets. Layers are rigid independent images; mesh deformation, automatic body-part segmentation, IK and paint tools are not implemented.

## CPU behavior

Images are cached and loaded asynchronously. Animation uses elapsed wall time rather than accumulating a fixed step; missing a frame never slows the clip's clock. Static scenes do not run the animation loop. Unchanged layer properties do not trigger drawing updates. Streamed text is batched at 33 ms; completion bypasses that batch and unlocks input immediately. No scene file parsing, inference HTTP calls or image loading is performed in the animation evaluator.

The initial CPU fixture measured a 16.7 ms 95th-percentile update interval on this machine. That is an observation for one sprite, not a promise for every low-end CPU. Large transparent images, many overlapping layers and large windows increase raster cost. Crop authored layers to their visible bounds, keep source resolution close to display size, and reuse images/rigs between clips.

## Optional inference

Set the project's `inference.url` to the existing service and start that service separately. Each scene/actor gets an isolated session using its startup prompt and authored history. `session_store` optionally preserves session IDs across launches; the service owns actual conversation history. Credentials come from the named environment variable, not project files.

The player sends one streaming generation request per message. It does not run the old second director request. It accepts optional declared animation events from a compatible stream, and exposes a capability manifest through `Project.capabilities()`. The current inference service emits text events; adding model-directed animation events is a separate integration task. The service lock is released after the durable response commit and before yielding `done`. The UI unlocks on `done`, not socket teardown, animation completion, or text effects.

## Verification

```bat
Engine\.venv\Scripts\python.exe Engine\run.py Engine\examples\project.panel --validate
Engine\.venv\Scripts\python.exe -m pytest Engine\tests -q
Engine\.venv\Scripts\python.exe Engine\run.py Engine\examples\project.panel --scene stage --smoke-seconds 5
```

The smoke command prints the actual renderer and frame intervals. `GraphicsApi.Software` is expected. `--capture path.png` saves the window for inspection. The GUI dependency is pinned in requirements.txt. Tests additionally require pytest. Qt/PySide6 licensing applies when distributing the framework; see its installed license files.
