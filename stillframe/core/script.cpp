#include "script.hpp"
#include <algorithm>
#include <cctype>

namespace sf {

// ------------------------------------------------------------------ text
Text parse_text(const std::string& raw) {
    Text out;
    std::string cur;
    auto flush = [&] { if (!cur.empty()) { out.push_back({false, cur, nullptr}); cur.clear(); } };
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '[') {
            if (i + 1 < raw.size() && raw[i + 1] == '[') { cur += '['; ++i; continue; }
            int depth = 1;
            size_t j = i + 1;
            char q = 0;
            for (; j < raw.size() && depth > 0; ++j) {
                if (q) { if (raw[j] == q) q = 0; continue; }
                if (raw[j] == '"' || raw[j] == '\'') q = raw[j];
                else if (raw[j] == '[') ++depth;
                else if (raw[j] == ']') --depth;
            }
            if (depth != 0) throw Error("unterminated [ in text (write [[ for a literal bracket)");
            std::string inner = raw.substr(i + 1, j - i - 2);
            flush();
            out.push_back({true, inner, parse_expr(inner)});
            i = j - 1;
        } else cur += c;
    }
    flush();
    return out;
}

std::string render_text(const Text& t, EvalContext& ctx) {
    std::string s;
    for (auto& seg : t) {
        if (!seg.is_expr) { s += seg.text; continue; }
        Json v = eval(seg.expr, ctx);
        if (v.is_string()) s += v.as_str();
        else if (v.is_int()) s += std::to_string(v.as_int());
        else if (!v.is_null()) s += v.dump();
    }
    return s;
}

namespace {

struct Line { int indent; std::string text; int no; };
struct Tk { enum K { Word, Str, Sym, End } k; std::string s; };

const std::set<std::string> kTransitions = {"none", "fade", "dissolve", "slide_left", "slide_right", "wipe_left", "wipe_right", "flash", "cut"};

struct Compiler {
    explicit Compiler(Program& p) : prog(p) {}
    Program& prog;
    std::string file;
    std::vector<Line> lines;
    size_t i = 0;
    struct Pending { int pc; std::string label; int line; std::string file; };
    std::vector<Pending> pending;

    [[noreturn]] void fail(int no, const std::string& m) const { throw Error(file + ":" + std::to_string(no) + ": " + m); }

    static std::string strip_comment(const std::string& s) {
        char q = 0;
        for (size_t k = 0; k < s.size(); ++k) {
            char c = s[k];
            if (q) { if (c == '\\') ++k; else if (c == q) q = 0; }
            else if (c == '"' || c == '\'') {
                // apostrophes inside bare words (don't) are not quotes
                if (c == '\'' && k > 0 && std::isalpha(static_cast<unsigned char>(s[k - 1]))) continue;
                q = c;
            } else if (c == '#') return s.substr(0, k);
        }
        return s;
    }

    void load(const SourceFile& f) {
        file = f.name;
        lines.clear(); i = 0;
        int no = 0;
        size_t pos = 0;
        const std::string& t = f.text;
        while (pos <= t.size()) {
            size_t e = t.find('\n', pos);
            if (e == std::string::npos) e = t.size();
            std::string raw = t.substr(pos, e - pos);
            pos = e + 1;
            ++no;
            if (no == 1 && raw.size() >= 3 && static_cast<uint8_t>(raw[0]) == 0xEF) raw = raw.substr(3);
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            int indent = 0; size_t k = 0;
            for (; k < raw.size() && (raw[k] == ' ' || raw[k] == '\t'); ++k) indent = raw[k] == '\t' ? (indent / 4 + 1) * 4 : indent + 1;
            std::string body = trim(strip_comment(raw.substr(k)));
            if (body.empty()) continue;
            lines.push_back({indent, body, no});
        }
    }

