#include "conversation.hpp"
#include <algorithm>

namespace sf {

static const std::vector<std::string>& default_repair_words() {
    static const std::vector<std::string> w = {"sorry", "apolog", "forgive", "my bad", "i was wrong", "i shouldn't have", "i should not have",
                                               "perd\xC3\xB3n", "perdon", "lo siento", "disculp", "me equivoqu"};
    return w;
}

bool offers_repair(const std::string& text, const std::vector<std::string>& words) {
    for (auto& w : (words.empty() ? default_repair_words() : words)) if (contains_ci(text, w)) return true;
    return false;
}

std::string wrap_prompt(const std::string& tmpl, const std::string& system, const std::string& user) {
    if (tmpl == "plain") return "### System\n" + system + "\n\n### User\n" + user + "\n\n### Reply\n";
    return "<|im_start|>system\n" + system + "<|im_end|>\n<|im_start|>user\n" + user + "<|im_end|>\n<|im_start|>assistant\n";
}

ConvOptions ConvOptions::from_game(const Game& g, const Json& talk) {
    ConvOptions o;
    const Json& l = g.llm;
    o.premise = l["system"].str_or("");
    o.tmpl = l["template"].str_or("chatml");
    o.max_tokens = static_cast<int>(l["max_tokens"].as_int(o.max_tokens));
    o.temperature = l["temperature"].as_num(o.temperature);
    o.slots = std::max<int>(1, static_cast<int>(l["slots"].as_int(1)));
    o.interject_limit = static_cast<int>(l["interject_limit"].as_int(o.interject_limit));
    o.interject_cooldown = static_cast<int>(l["interject_cooldown"].as_int(o.interject_cooldown));
    o.language = l["language"].str_or("");
    for (auto& w : l["repair_words"].items()) o.repair_words.push_back(w.as_str());
    if (talk["interject_limit"].as_int(-1) >= 0) o.interject_limit = static_cast<int>(talk["interject_limit"].as_int());
    return o;
}

Conversation::Conversation(const Game& game, MemoryHub& hub, LlmScheduler& sched, const Json& talk, ConvOptions opt)
    : game_(game), hub_(hub), sched_(sched), opt_(std::move(opt)) {
    for (auto& c : talk["cast"].items()) {
        if (!hub_.has(c.as_str())) throw Error("talk cast member '" + c.as_str() + "' has no character definition");
        cast_.push_back(c.as_str());
    }
    if (cast_.empty()) throw Error("talk block without a cast");
    addressee_ = talk["start"].str_or(cast_[0]);
    goal_ = talk["goal"].as_str();
    commit_ = talk["commit"].str_or("ask");
    on_end_ = talk["on_end"].as_str();
    max_turns_ = static_cast<int>(talk["max_turns"].as_int());
    min_turns_ = static_cast<int>(talk["min_turns"].as_int(1));
    interject_allowed_ = talk["interject"].as_bool(true) && cast_.size() > 1;
    limit_ = opt_.interject_limit;
    for (auto& o : talk["outcomes"].items()) outcomes_.push_back({o["id"].as_str(), o["label"].as_str(), o["rule"].as_str()});
}

std::string Conversation::display_name(const std::string& id) const {
    if (id == "player") return opt_.player_name;
    if (auto* c = game_.character(id)) return c->name;
    return id;
}

bool Conversation::busy() const {
    for (auto& kv : jobs_) if (kv.second.kind == Kind::Reply) return true;
    return false;
}

bool Conversation::looks_like_repair(const std::string& text) const { return offers_repair(text, opt_.repair_words); }

// ----------------------------------------------------------------------------- prompt + schema

Json Conversation::schema_for(const std::string& speaker, bool interjection) const {
    auto str = [] { Json s = Json::object(); s.set("type", "string"); return s; };
    auto enum_of = [](const std::vector<std::string>& v) { Json s = Json::object(), a = Json::array(); for (auto& x : v) a.push(x); s.set("enum", a); return s; };
    Json props = Json::object();
    if (interjection) {
        Json b = Json::object(); b.set("type", "boolean");
        props.set("speak", b);
    }
    props.set("reply", str());
    if (auto* c = game_.character(speaker)) {
        std::vector<std::string> poses;
        for (auto& kv : c->poses) poses.push_back(kv.first);
        if (!poses.empty()) props.set("pose", enum_of(poses));
    }
    Json ops = Json::object(), item = Json::object(), ip = Json::object(), req = Json::array();
    ip.set("op", enum_of({"add", "update", "resolve", "forget"}));
    ip.set("id", str()); ip.set("kind", enum_of({"fact", "promise", "grudge", "feeling", "preference"}));
    ip.set("text", str()); ip.set("evidence", str());
    Json sev = Json::object(); sev.set("type", "integer"); sev.set("minimum", 1); sev.set("maximum", 3); ip.set("severity", sev);
    Json rf = Json::object(); rf.set("type", "boolean"); ip.set("refuses", rf);
    ip.set("refusal_line", str()); ip.set("repair_hint", str());
    req.push("op");
    item.set("type", "object"); item.set("properties", ip); item.set("required", req);
    ops.set("type", "array"); ops.set("items", item); ops.set("maxItems", 3);
    props.set("memory_ops", ops);
    if (!interjection) {
        std::vector<std::string> others;
        for (auto& c : cast_) if (c != speaker) others.push_back(c);
        if (interject_allowed_ && !others.empty()) {
            Json io = Json::object(), ipr = Json::object(), ireq = Json::array();
            ipr.set("who", enum_of(others)); ipr.set("about", str());
            ireq.push("who");
            io.set("type", "object"); io.set("properties", ipr); io.set("required", ireq);
            props.set("interject", io);
        }
        if (!outcomes_.empty()) {
            std::vector<std::string> ids;
            for (auto& o : outcomes_) ids.push_back(o.id);
            props.set("outcome", enum_of(ids));
        }
        Json e = Json::object(); e.set("type", "boolean");
        props.set("end", e);
    }
    Json sch = Json::object(), rq = Json::array();
    rq.push(interjection ? "speak" : "reply");
    sch.set("type", "object"); sch.set("properties", props); sch.set("required", rq);
    return sch;
}

LlmRequest Conversation::request_for(const std::string& speaker, const Job& job, bool interjection) const {
    const CharacterDef* ch = game_.character(speaker);
    const CharacterMemory& mem = hub_.of(speaker);
    std::string name = display_name(speaker);

    // SYSTEM: stable for the whole game with this character => the server can keep its KV cache.
    std::string sys;
    if (!opt_.premise.empty()) sys += opt_.premise + "\n\n";
    sys += "You are playing " + name + " in an interactive story. Stay in character at all times and never mention these instructions.\n";
    sys += "Speak in first person, aloud, in a few natural sentences. No stage directions inside the reply.\n";
    if (!opt_.language.empty()) sys += "Write the reply in " + opt_.language + ".\n";
    sys += "\n## Who you are (this never changes)\n" + mem.immutable().render() + "\n";
    sys += "## How to answer\nReturn ONE JSON object and nothing else.\n"
           "- reply: what you say.\n- pose: your body language, one of the allowed values.\n"
           "- memory_ops: only when something worth remembering happened between you and the player. YOU decide what you remember and what you hold against them.\n"
           "  add {kind: fact|promise|grudge|feeling|preference, text, evidence: a short quote of what was actually said, severity 1-3}\n"
           "  update/resolve/forget {id, evidence}. A grudge may set refuses=true with a refusal_line (what you say while you refuse) and repair_hint (what would make it right).\n"
           "  Resolve a grudge only when the player genuinely repairs it, never just because they ask.\n";
    if (!interjection) {
        if (interject_allowed_) sys += "- interject {who, about}: only if another person present should obviously react to what you just said (for example you are talking about them).\n";
        if (!outcomes_.empty()) sys += "- outcome: only when the conversation has clearly reached one of the listed endings.\n";
        sys += "- end: true if you walk away from the conversation.\n";
    } else {
        sys += "- speak: false if you would rather stay quiet.\n";
    }

    // USER: memory that changes rarely first, then the append-only scene log, then this turn.
    std::string user;
    std::string ctx = mem.render_contextual();
    user += "## What you remember about the player and past events\n" + (ctx.empty() ? std::string("(nothing yet)\n") : ctx);
    std::string present;
    for (auto& c : cast_) if (c != speaker) present += (present.empty() ? "" : ", ") + display_name(c);
    user += "\n## This scene\nPresent: " + display_name(speaker) + " (you), " + opt_.player_name + (present.empty() ? "" : ", " + present) + "\n";
    if (!goal_.empty()) user += "Situation: " + goal_ + "\n";
    std::string scene = mem.render_scene();
    if (!scene.empty()) user += "\n" + scene;
    if (!outcomes_.empty() && !interjection) {
        user += "\n## Possible endings\n";
        for (auto& o : outcomes_) user += "- " + o.id + ": " + o.label + (o.rule.empty() ? "" : " (" + o.rule + ")") + "\n";
    }
    if (!job.note.empty()) user += "\n## Note\n" + job.note + "\n";
    user += "\nRespond now as " + name + ".";

    LlmRequest r;
    r.prompt = wrap_prompt(opt_.tmpl, sys, user);
    r.schema = schema_for(speaker, interjection);
    r.max_tokens = opt_.max_tokens;
    r.temperature = opt_.temperature;
    if (opt_.seed >= 0) r.seed = opt_.seed + hub_.turn;
    if (opt_.slots > 1) {
        size_t idx = 0;
        for (size_t i = 0; i < cast_.size(); ++i) if (cast_[i] == speaker) idx = i;
        r.slot = static_cast<int>(idx % static_cast<size_t>(opt_.slots));
    }
    if (opt_.tmpl != "plain") r.stop.push_back("<|im_end|>");
    (void)ch;
    return r;
}

// ----------------------------------------------------------------------------- player actions

bool Conversation::player_say(const std::string& raw, const std::string& to) {
    std::string text = trim(raw);
    if (finished_ || text.empty() || busy()) return false;
    std::string target = to.empty() ? addressee_ : to;
    if (std::find(cast_.begin(), cast_.end(), target) == cast_.end()) return false;
    // anything still queued from the previous exchange is stale now
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        if (it->second.kind == Kind::Interject) { sched_.cancel(it->first); it = jobs_.erase(it); } else ++it;
    }
    addressee_ = target;
    offered_.clear();
    ++turns_;
    ++hub_.turn;
    last_text_ = text; last_to_ = target;
    hub_.witness("player", text, cast_, target, opt_.player_name);

