"""Dependency-free streaming chat client. Run python chat.py --help."""
import argparse
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    parser.add_argument("--session", help="Resume an existing session UUID")
    parser.add_argument("--session-file", type=Path, default=Path(__file__).resolve().parent / ".chat-session")
    parser.add_argument("--new", action="store_true", help="Create a new conversation")
    parser.add_argument("--memory-test", action="store_true", help="Run two turns and check recall")
    parser.add_argument("--max-tokens", type=int, default=512)
    parser.add_argument("--system-file", type=Path, help="UTF-8 character/system prompt for newly created sessions")
    parser.add_argument("--temperature", type=float, default=0.7)
    parser.add_argument("--summary-every", type=int, default=0, help="Write a growing synopsis every N successful turns (0 disables)")
    parser.add_argument("--scratch", action="store_true", help="Create a fresh roleplay session with a character prompt entered now")
    args = parser.parse_args()
    if args.summary_every < 0:
        parser.error('--summary-every must not be negative')
    scratch_prompt = None
    if args.scratch:
        scratch_prompt = input('Describe your character, world and writing style (blank for no preset character): ').strip()
        scratch_prompt = scratch_prompt or 'Collaborate on fictional roleplay. Let the user establish the characters and setting. Do not invent a preset character. Preserve continuity and do not control the user character.'
        args.new = True
    headers = {"Content-Type": "application/json"}
    if os.getenv("INFERENCE_API_KEY"):
        headers["Authorization"] = "Bearer " + os.environ["INFERENCE_API_KEY"]

    def request(path, data=None, method=None):
        return urllib.request.urlopen(urllib.request.Request(
            args.url.rstrip("/") + path,
            data=json.dumps(data).encode() if data is not None else None,
            headers=headers, method=method), timeout=900)

    def create():
        data = {"system": scratch_prompt} if scratch_prompt else ({"system": args.system_file.read_text(encoding="utf-8")} if args.system_file else {})
        with request("/v1/sessions", data) as response:
            sid = json.load(response)["session_id"]
        args.session_file.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.session_file.with_suffix('.tmp')
        temporary.write_text(sid, encoding="utf-8")
        temporary.replace(args.session_file)
        return sid

    if args.memory_test or args.new:
        sid = create()
    elif args.session:
        sid = str(uuid.UUID(args.session))
    elif args.session_file.exists():
        sid = str(uuid.UUID(args.session_file.read_text(encoding="utf-8").strip()))
    else:
        sid = create()
    print(f"Session: {sid}\nCommands: /new, /history, /summary, /retry, /exit")
    pending = None
    completed = 0

    def summarize():
        print('Updating the roleplay synopsis...', flush=True)
        with request(f'/v1/sessions/{sid}/summary', {}, method='POST') as response:
            note = json.load(response)
        directory = Path(__file__).resolve().parent / 'inference' / 'data' / 'summaries'
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / f'{sid}.txt'
        temporary = path.with_suffix('.tmp')
        temporary.write_text(note['summary'], encoding='utf-8')
        temporary.replace(path)
        print(f"{note['summary']}\n[Summary saved: {path}]")

    def send(body):
        print("Assistant: ", end="", flush=True)
        received = ""
        done = None
        with request(f"/v1/sessions/{sid}/chat", body) as response:
            for line in response:
                if not line.strip():
                    continue
                event = json.loads(line)
                if event["type"] == "token":
                    received += event["content"]
                    print(event["content"], end="", flush=True)
                elif event["type"] == "done":
                    done = event
                    if not received:  # Idempotent replay sends the final answer only.
                        print(event["content"], end="", flush=True)
                elif event["type"] == "error":
                    raise RuntimeError(event["detail"])
        print()
        if done is None:
            raise RuntimeError("Stream ended before commit confirmation; use /retry")
        timings = done.get("timings", {})
        if "predicted_per_second" in timings:
            print(f"[{timings['predicted_per_second']:.1f} tokens/s; {done['prompt_tokens']} prompt tokens]")
        return done["content"]

    def body(text):
        return {"request_id": str(uuid.uuid4()), "message": text,
                "max_tokens": args.max_tokens, "temperature": args.temperature, "stream": True}

    if args.memory_test:
        secret = "orchid-" + uuid.uuid4().hex[:10]
        send(body(f"Remember my project code exactly: {secret}. Acknowledge briefly."))
        answer = send(body("What is my project code? Reply with only the code."))
        if secret not in answer:
            raise RuntimeError("Memory recall check failed; inspect the model's answers")
        print("Memory recall passed. Restart the client with this session to test persistence.")
        return

    while True:
        try:
            text = input("You: ").strip()
            if not text:
                continue
            if text == "/exit":
                break
            if text == "/new":
                sid, pending = create(), None
                completed = 0
                print(f"Session: {sid}")
                continue
            if text == '/summary':
                summarize()
                continue
            if text == "/history":
                after = 0
                while True:
                    with request(f"/v1/sessions/{sid}?after={after}&limit=100") as response:
                        page = json.load(response)
                    for turn in page["turns"]:
                        print(f"You: {turn['user']}\nAssistant: {turn['assistant']}")
                        after = turn["seq"]
                    if len(page["turns"]) < 100:
                        break
                continue
            if text == "/retry":
                if pending is None:
                    print("No failed request to retry.")
                    continue
            else:
                if pending is not None:
                    print("Use /retry to resolve the previous request, or /new for a new session.")
                    continue
                pending = body(text)
            send(pending)
            pending = None
            completed += 1
            if args.summary_every and completed % args.summary_every == 0:
                try:
                    summarize()
                except (OSError, RuntimeError) as exc:
                    print(f'Summary failed: {exc}. Your chat turn is saved; use /summary to retry.')
        except (urllib.error.URLError, RuntimeError, OSError) as exc:
            print(f"\nRequest failed: {exc}. Use /retry with the same request ID.", file=sys.stderr)
        except (EOFError, KeyboardInterrupt):
            print()
            break


if __name__ == "__main__":
    try:
        main()
    except (urllib.error.URLError, RuntimeError, ValueError, OSError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)
