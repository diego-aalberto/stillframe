"""Real-model integration checks. Requires a running memory API; uses ordinary fiction."""
import argparse
import json
from pathlib import Path
import time
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    parser.add_argument("--output", type=Path, default=ROOT / "data" / "smoke-results.json")
    parser.add_argument("--long", action="store_true", help="Also force context compaction and verify old fact recall")
    parser.add_argument("--resume-check", type=Path, help="Verify recall in a saved report's session after restarting the server")
    args = parser.parse_args()
    import os
    headers = {"Content-Type": "application/json"}
    if os.getenv("INFERENCE_API_KEY"):
        headers["Authorization"] = "Bearer " + os.environ["INFERENCE_API_KEY"]

    def call(path, body=None):
        req = urllib.request.Request(args.url + path, headers=headers,
                                     data=json.dumps(body).encode() if body is not None else None)
        with urllib.request.urlopen(req, timeout=900) as response:
            return json.load(response)

    if args.resume_check:
        result = json.loads(args.resume_check.read_text(encoding="utf-8"))
        sid = result.get("long_session_id", result["session_id"])
        answer = call(f"/v1/sessions/{sid}/chat", {"request_id": str(uuid.uuid4()), "message": "Recall only established facts: what is the expedition code, where is the brass key, and who do we meet at what place and time?", "stream": False, "temperature": 0, "max_tokens": 160})
        passed = all(word in answer["content"].lower() for word in ("lantern-731", "blue", "inez", "east", "dawn"))
        result["restart_check"] = {"passed": passed, "response": answer}
        args.resume_check.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
        print(answer["content"], flush=True)
        if not passed:
            raise RuntimeError("Restart recall failed")
        print("Restart recall passed", flush=True)
        return
    result = {"ready": call("/ready"), "turns": []}
    sid = call("/v1/sessions", {"system": (ROOT / "prompts" / "roleplay.txt").read_text(encoding="utf-8")})["session_id"]
    result["session_id"] = sid
    prompts = [
        "I'm Rowan, 31. Our expedition is called Lantern-731. The brass key is in my blue coat, and we promised to meet Inez at the east pier at dawn. Mara, the rain has trapped us here again. What do you do?",
        "I set a chipped mug on the table. 'You always look like you're planning three routes out of every room.' Keep your reply under 100 words.",
        "Quick continuity check, out of character: give our expedition code, who has the key and where, and who we meet, where and when. Use only established facts.",
    ]
    for prompt in prompts:
        request_id = str(uuid.uuid4())
        body = {"request_id": request_id, "message": prompt, "stream": False, "temperature": 0.4, "max_tokens": 256}
        started = time.monotonic()
        answer = call(f"/v1/sessions/{sid}/chat", body)
        result["turns"].append({"prompt": prompt, "response": answer, "elapsed_seconds": time.monotonic() - started})
        print(answer["content"], flush=True)
    recall = result["turns"][-1]["response"]["content"].lower()
    result["recall_passed"] = all(word in recall for word in ("lantern-731", "blue", "inez", "east", "dawn"))
    result["idempotency_passed"] = call(f"/v1/sessions/{sid}/chat", body) == answer
    result["history_passed"] = len(call(f"/v1/sessions/{sid}")["turns"]) == 3
    if args.long:
        long_sid = call("/v1/sessions", {"system": "You are a precise conversation memory assistant. Preserve important facts from earlier turns. Follow the latest instruction briefly."})["session_id"]
        result["long_session_id"] = long_sid
        result["long_turns"] = []
        prompts = ["Remember these important facts: expedition Lantern-731; Rowan keeps the brass key in a blue coat; meet Inez at the east pier at dawn. Reply with ACK."]
        filler = "The harbor log records ordinary rain, rope inspections, dock repairs, lantern cleaning, supply checks and routine weather observations. "
        prompts.extend(f"Routine log batch {n}. These repeated background notes do not change the expedition facts. " + filler * 55 + "\nAcknowledge with ACK only." for n in range(1, 7))
        prompts.append("Recall only established facts: give the expedition code, key location, and who we meet at what place and time.")
        for i, prompt in enumerate(prompts):
            answer = call(f"/v1/sessions/{long_sid}/chat", {"request_id": str(uuid.uuid4()), "message": prompt, "stream": False, "temperature": 0, "max_tokens": 160})
            result["long_turns"].append(answer)
            print(f"Long-context turn {i + 1}: {answer['prompt_tokens']} input tokens, summarized through {answer['summarized_through']}", flush=True)
        history = call(f"/v1/sessions/{long_sid}")
        result["compaction_passed"] = history["summarized_through"] > 0 and len(history["turns"]) == len(prompts)
        result["long_recall_passed"] = all(word in answer["content"].lower() for word in ("lantern-731", "blue", "inez", "east", "dawn"))
        result["long_summary"] = history["summary"]
        print(answer["content"], flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    if not all(result[k] for k in ("recall_passed", "idempotency_passed", "history_passed")):
        raise RuntimeError(f"Integration check failed; see {args.output}")
    if args.long and not (result["compaction_passed"] and result["long_recall_passed"]):
        raise RuntimeError(f"Long memory check failed; see {args.output}")
    print(f"Real-model smoke checks passed. Results: {args.output}", flush=True)


if __name__ == "__main__":
    main()