    // ---- statement tokenizer
    std::vector<Tk> lex(const Line& ln, const std::string& s) {
        std::vector<Tk> out;
        size_t k = 0;
        while (k < s.size()) {
            char c = s[k];
            if (c == ' ' || c == '\t') { ++k; continue; }
            if (c == '"') {
                std::string v; ++k;
                bool closed = false;
                while (k < s.size()) {
                    char d = s[k++];
                    if (d == '"') { closed = true; break; }
                    if (d == '\\' && k < s.size()) { char e = s[k++]; v += e == 'n' ? '\n' : e == 't' ? '\t' : e; }
                    else v += d;
                }
                if (!closed) fail(ln.no, "unterminated string");
                out.push_back({Tk::Str, v});
            } else if (c == '-' && k + 1 < s.size() && s[k + 1] == '>') { out.push_back({Tk::Sym, "->"}); k += 2; }
            else if (c == ':' || c == ',' || c == '(' || c == ')' || c == '=') { out.push_back({Tk::Sym, std::string(1, c)}); ++k; }
            else {
                size_t st = k;
                while (k < s.size() && !std::isspace(static_cast<unsigned char>(s[k])) && s[k] != '"' && s[k] != ':' && s[k] != ',' && s[k] != '(' && s[k] != ')' && s[k] != '=' && !(s[k] == '-' && k + 1 < s.size() && s[k + 1] == '>')) ++k;
                out.push_back({Tk::Word, s.substr(st, k - st)});
            }
        }
        out.push_back({Tk::End, ""});
        return out;
    }

    int emit(Instr in, const Line& ln) {
        in.line = ln.no; in.file = file;
        prog.code.push_back(std::move(in));
        return static_cast<int>(prog.code.size()) - 1;
    }
    int here() const { return static_cast<int>(prog.code.size()); }

    Expr expr_of(const Line& ln, const std::string& text) {
        try { return parse_expr(text); }
        catch (const Error& e) { fail(ln.no, e.what()); }
    }
    Text text_of(const Line& ln, const std::string& raw) {
        try { return parse_text(raw); }
        catch (const Error& e) { fail(ln.no, e.what()); }
    }

    static bool ident_ok(const std::string& w) {
        if (w.empty() || !(std::isalpha(static_cast<unsigned char>(w[0])) || w[0] == '_')) return false;
        for (char c : w) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
        return true;
    }

    // Parses `with <transition> [seconds]` at tokens[k...]; returns transition name and sets duration.
    void parse_with(const Line& ln, const std::vector<Tk>& t, size_t& k, std::string& trans, double& dur) {
        if (!(t[k].k == Tk::Word && t[k].s == "with")) return;
        ++k;
        if (t[k].k != Tk::Word) fail(ln.no, "expected a transition name after 'with'");
        trans = t[k++].s;
        if (!kTransitions.count(trans)) fail(ln.no, "unknown transition '" + trans + "' (try: fade, dissolve, slide_left, slide_right, wipe_left, wipe_right, flash, none)");
        prog.refs.transitions.insert(trans);
        if (t[k].k == Tk::Word && (std::isdigit(static_cast<unsigned char>(t[k].s[0])) || t[k].s[0] == '.')) dur = std::atof(t[k++].s.c_str());
    }

    void end_of(const Line& ln, const std::vector<Tk>& t, size_t k) {
        if (t[k].k != Tk::End) fail(ln.no, "unexpected '" + t[k].s + "' at end of statement");
    }

    // ---- block parsing
    void block(int parent_indent) {
        if (i >= lines.size() || lines[i].indent <= parent_indent) fail(i < lines.size() ? lines[i].no : (lines.empty() ? 1 : lines.back().no), "expected an indented block");
        int indent = lines[i].indent;
        while (i < lines.size() && lines[i].indent >= indent) {
            if (lines[i].indent > indent) fail(lines[i].no, "unexpected indentation");
            statement(indent);
        }
    }

