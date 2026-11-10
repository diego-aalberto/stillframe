# The Light We Leave On — playable integration

Run **`../start-game.cmd`**. It starts or reuses the managed local inference backend, then opens the game through **Engine/** using Qt's CPU software renderer. Run `setup-engine.cmd` first if its Python environment has not been installed.

The first launch may take time to load the model. The backend stays running when you close the game so reopening is quick. Use `Inference/end-backend.cmd` when you want to unload it. The game renderer always uses the CPU; inference keeps its existing backend configuration.

## Playing

- Start or resume from the menu, read the scene introduction, then select Begin scene. Type what you want to say and click Speak. In group scenes, choose the woman you address. Others present witness the conversation; absent characters do not receive it.
- Continue becomes available when a supported conversation outcome is ready. It commits that outcome once. Keep talking preserves the recognized outcome while you linger; new conduct or an explicit change of mind can invalidate it. There is no Leave scene button or navigation bypass. The model must recognize an outcome or end the conversation.
- Between shared scenes, choose up to four personal visits, or Next day. Individual route chapters unlock as the calendar advances.
- Romance includes hand-holding, first kisses, embraces, and cuddling. Four separate romances and four explicit shared agreements unlock **G01**, the all-four relationship scene and ending. Exclusive commitments cannot silently be converted into a shared relationship.
- Progress saves automatically. Retry response recovers a pending inference request using its original request ID. New story backs up the current save and creates fresh character sessions.
- New saves ask for your name; it is saved and supplied to every character. The ? button at the top shows short scene-specific hints. Dialogue input grows as you write; very long drafts can scroll. You can draft your next message while the current response finishes.

## Architecture and memory

`story.py` is the authored catalog: 35 scenes plus the private repair template. `runtime.py` owns relationship values, chapter availability, outcome gates, and endings. `panel_player.py` adapts that controller to the generic Panel runtime, and `StoryPlayer.qml` supplies the game interface. `panel/` contains the converted project, scenes, rigs, animation clips and extracted pose images. The old `server.py` and C++ bridge remain for the legacy export. No browser or cloud inference is required.

One GPU/model process serves a separate persistent joint dialogue-and-decision session for each woman. Her seed contains her identity, voice, desires, boundaries and conversation lifecycle rules. Each receives only her own relationship state, memories and witnessed conversations. Other characters' private transcripts are excluded. Shared-relationship consent is disclosed to the group only after each has independently agreed.

“Seed” has two meanings here: the personality/system prompt and a numeric sampling seed. Both are character-specific. A stable character seed plus the journal length initializes each turn; retries reuse the exact payload. This supports consistent sampling parameters, not guaranteed byte-identical GPU output across machines. Personality comes from the prompt and memory, not the numeric seed.

Speech streams from the reply field of a single schema-constrained response. That same response classifies scene completion, current boundaries, memories and whether she wants to keep talking. An evidence-backed allowed outcome unlocks Continue even if she remains willing to chat. Completion is retained in her next-turn context, rather than requiring the player to earn it again. Relationship prerequisites are still enforced by the game.

A first refusal offers Continue and one Keep talking opportunity. A further refusal ends this encounter and removes Keep talking. A new evidenced insult immediately costs one trust point and creates a lasting incident; remembering old hurt or declining an apology does not add a penalty. Refused personal scenes do not award chapter completion or romantic progress. Repair is available through the existing repair route. A new scene resets the encounter's refusal count, not her emotional memory.

`encounter.py` defines the lifecycle contract and interruption destinations. `runtime.py` validates and applies the model decision atomically with memories and save state. Scene navigation cannot bypass a terminal refusal. Model classifications remain probabilistic; exact evidence is necessary but is not a semantic proof.

Game state lives in `Game/data/save.json`; inference histories remain in the existing inference SQLite database. The complete pending joint request is saved before dispatch and retried idempotently. Legacy pending requests finish with their old payload before new sessions receive the upgraded system seed; old backend sessions and local memories remain preserved. Emotional reactions commit immediately after the exchange, without waiting for Continue. Unresolved hurt survives greetings, scene changes and restarts; reconciliation requires an actual repair attempt. Personal facts, incidents and witnessed journals persist outside inference compaction. Recent exchanges plus relevant older exchanges accompany each message. Retrieval and model interpretation can still miss details.

All characters are adult women. The player is an adult; dialogue can adapt to the identity they give. No predetermined dialogue-choice menu is used. Native controls select a speaker, visit, or scene exit, rather than choosing what the player says.

## Visual scope

All four women have finished Hana-style summer artwork: 12 distinct illustrated poses each, including seated, shy, laughing, affectionate and cuddle poses. Each pose supports ten motion selections (120 combinations per character, 480 total). The new engine animates each flattened pose as one rigid layer with gentle movement and rotation. It cannot reproduce the old mesh deformation or independent head/limb motion without separately drawn layers. These are reused motion clips, not 480 separately drawn animations. All ten locations have painted backgrounds.

Inference selects a permitted pose with each reply. On every new player message, a gesture is drawn uniformly from the ten gestures plus one duplicate of the current gesture: current has probability 2/11, each other gesture 1/11. Retries preserve the draw. The chosen animation loops continuously until another message changes it. Scene and relationship restrictions still apply to poses. Paired kisses and cuddles are conveyed in dialogue alongside affectionate individual poses.

The native interface uses rounded pastel controls,  a start/resume menu, scene introductions, history and autosave. Hana's dialogue is mint, Mira's peach, Leila's lilac and Tessa's blue. Escape toggles the menu; Ctrl+Enter sends a message. Start with **Begin your story**, then **Begin scene**. **Continue** commits a conversational outcome; **Keep talking** stays in the scene.

The main dialogue card is translucent over the scene; the small name pills and controls remain opaque. Quoted speech, including straight or curly quotation marks, renders in bold. Character motion runs at half the previous speed. The adapter displays streamed speech on the UI thread at short intervals, without waiting for the remaining decision fields to finish. Story actions still wait for that metadata to commit safely. Character prompts encourage reciprocal flirting and modest invitations while preserving trust and consent requirements.

## Scene music

Put your looping PCM WAV files in `Game/music`. The [music list](music/README.md) maps all 36 expected filenames to scenes; `tracks.csv` and `tracks.json` provide the same mapping. Music changes with the scene and loops automatically. Missing files are silent; no audio has been generated or supplied.

## Build and checks

From the repository root:

```powershell
Engine\.venv\Scripts\python.exe -m pytest Game/test_panel_port.py Game/test_runtime.py Game/test_encounter.py Engine/tests -q
Engine\.venv\Scripts\python.exe Game/export_panel.py
```

The exporter rebuilds `Game/panel/` from the existing story and artwork. Launch with `start-game.cmd` to include the story controller: opening `story.panel` in the generic player alone previews scene assets but does not supply the game's progression logic. The port tests cover all scenes, pose/motion combinations, save continuity, streamed responses and actual software-rendered QML windows. Inference is mocked in those checks.

The original native smoke scripts target the retained C++ player, not this port. `Game/smoke_live.py` and `Game/check_continuity_live.py` remain available for backend checks with isolated sessions.

`export_story.py` regenerates the scene guide and standalone `Story/story-map.tex` from the catalog. Open the LaTeX file in Codex for its PDF preview.

Current limitations: model intent classification is probabilistic; only one addressed character speaks per turn; no automatic arbitrary-promise detector. Unsupported or ambiguous outcomes stay in conversation. A malformed model response changes no branch and asks for a new message; an interrupted request can be retried safely.

`Game/check_encounter_live.py` checks completion and repeated refusal against the running local model using an isolated save and sessions. It never changes the player save.