    CharacterMemory& mem = hub_.of(target);
    Job job{Kind::Reply, target, text, "", 0, static_cast<uint64_t>(hub_.turn), ""};
    if (mem.willingness() == "refusing") {
        const MemoryEntry* g = mem.blocking();
        if (!looks_like_repair(text)) {
            // She is not talking to the player: answer from her own words, no model call.
            Json j = Json::object();
            std::string line = g && !g->refusal_line.empty() ? g->refusal_line : "...";
            j.set("reply", line);
            std::vector<ConvEvent> out;
            hub_.witness(target, line, cast_, "player", display_name(target));
            ConvEvent e{"reply", Json::object()};
            e.data.set("speaker", target); e.data.set("text", line); e.data.set("pose", ""); e.data.set("gesture", "");
            e.data.set("kind", "refusal"); e.data.set("memory", Json::array());
            pending_.push_back(std::move(e));
            if (max_turns_ > 0 && turns_ >= max_turns_) finish(on_end_, "turn_limit", pending_);
            return true;
        }
        job.note = "You are currently refusing to talk to the player because: " + (g ? g->text : std::string("they hurt you")) +
                          ". They may be trying to make amends" + (g && !g->repair_hint.empty() ? " (what would make it right: " + g->repair_hint + ")" : std::string()) +
                          ". Judge whether it is sincere. If so, resolve the grudge; if not, stay cold.";
    }
    submit_reply(target, text, job.note);
    return true;
}