    void statement(int indent) {
        Line ln = lines[i];
        const std::string& s = ln.text;
        if (s[0] == '$') { ++i; assignment(ln, trim(s.substr(1))); return; }
        std::vector<Tk> t = lex(ln, s);
        if (t[0].k == Tk::Word) {
            const std::string& w = t[0].s;
            if (w == "label") fail(ln.no, "labels cannot be nested inside other blocks");
            if (w == "if") { if_chain(ln, indent); return; }
            if (w == "elif" || w == "else") fail(ln.no, "'" + w + "' without a matching 'if'");
            if (w == "menu") { ++i; menu(ln, t, indent); return; }
            if (w == "talk") { ++i; talk(ln, t, indent); return; }
            if (w == "jump" || w == "call") {
                ++i;
                if (t[1].k != Tk::Word) fail(ln.no, w + " needs a label name");
                end_of(ln, t, 2);
                Instr in; in.op = w == "jump" ? Op::Jump : Op::Call; in.s1 = t[1].s;
                pending.push_back({emit(in, ln), t[1].s, ln.no, file});
                return;
            }
            if (w == "return") { ++i; end_of(ln, t, 1); Instr in; in.op = Op::Return; emit(in, ln); return; }
            if (w == "scene") {
                ++i;
                if (t[1].k != Tk::Word) fail(ln.no, "scene needs a background id");
                Instr in; in.op = Op::Scene; in.s1 = t[1].s; size_t k = 2;
                parse_with(ln, t, k, in.s2, in.num); end_of(ln, t, k);
                prog.refs.backgrounds.insert(in.s1);
                emit(in, ln); return;
            }
            if (w == "show") {
                ++i;
                if (t[1].k != Tk::Word) fail(ln.no, "show needs a character id");
                Instr in; in.op = Op::Show; in.s1 = t[1].s; size_t k = 2;
                if (t[k].k == Tk::Word && t[k].s != "at" && t[k].s != "with") in.s2 = t[k++].s;
                if (t[k].k == Tk::Word && t[k].s == "at") {
                    ++k;
                    if (t[k].k != Tk::Word) fail(ln.no, "expected a position after 'at'");
                    in.s3 = t[k++].s;
                    prog.refs.positions.insert(in.s3);
                }
                parse_with(ln, t, k, in.s4, in.num); end_of(ln, t, k);
                prog.refs.characters.insert(in.s1);
                if (!in.s2.empty()) prog.refs.poses.insert({in.s1, in.s2});
                emit(in, ln); return;
            }
            if (w == "hide") {
                ++i;
                if (t[1].k != Tk::Word) fail(ln.no, "hide needs a character id");
                Instr in; in.op = Op::Hide; in.s1 = t[1].s; size_t k = 2;
                parse_with(ln, t, k, in.s2, in.num); end_of(ln, t, k);
                prog.refs.characters.insert(in.s1);
                emit(in, ln); return;
            }
            if (w == "anim") {
                ++i;
                if (t[1].k != Tk::Word || t[2].k != Tk::Word) fail(ln.no, "usage: anim <character> <motion>");
                end_of(ln, t, 3);
                Instr in; in.op = Op::Anim; in.s1 = t[1].s; in.s2 = t[2].s;
                prog.refs.characters.insert(in.s1); prog.refs.motions.insert(in.s2);
                emit(in, ln); return;
            }
            if (w == "with") {
                ++i;
                size_t k = 0; Instr in; in.op = Op::With;
                parse_with(ln, t, k, in.s1, in.num); end_of(ln, t, k);
                emit(in, ln); return;
            }
            if (w == "play" || w == "stop") {
                ++i;
                if (t[1].k != Tk::Word || (t[1].s != "music" && !(w == "play" && t[1].s == "sound"))) fail(ln.no, w == "play" ? "usage: play music|sound \"file\"" : "usage: stop music [fade <seconds>]");
                Instr in; size_t k = 2;
                if (w == "play") {
                    in.op = t[1].s == "music" ? Op::Music : Op::Sound;
                    if (t[k].k != Tk::Str && t[k].k != Tk::Word) fail(ln.no, "play needs a file path or audio id");
                    in.s1 = t[k++].s;
                    in.flag = in.op == Op::Music;   // loop by default
                    (in.op == Op::Music ? prog.refs.music : prog.refs.sounds).insert(in.s1);
                } else in.op = Op::StopMusic;
                in.num = 0;
                while (t[k].k == Tk::Word) {
                    if (t[k].s == "fade" && t[k + 1].k == Tk::Word) { in.num = std::atof(t[k + 1].s.c_str()); k += 2; }
                    else if (t[k].s == "once") { in.flag = false; ++k; }
                    else if (t[k].s == "loop") { in.flag = true; ++k; }
                    else fail(ln.no, "unexpected '" + t[k].s + "'");
                }
                end_of(ln, t, k);
                emit(in, ln); return;
            }
            if (w == "pause") {
                ++i;
                Instr in; in.op = Op::Pause; in.num = -1;
                if (t[1].k == Tk::Word) { in.num = std::atof(t[1].s.c_str()); end_of(ln, t, 2); } else end_of(ln, t, 1);
                emit(in, ln); return;
            }
            if (w == "minigame") { ++i; minigame(ln, s.substr(8)); return; }
            if (w == "end") {
                ++i;
                Instr in; in.op = Op::End;
                if (t[1].k == Tk::Str || t[1].k == Tk::Word) { in.s1 = t[1].s; end_of(ln, t, 2); } else end_of(ln, t, 1);
                emit(in, ln); return;
            }
            if (w == "notify") {
                ++i;
                if (t[1].k != Tk::Str) fail(ln.no, "usage: notify \"text\"");
                end_of(ln, t, 2);
                Instr in; in.op = Op::Notify; in.text = text_of(ln, t[1].s); emit(in, ln); return;
            }
            if (w == "checkpoint") { ++i; end_of(ln, t, 1); Instr in; in.op = Op::Checkpoint; emit(in, ln); return; }
        }
        // dialogue:  [speaker] [pose] "text"
        ++i;
        Instr in; in.op = Op::Say;
        size_t k = 0;
        if (t[k].k == Tk::Word) {
            in.s1 = t[k++].s;
            if (t[k].k == Tk::Word) in.s2 = t[k++].s;
        } else if (t[k].k == Tk::Str && t[k + 1].k == Tk::Str) {
            in.s1 = "\x01" + t[k++].s;   // ad-hoc speaker name (not a character)
        }
        if (t[k].k != Tk::Str) fail(ln.no, "I don't understand this line (expected a statement or dialogue like: hana \"Hello.\")");
        in.text = text_of(ln, t[k++].s);
        end_of(ln, t, k);
        if (!in.s1.empty() && in.s1[0] != '\x01' && in.s1 != "narrator") {
            prog.refs.speakers.insert(in.s1);
            if (!ident_ok(in.s1)) fail(ln.no, "'" + in.s1 + "' is not a valid character id");
            if (!in.s2.empty()) prog.refs.poses.insert({in.s1, in.s2});
        }
        emit(in, ln);
    }

