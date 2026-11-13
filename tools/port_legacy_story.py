#!/usr/bin/env python3
"""Converts the legacy "The Light We Leave On" (Game/story.py + Game/panel + Game/assets) into a v2 game package.

    python3 tools/port_legacy_story.py <legacy repo root> <output folder>

The output is a normal Stillframe game (folder form). It is checked in under games/ so nobody needs to run this
again; it stays here as the record of how the data was ported and as a template for converting other stories.
Needs only the Python standard library.
"""
import ast
import importlib.util
import json
import re
import shutil
import sys
from pathlib import Path

POSES = ["neutral", "wave", "explain", "folded", "shy", "laugh", "worried", "firm",
         "seated", "seated_relaxed", "affectionate", "cuddle"]
ROMANTIC_POSES = ["affectionate", "cuddle"]
APPEARANCE = {
    "hana": "Brown bob, amber eyes, gold hair clip; sleeveless ivory blouse with teal ribbon, navy pleated summer skirt, bare legs, brown ankle boots.",
    "mira": "Short dark curls with a copper streak, warm tan skin, green eyes; rust open shirt, black cropped camisole, olive shorts, black high-top sneakers.",
    "leila": "Long dark wavy hair loosely tied back, green-gray eyes; cream camisole, plum cardigan, charcoal tailored shorts, brown loafers.",
    "tessa": "Brown side braid, blue eyes; sleeveless tied denim shirt over cream camisole, dark denim shorts, brown ankle boots.",
}
SEATED_ROOMS = {"car", "cafe", "kitchen", "cinema", "roof", "hall", "bus", "workshop"}
CANVAS = [1260, 840]       # 3:2, like the background art
PREMISE = (
    "The Light We Leave On is a wholesome adult romantic ensemble set in Bellweather over six summer weeks. "
    "June, who owns the café called The Spare Room, is retiring; it may become a smaller café or a supper club. "
    "There is no tragedy and no magical rescue. The player is an adult (21+). "
    "Care is not romantic debt, and friendship is a good outcome. A refusal, a correction or disagreement is not cruelty. "
    "Never invent the player's speech, actions, feelings, touch or consent; offer a hand or a kiss instead of narrating an accepted one, "
    "unless the player's own words consent to it. "
    "Welcomed affection (hand-holding, kisses, hugs, cuddling) is warm and invited, never assumed. "
    "A consensual shared relationship between the player and all four women is possible only after each romance has developed and each woman "
    "independently opts in; consent for it is conditional until all four agree and can be withdrawn. Exclusive and shared agreements are different things. "
    "The women need not date each other. Prefer short, spoken replies, usually under 70 words. Let the scene determine the length. "
    "Use seated poses where people are sitting, firm poses for boundaries, and affectionate or cuddle poses only when the relationship allows it; a pose never asserts touch."
)
REPAIR_WORDS = ["sorry", "apologize", "apologise", "i was wrong", "my fault", "i shouldn't have", "i should not have",
                "lo siento", "perdón", "perdon", "me equivoqué", "fue mi culpa"]


def load_legacy(root: Path):
    spec = importlib.util.spec_from_file_location("legacy_story", root / "Game" / "story.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def legacy_ending_strings(root: Path):
    """The ending prose lives inside runtime.ending(); pull the literals out without importing the server code."""
    src = (root / "Game" / "runtime.py").read_text(encoding="utf-8")
    tree = ast.parse(src)
    fn = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == "ending")
    out = {}
    for st in fn.body:
        if isinstance(st, ast.Assign) and isinstance(st.targets[0], ast.Name):
            name = st.targets[0].id
            if name == "text":
                out["cafe"] = ast.literal_eval(st.value.body)
                out["club"] = ast.literal_eval(st.value.orelse)
            elif name == "romance":
                out["romance"] = ast.literal_eval(st.value)
        if isinstance(st, ast.If):
            seg = ast.get_source_segment(src, st)
            m = re.search(r"text\+=('(?:[^'\\]|\\.)*')", seg, re.S)
            if m and "ENOUGH ROOM" in seg:
                out["everyone"] = ast.literal_eval(m.group(1))
            elif m and "without needing" in seg:
                out["nolabel"] = ast.literal_eval(m.group(1))
    last = re.search(r"return text\+('(?:[^'\\]|\\.)*')", src)
    out["last"] = ast.literal_eval(last.group(1))
    return out


