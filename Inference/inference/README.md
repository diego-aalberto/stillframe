# Inference service

The compute engine is **llama.cpp, written in C++ with CUDA acceleration**. Python/FastAPI supplies HTTP orchestration and persistent memory; Python does not execute transformer layers. The design targets ordinary NVIDIA gaming PCs running Windows or Linux, using one GPU with optional CPU/RAM offloading for 12–20B quantized models. No particular GPU model or developer machine configuration is hard-coded.

## Install the engine

The current workspace is already installed and verified with llama.cpp **b10930 / 56381e407**, Windows x64 CUDA 13.3, and Magnum v4 12B Q4_K_M. The launcher uses the same portable defaults on other machines. For a fresh Windows setup use `python -m inference.download --model --windows-engine`; archives and model are SHA256-checked against `assets.json`. The engine's `--list-devices` and actual model inference both passed locally.

The pinned CUDA 13 build is for supported modern NVIDIA architectures and a compatible driver. NVIDIA documents CUDA 13.x minor-version compatibility with driver branches >=580, with feature limitations: see [CUDA compatibility](https://docs.nvidia.com/deploy/cuda-compatibility/minor-version-compatibility.html). This does not make one binary suitable for every NVIDIA GPU. Older cards may need another CUDA build through `--engine`; the model and API stay the same.

1. Install the Python dependencies from the root README.
2. If the pinned download is unsuitable, obtain a compatible CUDA build of `llama-server` from the official [llama.cpp releases](https://github.com/ggml-org/llama.cpp/releases). On Windows, extract the executable **and all accompanying runtime DLLs** into a separate engine directory and select it with `--engine`. Select a build supporting your GPU architecture, operating system and NVIDIA driver. The engine must support `--fit`, `--fit-target`, `--list-devices`, `/apply-template` and `/tokenize`. Record the selected release and `llama-server --version` output.
3. Add GGUF weights as described in `models/README.md`.
4. Run from the project root:

```powershell
.venv\Scripts\python -m inference.run --model models\model.gguf
```

Alternatively specify `--engine C:\path\to\llama-server.exe`. On Linux use a CUDA-enabled llama.cpp build and `--engine /path/to/llama-server`. See the official [build instructions](https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md). Older NVIDIA architectures may require a compatible earlier CUDA toolkit or a source build that includes their compute capability. The launcher checks that the engine exposes a CUDA device and fails clearly otherwise. GPU support ultimately depends on the engine build; not every historical NVIDIA card is supported. AMD, Intel GPU, Apple Metal and CPU-only server configurations are outside the current target.

The launcher starts the engine on loopback port 8081, waits up to 600 seconds for model loading, then starts the API on loopback port 8000. Ctrl+C stops both processes. `--startup-timeout` changes the model-loading allowance. Engine logs remain visible for GPU allocation and template diagnostics. If the engine dies after startup, API readiness and chat requests fail; restart the launcher.

API docs: [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs). `GET /health` checks the API process; `GET /ready` checks the engine and configured capacity. No weights means readiness cannot pass.

## API contract

This is a **session-based API**, not a drop-in OpenAI chat-completions endpoint. Call the memory API on 8000; calls directly to 8081 bypass durable memory.

| Method and route | Body / behavior |
| --- | --- |
| `POST /v1/sessions` | `{"system":"You are a helpful assistant."}`; returns `session_id` |
| `POST /v1/sessions/{id}/chat` | `request_id` UUID, `message`, optional `max_tokens` (512), `temperature` (0.7), `stream` (true) |
| `GET /v1/sessions/{id}?after=0&limit=100` | Summary and paginated full turns; use last turn's `seq` as next `after` |
| `DELETE /v1/sessions/{id}` | Delete the session and its stored turns |
| `GET /ready` | 200 when engine capacity is suitable, otherwise 503 |

Example with Python's standard library:

```python
import json, urllib.request, uuid

def post(path, body):
    req = urllib.request.Request(
        'http://127.0.0.1:8000' + path,
        data=json.dumps(body).encode(),
        headers={'Content-Type': 'application/json'},
    )
    with urllib.request.urlopen(req, timeout=900) as response:
        return json.load(response)

sid = post('/v1/sessions', {})['session_id']
result = post(f'/v1/sessions/{sid}/chat', {
    'request_id': str(uuid.uuid4()),
    'message': 'Remember that my project is called Orchid.',
    'stream': False,
})
print(result['content'])
# Reuse sid for subsequent turns, including after restarting the service.
```

Chat requests may also supply `seed` (an integer from 0 through 2147483647) and `response_schema` (a JSON schema for the completion). These are forwarded to llama.cpp for character-specific sampling and constrained story responses. Omit them for ordinary free-text chat. A committed retry still replays the first result; changing sampling or schema settings under the same request ID does not regenerate it.

Streaming uses newline-delimited JSON (`application/x-ndjson`):

```json
{"type":"token","content":"Hello"}
{"type":"done","session_id":"...","request_id":"...","content":"Hello","prompt_tokens":42,"finish_reason":"eos","timings":{},"summarized_through":0}
```

An `error` event replaces `done` if generation or context preparation fails after streaming headers are sent. Check event types even when HTTP status is 200. Only `done` confirms persistence. Disconnects cancel active generation; a disconnect racing with the final commit may still leave a completed turn, so retry with the **same request ID and message**. A committed retry returns its original result without generating again. Changing a message under an existing request ID returns 409. The first successful request's sampling options win on retries.

Malformed input returns 422; missing sessions 404; failed authentication 401; busy sessions/capacity 429; unavailable engines 503. Non-streaming generation failures return 502. Oversized context returns 422 for non-streaming or `context_too_large` in the stream.

## Memory behavior

SQLite in `inference/data/memory.sqlite3` stores the immutable system prompt, complete successful user/assistant pairs, rolling summary and request results. WAL mode supports concurrent reads. Each response and its updated summary commit atomically. An interrupted generation never commits a partial answer. Requests within a session serialize; separate sessions share bounded GPU capacity. Summary generation holds that capacity too.

For each request the service renders the model's chat template, tokenizes it, reserves requested output plus 32 guard tokens, then submits **those token IDs** to inference. If the prompt exceeds the budget, the same model summarizes older complete turns and preserves recent turns verbatim where space permits. Older text remains on disk even after compaction. Memory survives both API and engine restarts; the GPU prompt cache does not provide durability.

Summaries are lossy and model-dependent. The full archive is available through the history API, but it is **not automatically semantically searched** for omitted facts. This is bounded conversational memory, not perfect unlimited recall. If one old turn cannot fit in a summarization request, or the new message plus system/summary cannot fit, the request fails explicitly instead of silently discarding context. Summarization adds latency and rebuilds the prompt prefix when compaction occurs.

## Performance and tuning

The launcher enables automatic VRAM fitting, continuous batching and automatic Flash Attention selection, while disabling context shifting. The backend requests prompt-prefix reuse. These capabilities are documented in the [llama-server reference](https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md). Support depends on model architecture and engine build.

Portable defaults are one slot, automatic context (normally 4,096–16,384 tokens based on model metadata and free VRAM), a 512-token output ceiling, 512-token logical batches and 128-token microbatches. The engine chooses the GPU layer count from actual free VRAM while preserving the selected context and a 1,024 MiB reserve. Remaining layers can use CPU/RAM. This is capacity fitting, not a speed guarantee or continuous monitoring of other applications. Close memory-heavy games before inference. If available RAM is insufficient, choose a smaller model. Override with `--context 4096`, `--context 8192`, or another supported value when benchmarking reproducibly.

FP16 KV caching is the compatibility default. `--kv-cache q8_0` reduces KV storage and explicitly requires Flash Attention; use it after verifying support on the target model/GPU. Host prompt-cache snapshots are disabled to avoid the engine's potentially large extra RAM allocation; active-slot prefix reuse remains enabled. Compaction and session switching can invalidate that prefix.

`--parallel 2` doubles total configured context allocation and increases memory demand; use it only with measured spare capacity. `--device CUDA1` selects a different visible NVIDIA GPU; by default the first CUDA device is used. Multiple GPUs are not combined automatically.

```powershell
.venv\Scripts\python -m inference.run --context 2048 --vram-reserve 1536
.venv\Scripts\python -m inference.run --context 4096 --kv-cache q8_0
.venv\Scripts\python -m inference.run --context 8192 --parallel 2 --batch-size 1024 --ubatch-size 256
```

Benchmark actual prompts after model warmup. Compare prompt-processing and generated tokens/second in engine logs and response `timings`, first-response delay, long-context latency and peak VRAM. Test after compaction as well as with a reusable prefix. No universal fastest setting or measured tokens/second is claimed without weights.

For architecture-specific options, start llama-server yourself, then start the API in **one worker**:

```powershell
$env:LLAMA_URL = 'http://127.0.0.1:8081'
$env:CONTEXT_TOKENS = '4096'
$env:PARALLEL = '1'
.venv\Scripts\python -m uvicorn inference.app:create_app --factory --host 127.0.0.1 --port 8000 --workers 1 --limit-concurrency 64
```

Use compatible engine settings, disable context shifting and keep the engine private. If Q8 KV or Flash Attention is unsupported, configure a supported KV type/attention mode manually. The API checks `/props` before inference; it rejects insufficient slot/context capacity. Do not run multiple API processes against the same database: session locks are process-local. Horizontal scaling requires distributed locking and a shared database design.

| Environment variable | Default |
| --- | --- |
| `LLAMA_URL` | `http://127.0.0.1:8081` |
| `MEMORY_DB` | `inference/data/memory.sqlite3` (absolute project path) |
| `INFERENCE_API_KEY` | Empty; optional bearer key for local use |
| `CONTEXT_TOKENS` | 4096; launcher sets from `--context` |
| `PARALLEL` | 1; launcher sets from `--parallel` |
| `MAX_OUTPUT_TOKENS` | 512 |
| `SUMMARY_TOKENS` | 256 |
| `GENERATION_TIMEOUT` | 600 seconds for context preparation and generation |
| `QUEUE_TIMEOUT` | 15 seconds per lock/capacity wait |

Environment variables are read at startup. `.env` files are not automatically loaded. Relative custom database paths resolve against the process working directory.

`requirements.txt` applies `constraints.txt` to reproduce the dependency versions used in validation. Review and retest those constraints when upgrading dependencies. The engine is selected separately to match the deployment GPU and model.

## Start, stop and CPU benchmarking

Use `start-backend.cmd` to launch the managed backend in the background. It writes bounded logs to `inference/data/backend.log` and `engine.log`, owns a workspace lock, and records process identities. `end-backend.cmd` sends a loopback stop request, waits up to 25 seconds for graceful API shutdown and engine termination, then verifies the owned process identities before cleanup. `backend-status.cmd` reports whether this workspace's backend is ready. These scripts never kill a process by name alone. An unexpected supervisor exit is handled by the watchdog, which terminates only the engine process tree recorded for this workspace.

`start-server.cmd` is a compatibility alias for `start-backend.cmd`. Existing roleplay chat is `start-chat.cmd`, fresh user-defined character setup is `start-chat-fresh.cmd`, and `start-chat-summary.cmd` saves a rolling synopsis every four turns. Summaries are also available interactively through `/summary` and are written under `inference/data/summaries/`. The synopsis is an aid for reading/exporting a roleplay; the API's own summary remains the continuity memory used during generation.

To deliberately run inference without touching an NVIDIA GPU, use `benchmark-cpu.cmd` or `benchmark-cpu.ps1`. CPU mode skips `--list-devices`, passes `--device none`, `--n-gpu-layers 0`, disables Flash Attention, and uses FP16 KV storage. The same API, SQLite memory and model are exercised, then all managed processes are stopped. This is useful as a baseline for CPU/RAM throughput and for testing on a machine without a usable NVIDIA driver. A CUDA-built engine can still run its CPU backend; a separate CPU-only llama.cpp binary may be preferable on machines where loading CUDA DLLs is undesirable.

## Deployment and data

For network binding, set a strong `INFERENCE_API_KEY` and pass `--host 0.0.0.0`; the launcher refuses external binding without a key. Clients send `Authorization: Bearer YOUR_KEY`. This is a single-owner service: all clients holding that key share access to all session IDs. It does not implement user accounts or tenant authorization. Put TLS, request-body limits and rate limits at a reverse proxy before exposing it beyond a trusted network. The underlying engine remains loopback-only.

History is plaintext on disk and grows with successful turns. Protect its directory. Back up using SQLite's online backup API or stop the service before copying the database and its WAL files. Deleting a session is a logical database deletion, not guaranteed forensic erasure. Model files, memory and the virtual environment are excluded from Git.

## Validation

Run `python -m pytest inference/tests -q` using the project environment. Deterministic tests exercise persistence after application recreation, session isolation, rollback, budget compaction, API authentication and retry behavior. For real GPU tests, use `python chat.py --memory-test` and `python -m inference.smoke_test --long`. Restart and run `python -m inference.smoke_test --resume-check inference/data/smoke-results.json`.

These real-model tests passed with the installed model, including repeated summary compaction, complete archive preservation and recall after restarting both engine and API. Raw responses and timings are stored in `inference/data/smoke-results.json`. The tests cover a small known set of facts, not general long-context accuracy or all roleplay behaviors.
