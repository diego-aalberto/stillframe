# 07 — Inference and continuity

Inference is optional. The engine renders on the CPU independently of the service. Its project settings do not choose inference hardware or start the backend.

## Project configuration

Insert this object under `inference` in the `.panel`:

```json
{
  "url": "http://127.0.0.1:8000",
  "key_env": "INFERENCE_API_KEY",
  "max_tokens": 512,
  "temperature": 0.7,
  "timeout": 660,
  "session_store": "state/conversations.sessions"
}
```

| Field | Meaning | Default |
|---|---|---|
| `url` | Base URL of the existing session/chat API | Absent means offline |
| `key_env` | Environment variable containing the bearer API key | `INFERENCE_API_KEY` |
| `max_tokens` | Requested generation limit | `512`; must fit service limits |
| `temperature` | Sampling temperature sent to the service | `0.7` |
| `timeout` | HTTP timeout in seconds | `660` |
| `session_store` | Optional file retaining session IDs between launches | None |

Start the service separately using its existing launcher. Do not put credentials directly in game data. The chat controls appear only when a URL is configured and the scene has actors. `{}` or an omitted inference section permits fully offline scenes and animation.

## What the character receives

The client creates a session on first message for each `(scene ID, actor ID)` pair. The session's system text combines the actor's `startup_prompt`, its authored `history` serialized as JSON, and its declared pose/animation capability names.

Write the startup prompt as the character's instructions: identity, voice, knowledge, boundaries and the immediate situation. Write history as concise continuity notes. `scene.text` is visible presentation text; it is **not** automatically included in the character prompt. Put important scene facts into the startup prompt/history too.

The prompt is supplied when creating the session. Returning to an existing session does not replace that original prompt with later file edits. Use a new session store, or omit session persistence and relaunch, when you deliberately want fresh character context for testing.

## Three different kinds of history

| Data | Where it lives | Behavior |
|---|---|---|
| Authored continuity | Actor's `history` in the scene | Seed information supplied when creating the session |
| Actual messages and replies | Inference service's database | Service-managed conversation history and summaries |
| Session identifiers | Optional `.sessions` file | Reconnects scene/actor pairs to existing service sessions |

A generated session store has format `panel.sessions`, version 1, and a `sessions` object. Its keys are JSON-encoded scene/actor pairs and values are service session IDs. It is a runtime file; let the engine create it rather than inventing IDs.

Without `session_store`, sessions are reused during the current run but their IDs are not retained by the engine after exit. Old conversations may still exist in the service database. A session-store file alone is not a backup of those conversations: the corresponding service database must also be available.

No current game-progress save records the active scene, branch choices, current animation or relationship state. Histories do not merge between scenes or actors automatically. Changing the backend URL while reusing a store from another database can leave invalid session IDs; start a separate store for that backend.

## Streaming protocol

The client uses `POST /v1/sessions` to create sessions, then `POST /v1/sessions/{sid}/chat` with `stream: true`. Replies are newline-delimited JSON, one object per line. The main events are:

```json
{"type":"token","content":"A partial reply"}
```

```json
{"type":"done","content":"The complete final reply."}
```

```json
{"type":"error","detail":"A description of the failure."}
```

Completion unlocks input immediately; it does not wait for a clip, text effect or response connection teardown. There is one generation in flight in the player. The message field remains available for drafting while generating. The replacement player does not issue the legacy second director request.

## Optional animation command

A compatible stream may emit:

```json
{"type":"animation","actor":"speaker","pose":"standing","animation":"wave"}
```

All three IDs must resolve in the current scene. Invalid references are ignored. The command selects and starts an existing clip; it cannot supply a filename, execute code or create assets.

The current service emits text events and does not automatically produce these animation events. Writing an animation request in ordinary dialogue text does not invoke it. A future service adapter must emit the event explicitly. The engine's capability manifest lists scene → actor → pose → animation IDs for that integration.
