# Script language (`.sf`)

Indentation-based, Ren'Py-flavoured. All `scripts/*.sf` files of a game are compiled together; labels are global.

```
label start:
    scene cafe with fade
    show hana neutral at left with dissolve
    narrator "Rain taps the window."
    hana "You came back, [player]."          # [name] interpolates variables/expressions; [[ is a literal [
    $ trust += 1                             # expression statement
    menu:
        hana "What now?"                     # optional caption (speaker + text)
        "Sit at the counter":
            jump chat
        "Say something kind" if trust > 1:   # hidden when the condition is false
            jump chat
```

## Statements

| Statement | Meaning |
|---|---|
| `label name:` | jump target (not nestable) |
| `jump name` / `call name` / `return` | control flow; `call` returns to the next line |
| `if / elif / else` | conditions use the expression language below |
| `$ expr` | assignment or expression; l-values: `a`, `a.b`, `a[i]`; `+= -= *= /=` supported |
| `scene <bg> [with <transition> [secs]]` | change background (clears the stage; flushes every character's scene memory) |
| `show <char> [pose] [at <pos>] [with ...]`, `hide <char> [with ...]` | stage; positions: `left center right far_left far_right`, a number 0..1, or names from `game.json` `positions` |
| `anim <char> <motion>` | `breathe nod shake_head tilt emphasize chuckle bashful reassure sigh glance hop bob shake` |
| `with <transition> [secs]` | `fade dissolve slide_left slide_right slide_up slide_down wipe_left wipe_right flash` |
| `<char> "text"` / `narrator "text"` / `"Name" "text"` | dialogue; `<char> pose "text"` changes pose first |
| `menu:` | choices, optional caption, optional `if` per choice |
| `talk with a, b:` | an LLM conversation (below) |
| `minigame id(arg=value, ...) -> var` | run a minigame; see MINIGAMES.md |
| `play music "file" [fade s] [once]`, `stop music [fade s]`, `play sound "file"` | audio (needs an audio backend) |
| `pause <secs>`, `notify "text"`, `checkpoint`, `end ["text"]` | misc |

## Expression language

Numbers, strings, `true false null`, lists `[1, 2]`, `+ - * / // %`, comparisons, `and or not`, `in`, indexing, attribute
access (`rel.hana.trust`; a missing key is `null`, which is falsy). Functions: `min max clamp len str int seen("label")`,
`rand(a, b) chance(p) choice(list)`. No loops and no user functions by design: a downloaded game can only compute values from
its own variables. Randomness is seeded and saved, so loading a save never changes a roll.

## `talk` blocks

```
talk with hana, mira:
    goal: Hana wants to know why you came back.      # what this scene is about (the model sees it)
    commit: ask            # ask: the player confirms an offered ending; auto: it ends immediately
    min_turns: 1           # player messages before an ending can be offered
    max_turns: 18          # then on_end applies
    interject: on          # on | off   (and interject_limit: N)
    on_end: leave          # outcome used when she ends it, the player leaves, or the turn limit hits
    start: hana            # who speaks first (default: the first in the list)
    outcome stay "Stay for tea":           # id, label shown to the player
        rule: The player agrees to stay.   # plain language: when is this outcome true?
        when: trust >= 3                   # optional: hidden from the model unless true
        $ result = "stay"                  # body runs when the outcome is chosen
        jump after
```

The model never sees an outcome whose `when:` is false, so game rules (who is allowed to be romanced, what unlocks what)
live in the script, not in the prompt. See `games/the-light-we-leave-on` for a full example: the ported relationship rules are
`when:` expressions on the outcomes.