    void assignment(const Line& ln, const std::string& body) {
        // find the assignment operator at top level (not inside quotes/brackets, not ==, <=, >=, !=)
        int depth = 0; char q = 0;
        size_t op_at = std::string::npos, op_len = 0;
        for (size_t k = 0; k < body.size(); ++k) {
            char c = body[k];
            if (q) { if (c == '\\') ++k; else if (c == q) q = 0; continue; }
            if (c == '"' || c == '\'') { q = c; continue; }
            if (c == '(' || c == '[') ++depth;
            else if (c == ')' || c == ']') --depth;
            else if (depth == 0 && c == '=') {
                bool prev_cmp = k > 0 && (body[k - 1] == '=' || body[k - 1] == '!' || body[k - 1] == '<' || body[k - 1] == '>');
                bool next_eq = k + 1 < body.size() && body[k + 1] == '=';
                if (next_eq) { ++k; continue; }
                if (prev_cmp) continue;
                bool compound = k > 0 && std::string("+-*/%").find(body[k - 1]) != std::string::npos;
                op_at = compound ? k - 1 : k; op_len = compound ? 2 : 1;
                break;
            }
        }
        if (op_at == std::string::npos) fail(ln.no, "expected an assignment like: $ trust += 1");
        Instr in; in.op = Op::Set;
        in.expr = expr_of(ln, trim(body.substr(0, op_at)));
        if (!is_lvalue(in.expr)) fail(ln.no, "cannot assign to '" + trim(body.substr(0, op_at)) + "'");
        in.s1 = body.substr(op_at, op_len) == "=" ? "=" : body.substr(op_at, 1);
        in.expr2 = expr_of(ln, trim(body.substr(op_at + op_len)));
        emit(in, ln);
    }

    std::string cond_text(const Line& ln, const std::string& kw) {
        std::string rest = trim(ln.text.substr(kw.size()));
        if (rest.empty() || rest.back() != ':') fail(ln.no, "'" + kw + "' needs a condition and a trailing ':'");
        rest.pop_back();
        rest = trim(rest);
        if (rest.empty()) fail(ln.no, "'" + kw + "' needs a condition");
        return rest;
    }

