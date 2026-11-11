#include "vm.hpp"

namespace sf {

Vm::Vm(std::shared_ptr<const Program> p, uint64_t seed) : prog_(std::move(p)), rng_(seed) {
    for (auto& ls : prog_->label_starts) label_at_[ls.first] = ls.second;
}

EvalContext Vm::context() {
    EvalContext c;
    c.vars = &vars_;
    c.rng = &rng_;
    c.seen = [this](const std::string& l) { return seen_.count(l) > 0; };
    return c;
}

std::string Vm::where() const {
    if (pc_ >= 0 && pc_ < static_cast<int>(prog_->code.size())) {
        auto& in = prog_->code[wait_pc_ >= 0 ? wait_pc_ : pc_];
        return in.file + ":" + std::to_string(in.line);
    }
    return "script";
}

void Vm::jump_to(int pc) {
    pc_ = pc;
    auto it = label_at_.find(pc);
    if (it != label_at_.end()) seen_.insert(it->second);
}

void Vm::start(const std::string& label) {
    auto it = prog_->labels.find(label);
    if (it == prog_->labels.end()) throw Error("unknown start label '" + label + "'");
    stack_.clear(); wait_kind_.clear(); wait_pc_ = -1; finished_ = false;
    jump_to(it->second);
}

void Vm::block_on(const std::string& kind, Json event, int pc, Json data) {
    wait_kind_ = kind; wait_event_ = std::move(event); wait_pc_ = pc; wait_data_ = std::move(data);
}

void Vm::need(const char* kind) const {
    if (wait_kind_ != kind) throw Error(std::string("the game is not waiting for ") + kind + " (waiting for '" + wait_kind_ + "')");
}

std::vector<Event> Vm::run() {
    std::vector<Event> out;
    if (waiting() || finished_) return out;
    auto ev = [&](const char* t, Json d) { out.push_back({t, std::move(d)}); };
    EvalContext cx = context();
    int budget = 200000;
    while (true) {
        if (pc_ < 0 || pc_ >= static_cast<int>(prog_->code.size())) throw Error("script ran past its end");
        if (--budget < 0) throw Error(where() + ": script ran too long without waiting for the player (infinite loop?)");
        const Instr& in = prog_->code[pc_];
        try {
            switch (in.op) {
                case Op::Say: {
                    Json d = Json::object();
                    if (!in.s2.empty()) { Json p = Json::object(); p.set("char", in.s1); p.set("pose", in.s2); ev("pose", p); }
                    std::string who = in.s1;
                    if (!who.empty() && who[0] == '\x01') { d.set("speaker", ""); d.set("name", who.substr(1)); }
                    else d.set("speaker", who == "narrator" ? "" : who);
                    d.set("text", render_text(in.text, cx));
                    if (!in.s2.empty()) d.set("pose", in.s2);
                    block_on("say", d, pc_);
                    ++pc_;
                    ev("say", d);
                    return out;
                }
                case Op::Scene: { Json d = Json::object(); d.set("bg", in.s1); ev("scene", d); if (!in.s2.empty()) { Json w = Json::object(); w.set("transition", in.s2); w.set("duration", in.num); ev("with", w); } ++pc_; break; }
                case Op::Show: {
                    Json d = Json::object(); d.set("char", in.s1);
                    if (!in.s2.empty()) d.set("pose", in.s2);
                    if (!in.s3.empty()) d.set("at", in.s3);
                    ev("show", d);
                    if (!in.s4.empty()) { Json w = Json::object(); w.set("transition", in.s4); w.set("duration", in.num); ev("with", w); }
                    ++pc_; break;
                }
                case Op::Hide: {
                    Json d = Json::object(); d.set("char", in.s1); ev("hide", d);
                    if (!in.s2.empty()) { Json w = Json::object(); w.set("transition", in.s2); w.set("duration", in.num); ev("with", w); }
                    ++pc_; break;
                }
                case Op::Anim: { Json d = Json::object(); d.set("char", in.s1); d.set("motion", in.s2); ev("anim", d); ++pc_; break; }
                case Op::With: { Json d = Json::object(); d.set("transition", in.s1); d.set("duration", in.num); ev("with", d); ++pc_; break; }
                case Op::Music: { Json d = Json::object(); d.set("track", in.s1); d.set("fade", in.num); d.set("loop", in.flag); ev("music", d); ++pc_; break; }
                case Op::StopMusic: { Json d = Json::object(); d.set("fade", in.num); ev("stop_music", d); ++pc_; break; }
                case Op::Sound: { Json d = Json::object(); d.set("sound", in.s1); ev("sound", d); ++pc_; break; }
                case Op::Notify: { Json d = Json::object(); d.set("text", render_text(in.text, cx)); ev("notify", d); ++pc_; break; }
                case Op::Checkpoint: ev("checkpoint", Json::object()); ++pc_; break;
                case Op::Pause: {
                    Json d = Json::object(); d.set("seconds", in.num);
                    block_on("pause", d, pc_); ++pc_; ev("pause", d); return out;
                }
                case Op::Set: {
                    Json v = eval(in.expr2, cx);
                    if (in.s1 != "=") v = apply_op(in.s1, eval(in.expr, cx), v);
                    assign(in.expr, cx, std::move(v));
                    ++pc_; break;
                }
                case Op::Jump: jump_to(in.target); break;
                case Op::JumpIfNot: if (truthy(eval(in.expr, cx))) ++pc_; else pc_ = in.target; break;
                case Op::Call:
                    if (stack_.size() >= 256) throw Error("call stack too deep (recursive call?)");
                    stack_.push_back(pc_ + 1);
                    jump_to(in.target);
                    break;
                case Op::Return:
                    if (stack_.empty()) throw Error("'return' with nothing to return to (end a story branch with 'jump' or 'end')");
                    pc_ = stack_.back(); stack_.pop_back();
                    break;
                case Op::Menu: {
                    Json d = Json::object(), opts = Json::array(), map = Json::array();
                    int shown = 0;
                    for (size_t k = 0; k < in.options.size(); ++k) {
                        auto& o = in.options[k];
                        if (o.cond && !truthy(eval(o.cond, cx))) continue;
                        Json oj = Json::object();
                        oj.set("index", shown++);
                        oj.set("text", render_text(o.text, cx));
                        opts.push(oj);
                        map.push(static_cast<int>(k));
                    }
                    if (shown == 0) throw Error("every menu option is hidden by its condition");
                    d.set("options", opts);
                    if (!in.text.empty()) { d.set("caption", render_text(in.text, cx)); d.set("speaker", in.s1 == "narrator" ? "" : in.s1); }
                    block_on("menu", d, pc_, map);
                    ++pc_;
                    ev("menu", d);
                    return out;
                }
                case Op::Talk: {
                    auto& sp = *in.talk;
                    Json d = Json::object(), cast = Json::array(), outs = Json::array(), ids = Json::array();
                    for (auto& c : sp.cast) cast.push(c);
                    for (auto& o : sp.outcomes) {
                        if (o.when && !truthy(eval(o.when, cx))) continue;
                        Json oj = Json::object();
                        oj.set("id", o.id); oj.set("label", render_text(o.label, cx)); oj.set("rule", o.rule);
                        outs.push(oj); ids.push(o.id);
                    }
                    if (outs.size() == 0) throw Error("every outcome of this talk is hidden by its 'when' condition");
                    d.set("cast", cast);
                    d.set("start", sp.start.empty() ? sp.cast[0] : sp.start);
                    d.set("goal", render_text(sp.goal, cx));
                    d.set("commit", sp.commit);
                    d.set("max_turns", sp.max_turns);
                    d.set("interject", sp.interject);
                    d.set("interject_limit", sp.interject_limit);
                    d.set("on_end", sp.on_end);
                    d.set("min_turns", sp.min_turns);
                    d.set("outcomes", outs);
                    block_on("talk", d, pc_, ids);
                    ++pc_;
                    ev("talk", d);
                    return out;
                }
                case Op::Minigame: {
                    Json d = Json::object(), args = Json::object();
                    for (auto& a : in.args) args.set(a.first, eval(a.second, cx));
                    d.set("id", in.s1); d.set("args", args); d.set("result_var", in.s2);
                    block_on("minigame", d, pc_);
                    ++pc_;
                    ev("minigame", d);
                    return out;
                }
                case Op::End: {
                    Json d = Json::object(); d.set("ending", in.s1);
                    finished_ = true;
                    ev("end", d);
                    return out;
                }
            }
        } catch (const Error& e) {
            std::string m = e.what();
            if (m.find(".sf:") != std::string::npos) throw;
            throw Error(in.file + ":" + std::to_string(in.line) + ": " + m);
        }
    }
}

void Vm::advance() {
    if (wait_kind_ != "say" && wait_kind_ != "pause") need("say or pause");
    wait_kind_.clear(); wait_pc_ = -1;
}
void Vm::choose(int idx) {
    need("menu");
    if (idx < 0 || idx >= static_cast<int>(wait_data_.size())) throw Error("menu choice out of range");
    int opt = static_cast<int>(wait_data_.at(idx).as_int());
    int target = prog_->code[wait_pc_].options[opt].target;
    wait_kind_.clear(); wait_pc_ = -1;
    jump_to(target);   // option bodies are reached directly; they end in a jump past the menu
}
void Vm::finish_talk(const std::string& outcome) {
    need("talk");
    const Instr& in = prog_->code[wait_pc_];
    int target = in.talk->after;
    if (!outcome.empty()) {
        bool allowed = false;
        for (auto& id : wait_data_.items()) if (id.as_str() == outcome) allowed = true;
        if (!allowed) throw Error("outcome '" + outcome + "' is not available in this talk");
        for (auto& o : in.talk->outcomes) if (o.id == outcome) target = o.target;
    }
    wait_kind_.clear(); wait_pc_ = -1;
    jump_to(target);
}
void Vm::finish_minigame(Json result) {
    need("minigame");
    const std::string var = wait_event_["result_var"].as_str();
    wait_kind_.clear(); wait_pc_ = -1;
    if (!var.empty()) vars_.set(var, std::move(result));
}

Json Vm::build_pos(int pc) const {
    auto p = prog_->locate(pc);
    Json j = Json::object();
    j.set("label", p.first); j.set("offset", p.second);
    return j;
}
int Vm::read_pos(const Json& j) const {
    int pc = prog_->resolve(j["label"].as_str(), static_cast<int>(j["offset"].as_int()));
    return pc;
}

Json Vm::save() const {
    Json j = Json::object();
    j.set("program", std::to_string(prog_->hash));
    j.set("vars", vars_);
    j.set("rng", std::to_string(rng_));
    j.set("finished", finished_);
    j.set("pos", build_pos(pc_));
    Json st = Json::array();
    for (int r : stack_) st.push(build_pos(r));
    j.set("stack", st);
    Json sn = Json::array();
    for (auto& s : seen_) sn.push(s);
    j.set("seen", sn);
    if (!wait_kind_.empty()) {
        Json w = Json::object();
        w.set("kind", wait_kind_); w.set("event", wait_event_); w.set("data", wait_data_);
        w.set("pc", build_pos(wait_pc_));
        j.set("wait", w);
    }
    return j;
}

bool Vm::load(const Json& j) {
    bool exact = j["program"].as_str() == std::to_string(prog_->hash);
    vars_ = j["vars"].is_object() ? j["vars"] : Json::object();
    rng_ = std::strtoull(j["rng"].as_str().c_str(), nullptr, 10);
    finished_ = j["finished"].as_bool();
    int pc = read_pos(j["pos"]);
    if (pc < 0) throw Error("this save refers to a scene that no longer exists: " + j["pos"]["label"].as_str());
    pc_ = pc;
    stack_.clear();
    for (auto& r : j["stack"].items()) { int p = read_pos(r); if (p >= 0) stack_.push_back(p); }
    seen_.clear();
    for (auto& s : j["seen"].items()) seen_.insert(s.as_str());
    wait_kind_.clear(); wait_pc_ = -1; wait_event_ = Json(); wait_data_ = Json();
    if (j.has("wait")) {
        auto& w = j["wait"];
        wait_kind_ = w["kind"].as_str(); wait_event_ = w["event"]; wait_data_ = w["data"];
        wait_pc_ = read_pos(w["pc"]);
        if (wait_pc_ < 0) throw Error("this save is waiting inside a scene that no longer exists");
        if (!exact && (wait_kind_ == "menu" || wait_kind_ == "talk")) {
            // The options may have moved; the stored event still describes what the player was shown.
            const Instr& in = prog_->code[wait_pc_];
            bool ok = (wait_kind_ == "menu" && in.op == Op::Menu) || (wait_kind_ == "talk" && in.op == Op::Talk);
            if (!ok) { wait_kind_.clear(); wait_pc_ = -1; }
        }
    }
    return exact;
}

} // namespace sf