def q(s: str) -> str:
    """A script string literal."""
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def chunks(text: str, limit: int = 190):
    """Split prose into screen-sized beats at sentence ends."""
    parts = re.split(r"(?<=[.!?])\s+(?=[A-Z\"])", text.strip())
    out, cur = [], ""
    for p in parts:
        if cur and len(cur) + 1 + len(p) > limit:
            out.append(cur)
            cur = p
        else:
            cur = (cur + " " + p).strip()
    if cur:
        out.append(cur)
    return out


def clamp_set(var: str, delta: int) -> str:
    return f"$ {var} = clamp({var} + {delta}, 0, 5)"


class Writer:
    def __init__(self):
        self.lines = []

    def __call__(self, indent, text=""):
        self.lines.append("    " * indent + text if text else "")

    def text(self):
        return "\n".join(self.lines) + "\n"


def stage_positions(n):
    return {1: ["center"], 2: ["left", "right"], 3: ["far_left", "center", "far_right"], 4: ["far_left", "left", "right", "far_right"]}[n]


def default_pose(bg, who, node_id):
    return "seated" if bg in ("car", "cinema") else "neutral"


def pose_for_intro(bg):
    return "seated" if bg == "car" else ("seated_relaxed" if bg in ("cinema",) else "neutral")


def others_not(CAST, who, field, val):
    return [f'rel.{o}.{field} != "{val}"' for o in CAST if o != who]


def when_for(CAST, node, key, br):
    """Port of Runtime.select_outcome's gating, expressed as conditions that hide an outcome from the model."""
    st = br.get("status")
    if not (st in ("exploring", "exclusive", "shared") or key in ("romance", "flirt")):
        return None
    owner = node.get("owner") or node["cast"][0]
    conds = [f'rel.{owner}.status != "closed"', f"not rel.{owner}.repair"]
    conds += others_not(CAST, owner, "status", "exclusive")
    if st in ("exclusive", "shared"):
        conds += [f'(rel.{owner}.status == "exploring" or rel.{owner}.status == "shared")', f"rel.{owner}.trust >= 3", f"rel.{owner}.visits >= 2"]
    if st == "exclusive":
        conds += others_not(CAST, owner, "status", "shared")
    if st == "shared":
        for o in CAST:
            conds += [f'(rel.{o}.status == "exploring" or rel.{o}.status == "shared")', f"not rel.{o}.repair"]
    return " and ".join(conds)


def effects(w, indent, CAST, node, key, br, is_repair=False):
    cast = node["cast"]
    is_shared_scene = node["id"].startswith("S")
    recipients = cast if is_shared_scene else [node.get("owner") or cast[0]]
    for p in recipients:
        for stat in ("trust", "ease"):
            if br.get(stat):
                w(indent, clamp_set(f"rel.{p}.{stat}", br[stat]))
    if br.get("spark"):
        target = node.get("owner") or cast[0]
        w(indent, clamp_set(f"rel.{target}.spark", br["spark"]))
    owner = node.get("owner") or cast[0]
    if br.get("status"):
        w(indent, f'$ rel.{owner}.status = "{br["status"]}"')
    if br.get("repair"):
        w(indent, f"$ rel.{owner}.repair = true")
    if br.get("repaired"):
        w(indent, f"$ rel.{owner}.repair = false")
    if br.get("fact"):
        w(indent, f"$ facts.{br['fact']} = true")
    if br.get("flag"):
        w(indent, f"$ flags.{br['flag']} = true")
    if node.get("owner") and not is_repair:
        w(indent, f"$ rel.{owner}.progress += 1")
        w(indent, f"$ rel.{owner}.visits += 1")
    if node["id"] == "G01":
        w(indent, "$ flags.g01_done = true")


