#!/usr/bin/env python3
"""Reference minigame for the Stillframe protocol (stdin/stdout JSON lines).

It plays a tiny dice duel: both sides roll, the higher roll wins. `args.mode` lets tests make it misbehave.
Real minigames follow the same shape no matter what they are built with - read one JSON line, play, print
one JSON line with the result.
"""
import json, random, sys, time


def send(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def read():
    line = sys.stdin.readline()
    return json.loads(line) if line else None


start = read()
args = start.get("args", {})
mode = args.get("mode", "normal")
print("starting up (this line is plain logging and is ignored)")

if mode == "silent":
    time.sleep(60)
if mode == "crash":
    sys.exit(3)
if mode == "no_result":
    send({"type": "ready"})
    sys.exit(0)

send({"type": "ready"})
rng = random.Random(args.get("seed", 1))
send({"type": "progress", "value": 0.5, "label": "shuffling"})
if mode == "hang":
    # waits for cancel
    while True:
        msg = read()
        if msg is None or msg.get("type") == "cancel":
            sys.exit(0)

if args.get("ask_character"):
    send({"type": "say", "id": 7, "character": args["ask_character"], "prompt": "Cheer the player on in one line."})
    msg = read()
    send({"type": "event", "name": "cheer", "data": {"text": msg["text"]}})

you, her = rng.randint(1, 6), rng.randint(1, 6)
outcome = "win" if you > her else "lose" if you < her else "draw"
if mode == "bad_outcome":
    outcome = "banana"
send({"type": "result", "outcome": outcome, "data": {"you": you, "her": her, "difficulty": args.get("difficulty", 1), "player": start["context"].get("player", "")}})