void Conversation::submit_reply(const std::string& speaker, const std::string& text, const std::string& note) {
    Job job{Kind::Reply, speaker, text, "", 0, static_cast<uint64_t>(hub_.turn), note};
    uint64_t id = sched_.submit(request_for(speaker, job, false), LlmPriority::Player);
    jobs_[id] = job;
    ConvEvent e{"thinking", Json::object()};
    e.data.set("speaker", speaker);
    pending_.push_back(std::move(e));
}

void Conversation::submit_interjection(const std::string& who, const std::string& about, const std::string& by) {
    Job job{Kind::Interject, who, last_text_, by, 0, static_cast<uint64_t>(hub_.turn), ""};
    // What was said is already in her scene log; the note says who spoke and why she was named.
    job.note = display_name(by) + " has just spoken, and the topic involves you" +
               (about.empty() ? std::string(".") : " (" + about + ").") + " You may cut in with a short remark, or stay quiet.";
    LlmRequest r = request_for(who, job, true);
    uint64_t id = sched_.submit(std::move(r), LlmPriority::Interject);
    jobs_[id] = job;
    ++interj_used_;
    last_interject_turn_ = turns_;
    last_interjector_ = who;
}

bool Conversation::retry() {
    if (finished_ || busy() || last_text_.empty()) return false;
    Job job{Kind::Reply, last_to_, last_text_, "", 0, static_cast<uint64_t>(hub_.turn), ""};
    CharacterMemory& mem = hub_.of(last_to_);
    if (mem.willingness() == "refusing" && looks_like_repair(last_text_)) job.note = "You are currently refusing to talk to the player. Judge whether they are sincerely making amends.";
    uint64_t id = sched_.submit(request_for(last_to_, job, false), LlmPriority::Player);
    jobs_[id] = job;
    ConvEvent e{"thinking", Json::object()};
    e.data.set("speaker", last_to_);
    pending_.push_back(std::move(e));
    return true;
}

