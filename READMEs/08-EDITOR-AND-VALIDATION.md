# 08 — Editor workflow and validation

## Start a project

1. Run `start-editor.cmd` from the repository root.
2. Choose **New project** and select an empty folder. The editor creates a `.panel` entry point and an empty intro scene.
3. Use **Project & scenes** to edit JSON documents. This tab validates saved documents against the project; it is not a drag-and-drop scene-layout editor.
4. Add scene paths to the project's `scenes` map. Create the referenced `.scene` files before saving the project that registers them.
5. Add actor and pose declarations following [guide 03](03-CHARACTERS-AND-POSES.md). Prepare the referenced rigs and clips first, since project loading checks references.

You can also write files in any text editor. Keep the project/scene format in this README set as the source of truth; the engine does not invent fields from filenames or folder layout.

## Build the character rig

1. Choose **New rig**, then **Import image layer** for each existing body-part image. Importing stores references; it neither copies nor generates artwork.
2. Use **Rest / skeleton setup** while building the hierarchy. Click **Add joint**, then click its position on the canvas. Selecting an existing joint first makes it the new joint's parent.
3. Drag joints to position them. Select each layer and set its **Parent / bound joint**, pivot, local position, scale and drawing order. Changing binding can change its placement, so adjust local coordinates afterward.
4. Drag a layer to move it independently. Use the mouse wheel to zoom, drag empty space to pan, and **Fit canvas** to recenter.
5. Rename parts to stable descriptive IDs. Renaming updates the loaded clip, but other clips on disk also need the corresponding target-name changes.
6. Save the `.rig`. Saving to another directory rewrites layer image paths relative to the new rig file location.

To delete a joint, first reparent its child joints and unbind its layers. This prevents silent dangling references.

## Author animation

1. Choose **New clip** and set duration and loop behavior. This enables **Animation keys** mode.
2. Select a part, set the cursor time, then edit a transform channel or drag the part. Animation-mode edits add or replace keys; rest-mode edits change the underlying rig.
3. For a neutral start, set cursor time to zero and use **Key all transform channels** before changing later values. Otherwise a track's first key also supplies its value before that timestamp.
4. Use **Add / replace channel key** for one channel, or key all channels when useful. Every track has its own timestamps.
5. Choose interpolation for the segment after a key. At an existing key's timestamp, selecting interpolation updates that key for the selected target/channel.
6. Scrub by clicking the timeline, or drag key markers to arbitrary times. Delete a key using **Delete channel key at cursor**. Two keys in one track cannot occupy the same time.
7. **Play** previews the clip. **Save clip** writes the `.anim`. Rig and clip saves are separate; save both when both changed. Undo and redo cover edits.
8. Register the clip path in the correct scene actor/pose. Opening another clip does not automatically register it in the game's scene data.

## Validate and run

Run these commands from the repository root, replacing the project path:

```bat
Engine\.venv\Scripts\python.exe Engine\run.py path\to\project.panel --validate
start-engine.cmd path\to\project.panel
start-editor.cmd path\to\project.panel
```

Validation prints the capability manifest when successful. It checks document type/version, scene references, actor IDs, initial poses, rig hierarchy, image/background existence, clip targets, animation timing and declared button references. It is not a complete media-decoding or appearance check: open the player to verify fonts, actual image decoding, audio and layout.

To preview a particular scene or inspect CPU frame timing:

```bat
start-engine.cmd path\to\project.panel --scene room
Engine\.venv\Scripts\python.exe Engine\run.py path\to\project.panel --scene room --smoke-seconds 5
```

The smoke run exits after the interval and reports the actual renderer. Expect `GraphicsApi.Software`. Static scenes need not produce a continuous stream of frames. These commands do not launch the inference backend.

The existing [example project](../Engine/examples/project.panel) is runnable and reuses one existing flattened image. It demonstrates file wiring and timing, not a pre-separated articulated character.

## Common mistakes

| Symptom | Check |
|---|---|
| Missing layer image | Image path is relative to the `.rig`, not the scene |
| Missing rig or animation | Pose paths are relative to the `.scene`, not the character folder |
| Unknown default animation | Pose `animation` must be a key in its `animations` object |
| Invalid animation target | Target includes `joint:` or `layer:` and an exact rig ID |
| Joint hierarchy error | Parent exists and there are no cycles |
| Animation rejected | Duration is positive, keys are finite, unique, sorted, and within duration |
| Arm rotates around the wrong point | Pivot is the shoulder pixel in the arm image; binding is the shoulder joint |
| Head moves with the arm | Head is bound beneath the arm branch, or both are baked into one image |
| Pose jumps at the loop boundary | First and endpoint values do not match |
| Position jumps when keyed | Track values are absolute local coordinates, not offsets |
| Clip does not start on scene entry | Set the pose's default `animation`, not just its `animations` map |
| Prompt edits have no effect | Existing inference session retains its original system prompt |
| Text mentions an animation but nothing moves | Plain text is not an animation event |
| New story variable has no effect | Conditional state/variables are not implemented by this format |
| Custom font fails at runtime | Check the real font path and that Qt can load the file |

## Keep assets economical on the CPU

Use appropriately sized images, avoid huge transparent margins and unnecessary overlapping layers, and reuse image files across clips. A clip normally needs only key values, not a second set of image files. Use fewer keys when interpolation already describes the motion. Keep static parts untracked.

There is no required joint/layer count, but large canvases and many translucent pixels still cost CPU time. Measure your actual project on its target machine. The engine always uses CPU raster rendering; asset choices remain important even with smooth frame scheduling.