    void if_chain(const Line& first, int indent) {
        std::vector<int> ends;
        Line ln = first;
        std::string kw = "if";
        while (true) {
            ++i;
            Instr j; j.op = Op::JumpIfNot; j.expr = expr_of(ln, cond_text(ln, kw));
            int jpc = emit(j, ln);
            block(indent);
            if (i < lines.size() && lines[i].indent == indent && (lines[i].text.rfind("elif ", 0) == 0 || lines[i].text == "else:" || lines[i].text.rfind("else :", 0) == 0)) {
                Instr jmp; jmp.op = Op::Jump; ends.push_back(emit(jmp, lines[i]));
                prog.code[jpc].target = here();
                ln = lines[i];
                if (ln.text.rfind("elif ", 0) == 0) { kw = "elif"; continue; }
                ++i;
                block(indent);
                break;
            }
            prog.code[jpc].target = here();
            break;
        }
        for (int e : ends) prog.code[e].target = here();
    }

    void menu(const Line& ln, const std::vector<Tk>& t, int indent) {
        if (!(t[1].k == Tk::Sym && t[1].s == ":")) fail(ln.no, "usage: menu:");
        if (i >= lines.size() || lines[i].indent <= indent) fail(ln.no, "menu needs options");
        int mi = lines[i].indent;
        Instr in; in.op = Op::Menu;
        int mpc = here();
        emit(in, ln);
        std::vector<int> ends;
        std::vector<MenuOpt> opts;
        std::vector<std::pair<int, int>> bodies;  // option index -> (unused)
        while (i < lines.size() && lines[i].indent == mi) {
            Line ol = lines[i];
            std::vector<Tk> ot = lex(ol, ol.text);
            bool opt = ol.text.back() == ':' && ot[0].k == Tk::Str;
            if (!opt) {
                // caption: "text"  or  speaker "text"
                if (!opts.empty()) fail(ol.no, "captions must come before the options");
                ++i;
                size_t k = 0;
                if (ot[0].k == Tk::Word) { prog.code[mpc].s1 = ot[0].s; ++k; }
                if (ot[k].k != Tk::Str) fail(ol.no, "expected a menu option like: \"Say hello\":");
                prog.code[mpc].text = text_of(ol, ot[k].s);
                end_of(ol, ot, k + 1);
                continue;
            }
            ++i;
            MenuOpt mo; mo.text = text_of(ol, ot[0].s);
            size_t k = 1;
            if (ot[k].k == Tk::Word && ot[k].s == "if") {
                size_t q = 1;
                while (q < ol.text.size() && !(ol.text[q] == '"' && ol.text[q - 1] != '\\')) ++q;
                std::string after = trim(ol.text.substr(std::min(q + 1, ol.text.size())));
                after = trim(after.substr(2));
                if (after.empty() || after.back() != ':') fail(ol.no, "menu option needs a trailing ':'");
                after.pop_back();
                mo.cond = expr_of(ol, trim(after));
            } else if (!(ot[k].k == Tk::Sym && ot[k].s == ":" && ot[k + 1].k == Tk::End)) fail(ol.no, "menu option should look like: \"Text\": or \"Text\" if condition:");
            mo.target = here();
            opts.push_back(mo);
            block(mi);
            Instr jmp; jmp.op = Op::Jump; ends.push_back(emit(jmp, ol));
        }
        if (opts.empty()) fail(ln.no, "menu needs at least one option");
        prog.code[mpc].options = opts;
        for (int e : ends) prog.code[e].target = here();
    }

    static std::string unquote(std::string v) {
        v = trim(v);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        return v;
    }