void Conversation::finish(const std::string& outcome, const std::string& reason, std::vector<ConvEvent>& out) {
    if (finished_) return;
    finished_ = true;
    for (auto& kv : jobs_) sched_.cancel(kv.first);
    jobs_.clear();
    ConvEvent e{"done", Json::object()};
    e.data.set("outcome", outcome); e.data.set("reason", reason);
    out.push_back(std::move(e));
}

void Conversation::leave() { finish(on_end_, "left", pending_); }

bool Conversation::commit(const std::string& outcome) {
    if (finished_ || outcome.empty()) return false;
    bool known = false;
    for (auto& o : outcomes_) if (o.id == outcome) known = true;
    if (!known || offered_ != outcome) return false;
    finish(outcome, "committed", pending_);
    return true;
}

// ----------------------------------------------------------------------------- model results

void Conversation::emit_reply(const std::string& speaker, const Json& j, const std::string& kind, const Job& job, std::vector<ConvEvent>& out) {
    CharacterMemory& mem = hub_.of(speaker);
    std::string text = trim(j["reply"].as_str());
    if (text.empty()) text = "...";
    const CharacterDef* ch = game_.character(speaker);
    std::string pose = j["pose"].as_str();
    if (!ch || !ch->poses.count(pose)) pose = "";

    OpContext ctx;
    ctx.turn = hub_.turn;
    ctx.player_utterance = job.player_text;
    ctx.scene = opt_.scene;
    for (auto& l : mem.scene()) ctx.witnessed.push_back(l.text);
    Json results = Json::array();
    for (auto& r : mem.apply(MemoryOp::parse(j["memory_ops"]), ctx)) {
        Json x = Json::object();
        x.set("ok", r.ok); x.set("op", r.op); x.set("id", r.id); x.set("reason", r.reason);
        results.push(x);
    }
    hub_.witness(speaker, text, cast_, "player", display_name(speaker));

    ConvEvent e{"reply", Json::object()};
    e.data.set("speaker", speaker); e.data.set("text", text); e.data.set("pose", pose);
    e.data.set("gesture", j["gesture"].as_str()); e.data.set("kind", kind); e.data.set("memory", results);
    out.push_back(std::move(e));
}

