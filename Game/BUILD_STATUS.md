# Playable build — 14 October 2026

Launch from the repository's start-game.cmd. The native StoryGame executable is rebuilt.

Implemented:
- 35 authored story nodes plus a reusable repair scene, four individual romances and the mutually agreed all-four ending.
- One local inference process, separate persistent speech/director sessions per woman, durable personal facts, witnessed journals and unresolved emotional incidents. Hurt commits without Continue and survives scene changes and restarts.
- 12 illustrated summer poses per woman, with ten skeletal gestures per pose: 120 motion variants per woman, 480 total.
- Ten Hana-style painted backgrounds. All assets are stored under Game/assets; no placeholder cast or location art is used.
- Inference-selected poses; continuously looping gestures chosen after each new player message from all ten gestures plus a duplicate of the current gesture. Retries retain the selection.
- Rounded pastel interface, individual mint/peach/lilac/blue speech cards, animated reply dots and card entrances, live token streaming, start/resume menu, scene introductions, history, visits, autosave and interrupted-response recovery.
- Game/music lists 36 expected PCM WAV files. Supplied tracks loop per scene; missing tracks remain silent.
- Kisses and paired cuddles conveyed in prose; individual affectionate and cuddle poses are included.
- Updated Story/story-map.tex, successfully compiled in the built-in LaTeX editor.
- Half-speed motion, translucent main dialogue card, bold quoted speech, growing multiline composer, named new saves and scene hints. Romantic prompting encourages reciprocation without removing relationship gates.

Verification:
- 59 combined inference/game regressions passed, including emotional continuity, private memories, weighted animation selection, two-phase retry recovery, named saves, hints, commitment-scene handling and stop-sequence forwarding.
- Engine build and core animation/serialization tests passed.
- Native smoke checks passed across all ten locations, using isolated test saves.
- Menu resume, introduction dismissal and speaker selection checked in native controls.
- Native client rendered partial dialogue before the test speaker finished generating; final exchange committed once.
- The native tail-stream check delays director completion and verifies the entire spoken reply is already visible.
- Live inference retained Hana's unresolved hurt across restart/scene change and recalled Mira's private dog/tea facts. Replies streamed before completion.
- All 480 clips sampled through 33 frames, with finite skinning and nonzero movement.
- Live inference returned valid poses/gestures for four separate character sessions and selected the opening transition.
- Live kiss dialogue and the independent shared-relationship agreement passed.

Review images: Game/data/interface-menu.png and Game/data/interface-chat.png.
Motion audit: Game/data/native-S02.ppm.motions.csv.
Art prompts: Game/assets/prompts.json (built-in image generation).

Limits: one addressed woman speaks per turn; interpretation of unrestricted dialogue remains model-dependent. Motion counts are rigged variants across illustrated poses, not separately drawn frame-by-frame sequences.