def emit_node(w, CAST, node, label, is_repair=False):
    bg = node["background"]
    cast = node["cast"]
    w(0, f"label {label}:")
    w(1, f'$ node = "{node["id"]}"')
    w(1, f"scene {bg} with fade")
    for who, pos in zip(cast, stage_positions(len(cast))):
        w(1, f"show {who} {pose_for_intro(bg)} at {pos} with dissolve")
    w(1, 'checkpoint')
    for c in chunks(node["opening"]):
        w(1, f"narrator {q(c)}")
    goal = node["question"]
    if node.get("romantic_beat"):
        goal += " Staging note, used only if the player has clearly welcomed romance and nothing blocks it: " + node["romantic_beat"]
    w(1, f"talk with {', '.join(cast)}:")
    w(2, f"goal: {goal}")
    w(2, "commit: ask")
    w(2, "max_turns: 18")
    if "leave" in node["outcomes"]:
        w(2, "on_end: leave")
    for key, br in node["outcomes"].items():
        w(2, f'outcome {key} {q(br["label"])}:')
        w(3, f"rule: {br['rule']}")
        cond = when_for(CAST, node, key, br)
        if cond:
            w(3, f"when: {cond}")
        effects(w, 3, CAST, node, key, br, is_repair)
        if br.get("ending"):
            w(3, "jump ending")
        elif br.get("next"):
            idx = int(br["next"][1:]) - 1
            w(3, f"$ shared_index = {idx}")
            w(3, f"jump n_{br['next']}")
        else:
            if node["id"].startswith("S"):
                w(3, f"$ stage = {node['stage']}")
                w(3, "$ visits = 0")
            w(3, "jump hub")
    w(0)