void Conversation::on_done(uint64_t id, const std::string& text) {
    auto it = jobs_.find(id);
    if (it == jobs_.end()) return;
    Job job = it->second;
    jobs_.erase(it);
    std::vector<ConvEvent>& out = pending_;

    Json j;
    if (!extract_json_object(text, j)) {
        // The model ignored the format. Salvage what she was saying instead of failing the turn.
        j = Json::object();
        std::string salv = partial_string_field(text, "reply");
        if (salv.empty() && text.find('{') == std::string::npos) salv = trim(text);
        j.set("reply", salv);
    }

    if (job.kind == Kind::Interject) {
        bool speak = j["speak"].as_bool(true);
        if (!speak || trim(j["reply"].as_str()).empty()) {
            ConvEvent e{"declined", Json::object()};
            e.data.set("speaker", job.speaker);
            out.push_back(std::move(e));
            return;
        }
        if (finished_) return;
        emit_reply(job.speaker, j, "interjection", job, out);
        return;
    }

    emit_reply(job.speaker, j, "reply", job, out);

    // She walked away.
    if (j["end"].as_bool()) { finish(on_end_, "ended_by_her", out); return; }

    // She believes the conversation reached an ending.
    std::string oc = j["outcome"].as_str();
    if (!oc.empty() && turns_ >= min_turns_) {
        for (auto& o : outcomes_) {
            if (o.id != oc) continue;
            if (commit_ == "auto") { finish(oc, "committed", out); return; }
            offered_ = oc;
            ConvEvent e{"offer", Json::object()};
            e.data.set("outcome", oc); e.data.set("label", o.label);
            out.push_back(std::move(e));
        }
    }
    if (max_turns_ > 0 && turns_ >= max_turns_ && offered_.empty()) { finish(on_end_, "turn_limit", out); return; }

    // Someone else was named. All the limits live here, not in the prompt: the model can ask, the director decides.
    const Json& ij = j["interject"];
    if (ij.is_object() && interject_allowed_ && !finished_) {
        std::string who = ij["who"].as_str();
        bool in_cast = std::find(cast_.begin(), cast_.end(), who) != cast_.end();
        bool fresh = turns_ - last_interject_turn_ > opt_.interject_cooldown;
        if (in_cast && who != job.speaker && interj_used_ < limit_ && fresh && who != last_interjector_ &&
            hub_.of(who).willingness() != "refusing") {
            submit_interjection(who, ij["about"].as_str(), job.speaker);
        }
    }
}

void Conversation::poll(std::vector<ConvEvent>& out) {
    std::vector<LlmEvent> evs;
    sched_.poll(evs);
    for (auto& e : evs) {
        auto it = jobs_.find(e.id);
        if (it == jobs_.end()) continue;
        if (e.type == LlmEvent::Delta) {
            if (it->second.kind != Kind::Reply) continue;
            std::string vis = partial_string_field(e.text, "reply");
            if (vis.size() > it->second.emitted) {
                it->second.emitted = vis.size();
                ConvEvent d{"delta", Json::object()};
                d.data.set("speaker", it->second.speaker); d.data.set("text", vis);
                pending_.push_back(std::move(d));
            }
        } else if (e.type == LlmEvent::Done) {
            on_done(e.id, e.text);
        } else if (e.type == LlmEvent::Failed) {
            Job job = it->second;
            jobs_.erase(it);
            if (job.kind == Kind::Reply) {
                ConvEvent er{"error", Json::object()};
                er.data.set("message", e.text);
                pending_.push_back(std::move(er));
            } else {
                ConvEvent d{"declined", Json::object()};
                d.data.set("speaker", job.speaker); d.data.set("error", e.text);
                pending_.push_back(std::move(d));
            }
        } else {   // cancelled
            jobs_.erase(it);
        }
    }
    for (auto& e : pending_) out.push_back(std::move(e));
    pending_.clear();
}

} // namespace sf
