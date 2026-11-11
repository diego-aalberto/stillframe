#include "memory.hpp"
#include <algorithm>

namespace sf {

MemoryConfig MemoryConfig::from_json(const Json& j) {
    MemoryConfig c;
    c.contextual_max = static_cast<int>(j["contextual_max"].as_int(c.contextual_max));
    c.scene_max_lines = static_cast<int>(j["scene_max_lines"].as_int(c.scene_max_lines));
    c.max_ops_per_turn = static_cast<int>(j["max_ops_per_turn"].as_int(c.max_ops_per_turn));
    c.prompt_budget_chars = static_cast<int>(j["prompt_budget_chars"].as_int(c.prompt_budget_chars));
    c.scene_budget_chars = static_cast<int>(j["scene_budget_chars"].as_int(c.scene_budget_chars));
    c.min_turns_to_resolve = static_cast<int>(j["min_turns_to_resolve"].as_int(c.min_turns_to_resolve));
    if (j["witness"].is_string()) c.witness_present = j["witness"].as_str() != "addressed";
    c.contextual_max = std::clamp(c.contextual_max, 4, 500);
    c.scene_max_lines = std::clamp(c.scene_max_lines, 2, 200);
    c.max_ops_per_turn = std::clamp(c.max_ops_per_turn, 0, 10);
    return c;
}

// ---- helpers
static std::string squash(const std::string& s) {
    std::string r;
    bool sp = false;
    for (unsigned char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { sp = true; continue; }
        if (sp && !r.empty()) r += ' ';
        sp = false;
        r += static_cast<char>((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    return r;
}
bool contains_ci(const std::string& h, const std::string& n) {
    std::string nn = squash(n);
    if (nn.empty()) return false;
    return squash(h).find(nn) != std::string::npos;
}
static std::string clip(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    size_t cut = n;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;   // do not split UTF-8
    return s.substr(0, cut);
}
static const std::set<std::string>& kinds() {
    static const std::set<std::string> k = {"fact", "preference", "promise", "grudge", "event", "feeling", "secret"};
    return k;
}

std::string ImmutableSlot::render() const {
    std::string out;
    for (auto& f : facts_) {
        out += "- ";
        if (!f.first.empty()) out += f.first + ": ";
        out += f.second + "\n";
    }
    return out;
}

Json MemoryEntry::to_json() const {
    Json j = Json::object();
    j.set("id", id); j.set("kind", kind); j.set("text", text); j.set("status", status);
    j.set("severity", severity); j.set("refuses", refuses);
    if (!refusal_line.empty()) j.set("refusal_line", refusal_line);
    if (!repair_hint.empty()) j.set("repair_hint", repair_hint);
    if (!quote.empty()) j.set("quote", quote);
    j.set("scene", scene); j.set("turn", turn); j.set("updated", updated);
    return j;
}
MemoryEntry MemoryEntry::from_json(const Json& j) {
    MemoryEntry e;
    e.id = j["id"].as_str(); e.kind = j["kind"].as_str(); e.text = j["text"].as_str();
    e.status = j["status"].str_or("open");
    e.severity = static_cast<int>(j["severity"].as_int(1));
    e.refuses = j["refuses"].as_bool();
    e.refusal_line = j["refusal_line"].as_str(); e.repair_hint = j["repair_hint"].as_str(); e.quote = j["quote"].as_str();
    e.scene = j["scene"].as_str(); e.turn = j["turn"].as_int(); e.updated = j["updated"].as_int();
    return e;
}

std::vector<MemoryOp> MemoryOp::parse(const Json& arr) {
    std::vector<MemoryOp> out;
    for (auto& j : arr.items()) {
        if (!j.is_object()) continue;
        MemoryOp o;
        o.op = lower(j["op"].as_str()); o.id = j["id"].as_str(); o.kind = lower(j["kind"].as_str());
        o.text = j["text"].as_str(); o.evidence = j["evidence"].as_str(); o.refusal_line = j["refusal_line"].as_str();
        o.repair_hint = j["repair_hint"].as_str(); o.note = j["note"].as_str();
        o.severity = j["severity"].is_number() ? static_cast<int>(j["severity"].as_int()) : 0;
        if (j["refuses"].is_bool()) o.refuses = j["refuses"].as_bool() ? 1 : 0;
        out.push_back(std::move(o));
    }
    return out;
}

// ---- CharacterMemory
MemoryEntry* CharacterMemory::mut(const std::string& id) {
    for (auto& e : contextual_) if (e.id == id) return &e;
    return nullptr;
}
const MemoryEntry* CharacterMemory::find(const std::string& id) const {
    for (auto& e : contextual_) if (e.id == id) return &e;
    return nullptr;
}
const MemoryEntry* CharacterMemory::blocking() const {
    const MemoryEntry* best = nullptr;
    for (auto& e : contextual_)
        if (e.status == "open" && e.refuses && (!best || e.severity > best->severity)) best = &e;
    return best;
}
size_t CharacterMemory::open_count() const {
    return static_cast<size_t>(std::count_if(contextual_.begin(), contextual_.end(), [](const MemoryEntry& e) { return e.status == "open" && e.kind == "grudge"; }));
}
std::string CharacterMemory::willingness() const {
    if (blocking()) return "refusing";
    return open_count() ? "guarded" : "open";
}

// Makes space for one more entry: drops the oldest resolved entry, then the oldest low-importance
// non-grudge. Never drops an open grudge. Returns false if everything left is protected.
bool CharacterMemory::make_room() {
    if (static_cast<int>(contextual_.size()) < cfg_.contextual_max) return true;
    auto drop = [&](auto pred) {
        auto it = std::find_if(contextual_.begin(), contextual_.end(), pred);
        if (it == contextual_.end()) return false;
        contextual_.erase(it);
        return true;
    };
    if (drop([](const MemoryEntry& e) { return e.status == "resolved"; })) return true;
    if (drop([](const MemoryEntry& e) { return e.kind != "grudge" && e.kind != "promise" && e.severity <= 1; })) return true;
    if (drop([](const MemoryEntry& e) { return e.kind != "grudge" && e.kind != "promise"; })) return true;
    return false;
}

std::vector<OpResult> CharacterMemory::apply(const std::vector<MemoryOp>& ops, const OpContext& ctx) {
    std::vector<OpResult> results;
    int accepted = 0;
    auto reject = [&](const MemoryOp& o, std::string why) { results.push_back({false, o.op, o.id, std::move(why)}); };
    auto player_said = [&](const std::string& q) { return contains_ci(ctx.player_utterance, q); };
    auto witnessed = [&](const std::string& q) {
        if (player_said(q)) return true;
        for (auto& w : ctx.witnessed) if (contains_ci(w, q)) return true;
        return false;
    };
    for (size_t n = 0; n < ops.size(); ++n) {
        const MemoryOp& o = ops[n];
        if (accepted >= cfg_.max_ops_per_turn) { reject(o, "too many memory changes in one turn"); continue; }
        if (o.op == "add") {
            std::string kind = o.kind.empty() ? "fact" : o.kind;
            if (!kinds().count(kind)) { reject(o, "unknown kind '" + o.kind + "'"); continue; }
            std::string text = trim(o.text);
            if (text.empty()) { reject(o, "empty text"); continue; }
            if (kind != "feeling" && !witnessed(o.evidence)) { reject(o, "evidence must quote something said in this scene"); continue; }
            bool refuses = o.refuses == 1;
            if (refuses && kind != "grudge") { reject(o, "only a grudge can make her refuse to talk"); continue; }
            int sev = std::clamp(o.severity ? o.severity : (kind == "grudge" ? 2 : 1), 1, 5);
            if (refuses && sev < 2) { reject(o, "refusing to talk needs severity of at least 2"); continue; }
            std::string line = trim(o.refusal_line);
            if (refuses && line.empty()) { reject(o, "a refusal needs the line she says while refusing"); continue; }
            // same note again => strengthen the existing entry instead of duplicating
            MemoryEntry* dup = nullptr;
            for (auto& e : contextual_) if (e.status == "open" && e.kind == kind && squash(e.text) == squash(text)) dup = &e;
            if (dup) {
                dup->updated = ctx.turn;
                dup->severity = std::max(dup->severity, sev);
                if (refuses) { dup->refuses = true; dup->refusal_line = clip(line, 200); }
                results.push_back({true, "add", dup->id, "merged with an existing memory"});
                ++accepted;
                continue;
            }
            if (!make_room()) { reject(o, "memory is full of unresolved matters"); continue; }
            MemoryEntry e;
            e.id = "m" + std::to_string(next_id_++);
            e.kind = kind; e.text = clip(text, 300); e.severity = sev; e.refuses = refuses;
            e.refusal_line = clip(line, 200); e.repair_hint = clip(trim(o.repair_hint), 200);
            e.quote = clip(trim(o.evidence), 300); e.scene = ctx.scene; e.turn = e.updated = ctx.turn;
            contextual_.push_back(e);
            results.push_back({true, "add", e.id, ""});
            ++accepted;
        } else if (o.op == "update") {
            MemoryEntry* e = mut(o.id);
            if (!e) { reject(o, "no such memory"); continue; }
            if (e->status != "open") { reject(o, "that memory is already resolved"); continue; }
            int sev = o.severity ? std::clamp(o.severity, 1, 5) : e->severity;
            bool calming = sev < e->severity || o.refuses == 0;
            if (calming && e->refuses && !player_said(o.evidence)) { reject(o, "calming a grudge needs a quote from what the player just said"); continue; }
            if (calming && e->refuses && ctx.turn - e->turn < cfg_.min_turns_to_resolve) { reject(o, "too soon to change her mind"); continue; }
            if (!trim(o.text).empty()) e->text = clip(trim(o.text), 300);
            e->severity = sev;
            if (o.refuses == 0) e->refuses = false;
            if (o.refuses == 1 && e->kind == "grudge" && sev >= 2 && !trim(o.refusal_line).empty()) { e->refuses = true; }
            if (!trim(o.refusal_line).empty()) e->refusal_line = clip(trim(o.refusal_line), 200);
            if (!trim(o.repair_hint).empty()) e->repair_hint = clip(trim(o.repair_hint), 200);
            e->updated = ctx.turn;
            results.push_back({true, "update", e->id, ""});
            ++accepted;
        } else if (o.op == "resolve") {
            MemoryEntry* e = mut(o.id);
            if (!e) { reject(o, "no such memory"); continue; }
            if (e->status != "open") { reject(o, "already resolved"); continue; }
            if (e->kind == "grudge" || e->kind == "promise") {
                if (!player_said(o.evidence)) { reject(o, "resolving needs a quote from what the player just said"); continue; }
                if (ctx.turn - e->turn < cfg_.min_turns_to_resolve) { reject(o, "too soon: it was only just recorded"); continue; }
            }
            e->status = "resolved"; e->refuses = false; e->updated = ctx.turn;
            if (!trim(o.note).empty()) e->text = clip(e->text + " (resolved: " + trim(o.note) + ")", 400);
            results.push_back({true, "resolve", e->id, ""});
            ++accepted;
        } else if (o.op == "forget") {
            auto it = std::find_if(contextual_.begin(), contextual_.end(), [&](const MemoryEntry& e) { return e.id == o.id; });
            if (it == contextual_.end()) { reject(o, "no such memory"); continue; }
            if (it->status == "open" && (it->kind == "grudge" || it->kind == "promise")) { reject(o, "an unresolved grudge or promise cannot simply be forgotten; resolve it"); continue; }
            contextual_.erase(it);
            results.push_back({true, "forget", o.id, ""});
            ++accepted;
        } else reject(o, "unknown operation '" + o.op + "'");
    }
    return results;
}

void CharacterMemory::witness(const std::string& speaker, const std::string& text) {
    scene_.push_back({speaker, clip(text, 600)});
    while (static_cast<int>(scene_.size()) > cfg_.scene_max_lines) scene_.pop_front();
}

std::string CharacterMemory::render_contextual() const {
    std::vector<const MemoryEntry*> open, done;
    for (auto& e : contextual_) (e.status == "open" ? open : done).push_back(&e);
    std::stable_sort(open.begin(), open.end(), [](const MemoryEntry* a, const MemoryEntry* b) {
        if (a->refuses != b->refuses) return a->refuses;
        if ((a->kind == "grudge") != (b->kind == "grudge")) return a->kind == "grudge";
        if (a->severity != b->severity) return a->severity > b->severity;
        return a->updated > b->updated;
    });
    std::stable_sort(done.begin(), done.end(), [](const MemoryEntry* a, const MemoryEntry* b) { return a->updated > b->updated; });
    std::string out;
    size_t budget = static_cast<size_t>(cfg_.prompt_budget_chars);
    auto line = [&](const MemoryEntry& e) {
        std::string l = "- [" + e.id + "] " + e.kind;
        if (e.status == "open" && e.severity > 1) l += " (severity " + std::to_string(e.severity) + ")";
        if (e.refuses) l += " (you refuse to talk until this is repaired)";
        l += ": " + e.text;
        if (e.status == "open" && e.refuses && !e.repair_hint.empty()) l += " -- repair would be: " + e.repair_hint;
        return l + "\n";
    };
    for (auto* e : open) { std::string l = line(*e); if (out.size() + l.size() > budget && !e->refuses) break; out += l; }
    int shown = 0;
    for (auto* e : done) {
        if (shown >= 4) break;
        std::string l = "- [" + e->id + "] resolved " + e->kind + ": " + clip(e->text, 120) + "\n";
        if (out.size() + l.size() > budget) break;
        out += l; ++shown;
    }
    return out;
}

std::string CharacterMemory::render_scene() const {
    std::vector<std::string> lines;
    size_t total = 0;
    for (auto it = scene_.rbegin(); it != scene_.rend(); ++it) {
        std::string l = it->speaker.empty() ? it->text : it->speaker + ": " + it->text;
        if (total + l.size() > static_cast<size_t>(cfg_.scene_budget_chars) && !lines.empty()) break;
        total += l.size();
        lines.push_back(std::move(l));
    }
    std::string out;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) out += *it + "\n";
    return out;
}

Json CharacterMemory::save() const {
    Json j = Json::object(), c = Json::array(), s = Json::array();
    for (auto& e : contextual_) c.push(e.to_json());
    for (auto& l : scene_) { Json x = Json::object(); x.set("who", l.speaker); x.set("text", l.text); s.push(x); }
    j.set("contextual", c); j.set("scene", s); j.set("next_id", next_id_);
    return j;
}
void CharacterMemory::load(const Json& j) {
    contextual_.clear(); scene_.clear();
    for (auto& e : j["contextual"].items()) contextual_.push_back(MemoryEntry::from_json(e));
    for (auto& l : j["scene"].items()) scene_.push_back({l["who"].as_str(), l["text"].as_str()});
    next_id_ = std::max<int64_t>(j["next_id"].as_int(1), 1);
    for (auto& e : contextual_) {   // never reuse an id even if the counter was lost
        if (e.id.size() > 1 && e.id[0] == 'm') next_id_ = std::max<int64_t>(next_id_, std::atoll(e.id.c_str() + 1) + 1);
    }
}

// ---- MemoryHub
MemoryHub::MemoryHub(const Game& game, MemoryConfig cfg) : cfg_(cfg) {
    for (auto& kv : game.characters)
        mem_.emplace(kv.first, CharacterMemory(kv.first, std::make_shared<const ImmutableSlot>(kv.second.immutable), cfg));
}
CharacterMemory& MemoryHub::of(const std::string& id) {
    auto it = mem_.find(id);
    if (it == mem_.end()) throw Error("no memory for unknown character '" + id + "'");
    return it->second;
}
const CharacterMemory& MemoryHub::of(const std::string& id) const {
    auto it = mem_.find(id);
    if (it == mem_.end()) throw Error("no memory for unknown character '" + id + "'");
    return it->second;
}
void MemoryHub::begin_scene() { for (auto& kv : mem_) kv.second.begin_scene(); }

void MemoryHub::witness(const std::string& speaker, const std::string& text, const std::vector<std::string>& present, const std::string& addressee, const std::string& label) {
    for (auto& id : present) {
        auto it = mem_.find(id);
        if (it == mem_.end()) continue;
        if (!cfg_.witness_present && id != speaker && id != addressee) continue;
        it->second.witness(label.empty() ? speaker : label, text);
    }
}
Json MemoryHub::save() const {
    Json j = Json::object();
    for (auto& kv : mem_) j.set(kv.first, kv.second.save());
    j.set("_turn", Json(turn));
    return j;
}
void MemoryHub::load(const Json& j) {
    for (auto& kv : mem_) kv.second.load(j[kv.first]);
    turn = j["_turn"].as_int();
}

} // namespace sf
