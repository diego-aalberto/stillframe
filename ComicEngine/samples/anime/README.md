# Hana: three poses, nine animation tests

Run **`../../start-anime-test.cmd`** to open the hardcoded looping test. It plays the following 27-second sequence forever:

| Seconds | Pose | Animation |
|---|---|---|
| 0–3 | Standing | Gentle head sway |
| 3–6 | Standing | Small hand wave |
| 6–9 | Standing | Breathing and body sway |
| 9–12 | Seated | Alternating leg swings |
| 12–15 | Seated | Small head nod |
| 15–18 | Seated | Ankle/foot bounce |
| 18–21 | Kneeling | Curious head tilt |
| 21–24 | Kneeling | Open-palm gesture |
| 24–27 | Kneeling | Gentle bow |

Each pose stays on screen for **nine seconds**, and each animation lasts **three seconds**. At 27 seconds the sequence returns to standing/head sway. Space pauses or resumes, Home restarts, and 1–9 jumps to a particular animation. Pausing or minimizing pauses the demo clock. Normal active playback uses elapsed wall time so slow frames do not stretch the three-second slots.

The character artwork is three original transparent PNGs. The standing drawing was used as the identity reference for the seated and kneeling drawings, preserving the amber eyes, chestnut bob, gold triangular clip, ivory blouse, teal ribbon, navy skirt, charcoal tights and brown boots. The drawings are generated variants, so tiny line/proportion differences remain; they are not pixel-identical redraws. The scene uses image scales chosen to keep her head size similar between poses.

The built-in image-generation tool created the artwork; the exact prompts are in [PROMPTS.md](PROMPTS.md). Original PNGs are retained here. Scene generation imports them at 512 × 768 with alpha-aware downsampling, then packs all artwork into `../anime-test.cscene`. There are **no animation-frame images**: each pose has its own 21-joint rig, and each of its three clips contains 25 joint-point keys. The runtime skins the image mesh using the existing engine. Every clip starts and ends at the same resting pose. The bench is a separate static background, so leg movement does not bend it.

The hardcoded rig positions, nine motion definitions and sequence logic are in `../../src/anime_demo.cpp`. The test uses a 40 × 40 mesh per image (1,681 vertices), with only one pose visible at a time. The 12 fps held-pose style remains enabled.

To edit the scene, run **`../../edit-anime-test.cmd`**. In Studio, uncheck Visible on the current image, select the desired pose under Image layer, and check Visible on that image. Select its animation and play or scrub it. Use Save as to keep the supplied test intact. The timed player expects the three pose layers and their nine clip names/durations to remain in the original order. The scene itself remains a normal, self-contained `.cscene`; only the automatic sequencing is hardcoded in the player.

Rebuild the scene from the PNGs after changing rig/motion code:

```powershell
./build.ps1
$p = Start-Process ./bin/ComicPlayer.exe -ArgumentList '--make-anime-demo' -WindowStyle Hidden -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Scene generation failed' }
./test-anime.ps1
```

Run those commands from the `ComicEngine` directory. Scene generation replaces `samples/anime-test.cscene`; it does not modify the original PNGs. `test-anime.ps1` renders all nine motions in hidden windows and checks the timing/visibility log. Core tests additionally verify the exact 3-, 9-, and 27-second boundaries, actual vertex displacement for every clip, matching start/end poses, and save/reload behavior. Inference is not connected.
