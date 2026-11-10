# Panel — Python CPU comic engine

**Developer handoff:** read [CLAUDE_HANDOFF.md](CLAUDE_HANDOFF.md) for the current architecture, legacy-code distinctions, setup, limitations and root-folder transfer notes. See [HANDOFF_VERIFICATION.md](HANDOFF_VERIFICATION.md) for checks rerun for the handoff.

**Authoring documentation:** start with [READMEs/README.md](READMEs/README.md) for separate guides to game structure, scenes/story, characters, layered sprites, skeletons, animation timing, UI, audio and inference.

The current player and animation editor live in **Engine/**. All engine drawing uses Qt's software raster backend. There is no Vulkan, OpenGL, CUDA, or integrated-GPU rendering path in the new engine. Inference is a separate optional service and may use the dedicated GPU.

- **start-engine.cmd** opens a `.panel` project chooser. Supply a project path to open it directly.
- **start-editor.cmd** opens the layer/skeleton/timeline editor.
- **setup-engine.cmd** installs the pinned Python UI dependency when setting up another checkout.
- **start-game.cmd** launches The Light We Leave On through the new CPU engine and starts or reuses its local inference service.

Try the existing-asset fixture with:

```bat
start-engine.cmd Engine\examples\project.panel
start-editor.cmd Engine\examples\project.panel
```

No new artwork was generated. The fixture reuses one existing flattened image as one layer. Independent arm/leg/head movement requires separately authored image layers; the engine does not pretend that a flattened image is layered.

See **[Engine/README.md](Engine/README.md)** for editing and running, and **[Engine/FORMAT.md](Engine/FORMAT.md)** for the custom project, scene, rig and animation formats.

The story is ported in `Game/panel/`, with `Game/panel_player.py` connecting the new engine to the existing story, saves, character memory and inference logic. See [Game/README.md](Game/README.md). `Story/` retains the story guide; C++ sources/binaries in `ComicEngine/` and the old `app/` / `app.zip` export are retained legacy material. The old portable export still contains the old game and must not be used to launch this rewrite. Its inference source copy has received the lock fix, but the old archive has not been repackaged.

The inference service remains in **Inference/**. The game launcher starts it automatically; generic projects can start it separately with its backend launcher. Existing model weights and saved story conversations are preserved.
