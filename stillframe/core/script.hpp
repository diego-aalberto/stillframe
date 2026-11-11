// The Stillframe script language (.sf): readable, indentation-based, Ren'Py-flavoured.
// See docs/SCRIPT.md for the full reference. This header is the compiled form.
#pragma once
#include "expr.hpp"
#include <map>
#include <set>

namespace sf {

struct TextSeg { bool is_expr = false; std::string text; Expr expr; };
using Text = std::vector<TextSeg>;
Text parse_text(const std::string& raw);               // "[name]" interpolation, "[[" escapes a bracket
std::string render_text(const Text& t, EvalContext& ctx);

enum class Op {
    Say, Scene, Show, Hide, Anim, With, Music, StopMusic, Sound, Pause,
    Set, Jump, JumpIfNot, Call, Return, Menu, Talk, Minigame, End, Notify, Checkpoint
};

struct MenuOpt { Text text; Expr cond; int target = -1; };
struct TalkOutcome { std::string id; Text label; std::string rule; Expr when; int target = -1; };
struct TalkSpec {
    std::vector<std::string> cast;
    std::string start;
    Text goal;
    std::string commit = "ask";   // "ask": player confirms with Continue; "auto": commit immediately
    int max_turns = 0;            // 0 = unlimited
    bool interject = true;
    int interject_limit = -1;     // -1 = game default
    std::string on_end;           // outcome used when the conversation ends without one (she walks away, turn limit, Leave)
    int min_turns = 1;            // player messages before an outcome may be offered
    std::vector<TalkOutcome> outcomes;
    int after = -1;               // pc to resume at when no outcome body continues elsewhere
};

struct Instr {
    Op op = Op::Notify;
    int line = 0;
    std::string file;
    std::string s1, s2, s3, s4;   // meaning depends on op (see script.cpp)
    double num = -1;              // duration / seconds / fade
    bool flag = false;
    Text text;
    Expr expr, expr2;
    int target = -1;
    std::vector<MenuOpt> options;
    std::vector<std::pair<std::string, Expr>> args;   // minigame arguments
    std::shared_ptr<TalkSpec> talk;
};

struct SourceFile { std::string name, text; };

struct Program {
    std::vector<Instr> code;
    std::map<std::string, int> labels;
    std::vector<std::pair<int, std::string>> label_starts;   // sorted by pc
    // Things scripts refer to; the game validator checks them against real assets.
    struct Refs {
        std::set<std::string> backgrounds, characters, music, sounds, minigames, transitions, motions, positions;
        std::set<std::pair<std::string, std::string>> poses;   // (character, pose)
        std::set<std::string> speakers;
    } refs;
    uint64_t hash = 0;

    static Program compile(const std::vector<SourceFile>& files);   // throws Error("file:line: message")
    std::pair<std::string, int> locate(int pc) const;                // pc -> (label, offset)
    int resolve(const std::string& label, int offset) const;         // inverse; -1 if the label is gone
};

} // namespace sf
