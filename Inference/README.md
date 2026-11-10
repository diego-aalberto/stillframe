# Python chat test

This project provides a local text-chat API backed by llama.cpp (C++/CUDA), with Python handling streaming requests and persistent SQLite conversation memory. This workspace now includes verified **Magnum v4 12B Q4_K_M** weights at `models/model.gguf` and a Windows CUDA engine in `inference/bin/`. Large binary files remain excluded from Git.

## Already installed in this workspace

This project now lives in the root's `Inference` subfolder. Run all commands in this guide from that folder.

On Windows, double-click `start-chat.cmd` to chat if the API is running. Start it with `start-backend.cmd` (or the compatibility launcher `start-server.cmd`) when needed, and use `end-backend.cmd` to stop it. The managed backend runs in the background. The server uses portable automatic GPU fitting. Run only one server instance on the default ports.

Equivalent commands from the root:

```powershell
.venv\Scripts\python -m inference.run
.venv\Scripts\python chat.py --system-file inference\prompts\roleplay.txt
```

The second command belongs in a separate terminal. The included character is an original fictional cartographer; edit `inference/prompts/roleplay.txt` and start a new chat (`--new` or `/new`) to change characters. Existing sessions retain their original system prompt. This is a starting prompt, not a restriction on creating your own characters.

To run normal chat on CPU only, double-click `start-backend-cpu.cmd`, then open any chat launcher. It stops this project's existing backend before starting CPU mode, so an existing GPU server cannot accidentally be reused. The CPU backend stays running until you use `end-backend.cmd`. No automatic test is run. Model layers, KV cache and other tensor operations stay on CPU; NVIDIA devices are hidden from the engine process. The default CPU context is 4K, adjustable with `start-backend-cpu.cmd --context 8192`.

Backend controls are separate from chat launchers: `start-backend.cmd` starts in the background, `end-backend.cmd` stops only this workspace's recorded API/engine/watchdog processes, and `backend-status.cmd` reports status. The managed backend uses automatic context sizing, bounded log rotation, a workspace lock, graceful shutdown and an orphan watchdog. Use `start-chat.cmd` for Mara, `start-chat-fresh.cmd` to describe a new character from zero, or `start-chat-summary.cmd` to save a roleplay synopsis every four turns. Each launcher has its own session file.

## Layout

```text
models/                Add your GGUF weights here
  README.md            Model format and placement instructions
inference/             API, memory, engine launcher, tests, configuration
  bin/                 Extract llama-server and its runtime libraries here
  data/                SQLite memory is created here automatically
  README.md            Installation, API reference, tuning, architecture
chat.py                Dependency-free interactive Python API client
README.md              This chat-client guide
```

## Start

Use CPython 3.11 or newer (3.12 recommended), with pip and venv. From this project's root, on Windows:

```powershell
python -m venv .venv
.venv\Scripts\python -m pip install -r inference\requirements.txt
```

On a fresh checkout, download the pinned model and Windows CUDA engine (about 8 GB total):

```powershell
.venv\Scripts\python -m inference.download --model --windows-engine
```

The bundled download recipe targets CUDA 13.3; older GPUs/drivers may need a different compatible engine as explained in [inference/README.md](inference/README.md). Linux users run `--model` only and install a Linux CUDA engine separately. The downloader resumes interrupted files and verifies published SHA256 hashes. Start the service:

```powershell
.venv\Scripts\python -m inference.run --model models\model.gguf
```

Open a second terminal in the same folder:

```powershell
python chat.py
```

The inference server targets NVIDIA GPUs on Windows and Linux. On Linux use `.venv/bin/python` instead of `.venv\Scripts\python`. The client itself uses only Python's standard library and can run on other operating systems when connecting to a supported server.

## Context and persistence test

```powershell
python chat.py --memory-test
```

This creates a new session, provides a random project code in the first turn, and asks the model to recall it in the second. It checks the answer and exits unsuccessfully if recall fails. The second request contains only the new question: the server supplies the conversation context.

For a restart test, run `python chat.py`, tell it a personal project fact, then `/exit`. Stop and restart the API, run `python chat.py` again, and ask about the fact. The client resumes the ID saved in `.chat-session`; the server reloads the transcript from SQLite. Do not delete `inference/data/` between runs. Changing model weights does not remove memory; start a new session when changing incompatible models or prompts.

## Commands and options

| Command | Action |
| --- | --- |
| `/new` | Start a new conversation, preserving the old one on the server |
| `/history` | Print the complete stored transcript in pages |
| `/retry` | Retry the failed request with its original ID, preventing duplicate committed turns |
| `/exit` | Quit while preserving memory |

```powershell
python chat.py --new
python chat.py --session YOUR-SESSION-UUID
python chat.py --url http://127.0.0.1:8000 --max-tokens 512
python chat.py --session-file another-session.txt
python chat.py --new --system-file inference\prompts\roleplay.txt --temperature 0.6
```

`--memory-test` always creates a new session. `--session` applies to that invocation; default sessions and `/new` are saved in the session file. Retry state is held in the running client, so inspect `/history` after a client crash before re-sending an uncertain turn. Do not switch servers while reusing a session file unless both use the same database.

If authentication is enabled on the server, set the same `INFERENCE_API_KEY` environment variable in the client terminal. No key is required for the default loopback setup. Streamed text is provisional until the final `done` event confirms a durable save. Generation speed appears when the engine supplies timing data.

## Backbone tests without weights

```powershell
.venv\Scripts\python -m pip install -r inference\requirements-dev.txt
.venv\Scripts\python -m pytest inference\tests -q
```

These tests substitute a deterministic backend to check context assembly, persistence, compaction, isolation, failed-stream rollback, retry deduplication and API validation. They do not measure model recall quality, CUDA compatibility or inference speed. Run the real memory test after adding weights.

## Real-model validation

```powershell
.venv\Scripts\python -m inference.smoke_test --long
```

This checks ordinary roleplay, fact recall, idempotency, full stored history, and compaction of a conversation longer than the 4K live context window. Results are saved in `inference/data/smoke-results.json`. Restart the server, then verify persisted recall:

```powershell
.venv\Scripts\python -m inference.smoke_test --resume-check inference\data\smoke-results.json
```

The included model passed those checks in this workspace. The 4K test configuration generated roughly 45–51 tokens/second in short test replies on an RTX 5070 Ti Laptop GPU with 12 GB VRAM. This is a local measurement, not a promise for other computers or workloads. Summarized recall passed for the test facts; summaries remain lossy.