    void talk(const Line& ln, const std::vector<Tk>& t, int indent) {
        // talk with a, b, c:
        size_t k = 1;
        if (!(t[k].k == Tk::Word && t[k].s == "with")) fail(ln.no, "usage: talk with <character>[, <character>...]:");
        ++k;
        auto spec = std::make_shared<TalkSpec>();
        while (true) {
            if (t[k].k != Tk::Word) fail(ln.no, "expected a character id");
            spec->cast.push_back(t[k++].s);
            prog.refs.characters.insert(spec->cast.back());
            if (t[k].k == Tk::Sym && t[k].s == ",") { ++k; continue; }
            break;
        }
        if (!(t[k].k == Tk::Sym && t[k].s == ":") || t[k + 1].k != Tk::End) fail(ln.no, "talk header must end with ':'");
        if (i >= lines.size() || lines[i].indent <= indent) fail(ln.no, "talk needs a body");
        int ti = lines[i].indent;
        Instr in; in.op = Op::Talk; in.talk = spec;
        int tpc = emit(in, ln);
        std::vector<int> ends;
        while (i < lines.size() && lines[i].indent == ti) {
            Line bl = lines[i];
            if (bl.text.rfind("outcome ", 0) == 0) {
                ++i;
                std::vector<Tk> ot = lex(bl, bl.text);
                if (ot[1].k != Tk::Word) fail(bl.no, "usage: outcome <id> \"Label\":");
                TalkOutcome oc; oc.id = ot[1].s;
                size_t q = 2;
                std::string label = oc.id;
                if (ot[q].k == Tk::Str) label = ot[q++].s;
                oc.label = text_of(bl, label);
                if (!(ot[q].k == Tk::Sym && ot[q].s == ":") || ot[q + 1].k != Tk::End) fail(bl.no, "outcome header must end with ':'");
                for (auto& o : spec->outcomes) if (o.id == oc.id) fail(bl.no, "duplicate outcome '" + oc.id + "'");
                oc.target = here();
                // optional leading `rule:` / `when:` lines
                if (i >= lines.size() || lines[i].indent <= ti) fail(bl.no, "outcome needs a body (at least a rule:)");
                int oi = lines[i].indent;
                while (i < lines.size() && lines[i].indent == oi) {
                    const std::string& tx = lines[i].text;
                    if (tx.rfind("rule:", 0) == 0) { oc.rule = unquote(tx.substr(5)); ++i; }
                    else if (tx.rfind("when:", 0) == 0) { oc.when = expr_of(lines[i], trim(tx.substr(5))); ++i; }
                    else break;
                }
                if (oc.rule.empty()) fail(bl.no, "outcome '" + oc.id + "' needs a rule: line describing when it is satisfied");
                while (i < lines.size() && lines[i].indent >= oi) {
                    if (lines[i].indent > oi) fail(lines[i].no, "unexpected indentation");
                    statement(oi);
                }
                Instr jmp; jmp.op = Op::Jump; ends.push_back(emit(jmp, bl));
                spec->outcomes.push_back(std::move(oc));
                continue;
            }
            auto colon = bl.text.find(':');
            if (colon == std::string::npos) fail(bl.no, "inside talk, expected 'goal:', 'commit:', 'start:', 'max_turns:', 'min_turns:', 'on_end:', 'interject:', 'interject_limit:' or an 'outcome'");
            std::string key = trim(bl.text.substr(0, colon)), val = trim(bl.text.substr(colon + 1));
            ++i;
            if (key == "goal") spec->goal = text_of(bl, unquote(val));
            else if (key == "commit") { val = unquote(val); if (val != "ask" && val != "auto") fail(bl.no, "commit must be ask or auto"); spec->commit = val; }
            else if (key == "start") spec->start = unquote(val);
            else if (key == "max_turns") spec->max_turns = std::atoi(val.c_str());
            else if (key == "interject") { val = unquote(val); if (val != "on" && val != "off") fail(bl.no, "interject must be on or off"); spec->interject = val == "on"; }
            else if (key == "interject_limit") spec->interject_limit = std::atoi(val.c_str());
            else if (key == "on_end") spec->on_end = unquote(val);
            else if (key == "min_turns") spec->min_turns = std::max(0, std::atoi(val.c_str()));
            else fail(bl.no, "unknown talk option '" + key + "'");
        }
        if (spec->outcomes.empty()) fail(ln.no, "talk needs at least one outcome");
        if (!spec->on_end.empty()) {
            bool found = false;
            for (auto& o : spec->outcomes) if (o.id == spec->on_end) found = true;
            if (!found) fail(ln.no, "on_end refers to unknown outcome '" + spec->on_end + "'");
        }
        if (!spec->start.empty() && std::find(spec->cast.begin(), spec->cast.end(), spec->start) == spec->cast.end()) fail(ln.no, "start character is not part of this talk");
        spec->after = here();
        for (int e : ends) prog.code[e].target = spec->after;
        (void)tpc;
    }