def build_main(story, endings):
    CAST, NODES, ROUTES = story.CAST, story.NODES, story.ROUTES
    w = Writer()
    w(0, "# The Light We Leave On - generated by tools/port_legacy_story.py from the legacy story data.")
    w(0, "# Relationship state lives in `rel.<who>`; story flags in `flags`; secrets revealed in `facts`.")
    w(0)
    w(0, "label start:")
    w(1, "scene car with fade")
    w(1, "narrator \"Six summer weeks in Bellweather. A flooded bus stop, a borrowed lift, and a café that might not be there much longer.\"")
    w(1, "jump n_S01")
    w(0)
    # hub
    w(0, "label hub:")
    w(1, "menu:")
    w(2, 'narrator "The room settles. What would you like to do with this stretch of the evening?"')
    for who in CAST:
        name = CAST[who]["name"]
        avail = (f'visits < 4 and rel.{who}.status != "closed" and (rel.{who}.repair or rel.{who}.progress < min(6, stage))')
        w(2, f'"Spend time with {name}" if {avail}:')
        w(3, f"jump visit_{who}")
    w(2, '"Let the story move on":')
    w(3, "jump advance")
    w(0)
    for who in CAST:
        w(0, f"label visit_{who}:")
        w(1, "$ visits += 1")
        w(1, f"if rel.{who}.repair:")
        w(2, f"jump n_REPAIR_{who}")
        for i, nid in enumerate(ROUTES[who]):
            w(1, f"if rel.{who}.progress == {i}:")
            w(2, f"jump n_{nid}")
        w(1, "jump hub")
        w(0)
    # advance
    all_shared = " and ".join(f'rel.{c}.status == "shared" and not rel.{c}.repair' for c in CAST)
    w(0, "label advance:")
    w(1, f"if shared_index == 8 and not flags.g01_done and {all_shared}:")
    w(2, "jump n_G01")
    w(1, "$ shared_index += 1")
    w(1, "$ visits = 0")
    for i in range(1, 10):
        w(1, f"if shared_index == {i}:")
        w(2, f"jump n_S{i + 1:02}")
    w(1, "jump ending")
    w(0)
    for nid, node in NODES.items():
        emit_node(w, CAST, node, f"n_{nid}")
    for who in CAST:
        node = story.repair_node(who)
        emit_node(w, CAST, node, f"n_REPAIR_{who}", is_repair=True)
    # ending
    w(0, "label ending:")
    w(1, "scene cafe with fade")
    w(1, "if flags.plan_cafe:")
    for c in chunks(endings["cafe"], 230):
        w(2, f"narrator {q(c)}")
    w(1, "else:")
    for c in chunks(endings["club"], 230):
        w(2, f"narrator {q(c)}")
    friend_detail = {"hana": "a seat on her actual break", "mira": "a film night without romantic ambiguity",
                     "leila": "a postcard and a call from the new city", "tessa": "breakfast without anything needing repairs"}
    for who, c in CAST.items():
        name = c["name"]
        r = f"rel.{who}"
        w(1, f'if {r}.status == "closed" or {r}.repair:')
        w(2, f'narrator {q(f"Your connection with {name} ends with distance. The story does not turn an unresolved boundary into forgiveness.")}')
        w(1, f'elif {r}.status == "exclusive":')
        for ch in chunks(endings["romance"][who], 230):
            w(2, f"narrator {q(ch)}")
        w(1, f'elif ({r}.status == "exploring" or {r}.status == "shared") and not {r}.repair:')
        w(2, f'narrator {q(f"You and {name} have another date planned. Neither pretends that a beginning is a guarantee; both want to see what comes next.")}')
        w(1, f'elif {r}.status == "friend" or ({r}.visits >= 2 and {r}.trust >= 3):')
        w(2, f'narrator {q(f"Your friendship with {name} continues through {friend_detail[who]}. It is chosen company, not a consolation prize.")}')
    everyone = f"flags.everyone_agreed and {all_shared}"
    w(1, f"if {everyone}:")
    for ch in chunks(endings["everyone"].replace("ENOUGH ROOM FOR ALL OF US\n", ""), 230):
        w(2, f"narrator {q(ch)}")
    none_excl = " and ".join(f'rel.{c}.status != "exclusive"' for c in CAST)
    w(1, f"if not ({everyone}) and {none_excl}:")
    for ch in chunks(endings["nolabel"].strip(), 230):
        w(2, f"narrator {q(ch)}")
    for ch in chunks(endings["last"].strip(), 230):
        w(1, f"narrator {q(ch)}")
    w(1, 'end "The Light We Leave On"')
    return w.text()


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    root, out = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    story = load_legacy(root)
    endings = legacy_ending_strings(root)
    if out.exists():
        shutil.rmtree(out)
    (out / "scripts").mkdir(parents=True)
    (out / "characters").mkdir()
    (out / "backgrounds").mkdir()
    (out / "sprites").mkdir()

    rel0 = {"trust": 1, "ease": 1, "spark": 0, "status": "acquaintance", "repair": False, "progress": 0, "visits": 0}
    manifest = {
        "format": "stillgame", "version": 1, "id": "the-light-we-leave-on", "title": "The Light We Leave On",
        "author": "Diego Alberto", "game_version": "2.0.0", "engine": ">=2", "start": "start",
        "cover": "backgrounds/cafe.png",
        "canvas": CANVAS,
        "variables": {"player": "You", "node": "", "stage": 0, "shared_index": 0, "visits": 0,
                      "rel": {c: dict(rel0) for c in story.CAST}, "flags": {}, "facts": {}},
        "positions": {"far_left": 0.14, "left": 0.33, "right": 0.67, "far_right": 0.86},
        "llm": {
            "system": PREMISE, "template": "chatml", "max_tokens": 320, "temperature": 0.8, "slots": 4,
            "interject_limit": 2, "interject_cooldown": 2, "repair_words": REPAIR_WORDS,
        },
        "theme": {"colors": {"accent": "#f0c987"}, "chars_per_second": 52},
    }
    (out / "game.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    for who, c in story.CAST.items():
        immutable = [
            {"label": "who", "text": f"{c['name']}, an adult woman aged {c['age']}."},
            {"label": "personality", "text": c["personality"]},
            {"label": "appearance", "text": APPEARANCE[who]},
            {"label": "desire", "text": c["desire"]},
            {"label": "fear", "text": c["fear"]},
            {"label": "boundary", "text": c["boundary"]},
            {"label": "voice", "text": "Example of her voice, not a catchphrase: " + c["voice"]},
            {"label": "private", "text": c["secret"] + " Never reveal this early."},
            {"label": "shared relationship needs", "text": story.SHARED_NEEDS[who]},
            {"label": "others", "text": "The other women are " + ", ".join(story.CAST[o]["name"] for o in story.CAST if o != who) + ". Never speak for them."},
        ]
        poses = {p: f"../sprites/{who}-{p}.png" for p in POSES}
        data = {
            "id": who, "name": c["name"], "color": c["color"], "immutable": immutable,
            "sprites": {"poses": poses, "default_pose": "neutral", "scale": 1.0, "anchor": [0.5, 1.0]},
            # Romantic poses only once she and the player are actually exploring something together.
            "pose_requires": {p: f'rel.{who}.status != "acquaintance" and rel.{who}.status != "friend" and rel.{who}.status != "closed" and not rel.{who}.repair'
                              for p in ROMANTIC_POSES},
        }
        (out / "characters" / f"{who}.json").write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        for p in POSES:
            shutil.copy2(root / "Game" / "panel" / "sprites" / f"{who}-{p}.png", out / "sprites" / f"{who}-{p}.png")

    for bg in sorted((root / "Game" / "assets" / "backgrounds").glob("*.png")):
        shutil.copy2(bg, out / "backgrounds" / bg.name)

    (out / "scripts" / "main.sf").write_text(build_main(story, endings), encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