    void minigame(const Line& ln, const std::string& rest) {
        // minigame cooking(order="soup", time=60) -> result
        std::string r = trim(rest);
        size_t k = 0;
        while (k < r.size() && (std::isalnum(static_cast<unsigned char>(r[k])) || r[k] == '_' || r[k] == '-')) ++k;
        Instr in; in.op = Op::Minigame; in.s1 = r.substr(0, k);
        if (in.s1.empty()) fail(ln.no, "usage: minigame <id>(name=value, ...) -> result");
        r = trim(r.substr(k));
        if (!r.empty() && r[0] == '(') {
            int depth = 0; char q = 0; size_t close = std::string::npos;
            for (size_t m = 0; m < r.size(); ++m) {
                char c = r[m];
                if (q) { if (c == '\\') ++m; else if (c == q) q = 0; continue; }
                if (c == '"' || c == '\'') q = c;
                else if (c == '(' || c == '[') ++depth;
                else if (c == ')' || c == ']') { if (--depth == 0) { close = m; break; } }
            }
            if (close == std::string::npos) fail(ln.no, "missing ')' in minigame arguments");
            std::string inner = r.substr(1, close - 1);
            r = trim(r.substr(close + 1));
            depth = 0; q = 0; size_t st = 0;
            auto add_arg = [&](std::string a) {
                a = trim(a);
                if (a.empty()) return;
                auto eq = a.find('=');
                if (eq == std::string::npos) fail(ln.no, "minigame arguments look like name=value");
                std::string name = trim(a.substr(0, eq));
                if (!ident_ok(name)) fail(ln.no, "bad argument name '" + name + "'");
                in.args.emplace_back(name, expr_of(ln, trim(a.substr(eq + 1))));
            };
            for (size_t m = 0; m <= inner.size(); ++m) {
                char c = m < inner.size() ? inner[m] : ',';
                if (q) { if (c == '\\') ++m; else if (c == q) q = 0; continue; }
                if (c == '"' || c == '\'') q = c;
                else if (c == '(' || c == '[') ++depth;
                else if (c == ')' || c == ']') --depth;
                else if (c == ',' && depth == 0) { add_arg(inner.substr(st, m - st)); st = m + 1; }
            }
        }
        if (!r.empty()) {
            if (r.rfind("->", 0) != 0) fail(ln.no, "expected '-> variable' after the minigame arguments");
            in.s2 = trim(r.substr(2));
            if (!ident_ok(in.s2)) fail(ln.no, "result variable '" + in.s2 + "' is not a valid name");
        }
        prog.refs.minigames.insert(in.s1);
        emit(in, ln);
    }

    void file_top() {
        while (i < lines.size()) {
            Line ln = lines[i];
            if (ln.indent != 0) fail(ln.no, "unexpected indentation (statements belong inside a label)");
            std::vector<Tk> t = lex(ln, ln.text);
            if (!(t[0].k == Tk::Word && t[0].s == "label")) fail(ln.no, "only 'label name:' can appear at the top level");
            if (t[1].k != Tk::Word || !ident_ok(t[1].s) || !(t[2].k == Tk::Sym && t[2].s == ":") || t[3].k != Tk::End) fail(ln.no, "usage: label <name>:");
            if (prog.labels.count(t[1].s)) fail(ln.no, "label '" + t[1].s + "' is already defined");
            ++i;
            prog.labels[t[1].s] = here();
            prog.label_starts.emplace_back(here(), t[1].s);
            block(0);
            Instr r; r.op = Op::Return; emit(r, ln);   // falling off the end of a label returns
        }
    }
};

uint64_t fnv(uint64_t h, const std::string& s) { for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; } return h; }

} // namespace

Program Program::compile(const std::vector<SourceFile>& files) {
    Program p;
    Compiler c(p);
    for (auto& f : files) {
        c.load(f);
        c.file_top();
        p.hash = fnv(p.hash ? p.hash : 1469598103934665603ull, f.text);
    }
    for (auto& pe : c.pending) {
        auto it = p.labels.find(pe.label);
        if (it == p.labels.end()) throw Error(pe.file + ":" + std::to_string(pe.line) + ": unknown label '" + pe.label + "'");
        p.code[pe.pc].target = it->second;
    }
    std::sort(p.label_starts.begin(), p.label_starts.end());
    return p;
}

std::pair<std::string, int> Program::locate(int pc) const {
    std::pair<std::string, int> r{"", pc};
    for (auto& ls : label_starts) { if (ls.first > pc) break; r = {ls.second, pc - ls.first}; }
    return r;
}

int Program::resolve(const std::string& label, int offset) const {
    auto it = labels.find(label);
    if (it == labels.end()) return -1;
    int pc = it->second + offset;
    return std::min(pc, static_cast<int>(code.size()) - 1);
}

} // namespace sf
