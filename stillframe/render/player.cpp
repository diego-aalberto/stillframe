#include "player.hpp"
#include "../core/platform.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace sf {

namespace {
const std::map<std::string, std::string>& default_strings() {
    static const std::map<std::string, std::string> s = {
        {"say_something", "Say something\xE2\x80\xA6"}, {"send", "Send"}, {"leave", "Leave"}, {"history", "History"}, {"menu", "Menu"},
        {"resume", "Resume"}, {"save", "Save"}, {"load", "Load"}, {"quit", "Back to library"}, {"back", "Back"},
        {"empty_slot", "Empty"}, {"retry", "Retry"}, {"skip", "Skip"}, {"cancel", "Cancel"}, {"continue", "Continue"},
        {"interrupts", "cutting in"}, {"minigame_playing", "Playing"}, {"minigame_hint", "The game opens in its own window."},
        {"the_end", "The End"}, {"text_speed", "Text speed"}, {"speed_instant", "Instant"}, {"speed_fast", "Fast"},
        {"speed_normal", "Normal"}, {"speed_slow", "Slow"}, {"saved", "Saved"}, {"loaded", "Loaded"},
        {"remembers", "{name} will remember that."}, {"lets_go", "{name} let it go."},
        {"model_offline", "The conversation model isn't reachable"},
        {"model_offline_help", "Start the model server (llama-server) or check \"llm.server\" in game.json, then press Retry."},
        {"minigame_failed", "The minigame couldn't finish"}, {"script_error", "The story hit a problem"},
        {"update_warning", "This game was updated since the save; your position was restored as closely as possible."},
        {"history_empty", "Nothing has been said yet."}, {"auto", "Autosave"}, {"quick", "Quicksave"},
    };
    return s;
}

size_t cp_count(const std::string& s) { return utf8_decode(s).size(); }

// First n code points of s.
std::string cp_prefix(const std::string& s, size_t n) {
    size_t i = 0, k = 0;
    while (i < s.size() && k < n) {
        ++i;
        while (i < s.size() && (static_cast<unsigned char>(s[i]) >> 6) == 2) ++i;
        ++k;
    }
    return s.substr(0, i);
}

std::string format_time(int64_t t) {
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string snippet(const std::string& s, size_t n) {
    std::string t;
    for (char c : s) t += (c == '\n') ? ' ' : c;
    if (cp_count(t) <= n) return t;
    return cp_prefix(t, n) + "\xE2\x80\xA6";
}
} // namespace

// ------------------------------------------------------------------------------------------------ layout

struct Player::Layout {
    Rect box, text, input, send, leave, offer, btn_menu, btn_history, back, retry, skip, mg_cancel, panel;
    Rect tag;                                  // where the name tag goes (x, y)
    std::vector<Rect> choices, chips, overlay_buttons, slots;
};

float Player::px_text() const { return theme_.text_size * h_; }
float Player::px_name() const { return theme_.name_size * h_; }
float Player::px_small() const { return theme_.small_size * h_; }

std::string Player::tr(const std::string& key) const {
    auto it = strings_.find(key);
    if (it != strings_.end()) return it->second;
    auto d = default_strings().find(key);
    return d == default_strings().end() ? key : d->second;
}

Player::Layout Player::layout() const {
    Layout L;
    const float W = static_cast<float>(w_), H = static_cast<float>(h_);
    const float m = theme_.margin * W;
    const bool talk = mode_ == Mode::Talk && conv_;
    const float bh = (theme_.box_height + (talk ? 0.10f : 0.f)) * H;
    L.box = {m, H - m * 0.7f - bh, W - 2 * m, bh};
    const float pad = 0.022f * W;
    const float in_h = 0.066f * H;
    L.tag = {L.box.x + pad * 0.9f, L.box.y - px_name() * 0.95f, 0, 0};
    L.text = {L.box.x + pad, L.box.y + px_name() * 1.35f, L.box.w - 2 * pad, bh - px_name() * 1.35f - pad * 0.7f - (talk ? in_h + pad * 0.5f : 0.f)};
    if (talk) {
        const float send_w = 0.11f * W, gap = 0.012f * W;
        L.input = {L.box.x + pad, L.box.bottom() - pad * 0.6f - in_h, L.box.w - 2 * pad - send_w - gap, in_h};
        L.send = {L.input.right() + gap, L.input.y, send_w, in_h};
        // addressee chips + leave, right-aligned above the box
        const float ch = px_small() * 2.3f;
        float x = L.box.right();
        std::vector<float> widths;
        for (auto& id : conv_->cast()) widths.push_back(theme_.fonts_bold.width(name_of(id, Json()), px_small()) + px_small() * 2.2f);
        float lw = theme_.fonts_bold.width(tr("leave"), px_small()) + px_small() * 2.2f;
        L.leave = {x - lw, L.box.y - ch - 12, lw, ch};
        x = L.leave.x - 10;
        L.chips.resize(widths.size());
        for (size_t i = widths.size(); i-- > 0;) { x -= widths[i]; L.chips[i] = {x, L.leave.y, widths[i], ch}; x -= 8; }
        float ow = std::min(0.5f * W, theme_.fonts_bold.width(offered_label_, px_small()) + px_small() * 4.f);
        L.offer = {L.box.x, L.box.y - ch * 1.25f - 12, ow, ch * 1.25f};
    }
    L.btn_menu = {W - 0.075f * W - 12, 12, 0.075f * W, px_small() * 2.2f};
    L.btn_history = {L.btn_menu.x - 0.095f * W - 8, 12, 0.095f * W, px_small() * 2.2f};

    if (mode_ == Mode::Menu) {
        const size_t n = menu_["options"].size();
        const float bhh = 0.082f * H, gap = 0.016f * H;
        float maxw = 0;
        for (auto& o : menu_["options"].items()) maxw = std::max(maxw, theme_.fonts_bold.width(o["text"].as_str(), px_text() * 0.92f));
        float bw = std::clamp(maxw + 0.08f * W, 0.34f * W, 0.8f * W);
        float total = n * bhh + (n ? (n - 1) * gap : 0);
        float region_h = L.box.y - 0.04f * H;
        float y = std::max(0.05f * H, (region_h - total) / 2);
        for (size_t i = 0; i < n; ++i) L.choices.push_back({(W - bw) / 2, y + i * (bhh + gap), bw, bhh});
    }
    if (mode_ == Mode::MenuOverlay) {
        float pw = 0.34f * W, bhh = 0.075f * H, gap = 0.014f * H;
        L.panel = {(W - pw) / 2, 0.15f * H, pw, 0.045f * H + 6 * (bhh + gap) + 0.03f * H};
        for (int i = 0; i < 6; ++i) L.overlay_buttons.push_back({L.panel.x + 0.04f * pw, L.panel.y + 0.045f * H + i * (bhh + gap), pw * 0.92f, bhh});
    }
    if (mode_ == Mode::SaveLoad) {
        float cw = 0.38f * W, chh = 0.16f * H, gx = 0.03f * W, gy = 0.03f * H;
        float x0 = (W - (2 * cw + gx)) / 2, y0 = 0.17f * H;
        for (int i = 0; i < 6; ++i) L.slots.push_back({x0 + (i % 2) * (cw + gx), y0 + (i / 2) * (chh + gy), cw, chh});
        L.back = {(W - 0.2f * W) / 2, 0.88f * H - px_small(), 0.2f * W, 0.07f * H};
    }
    if (mode_ == Mode::History) L.back = {(W - 0.2f * W) / 2, 0.9f * H - px_small(), 0.2f * W, 0.065f * H};
    if (mode_ == Mode::Error) {
        float pw = 0.56f * W, ph = 0.34f * H;
        L.panel = {(W - pw) / 2, (H - ph) / 2, pw, ph};
        float bw = 0.18f * W, bhh = 0.07f * H;
        L.retry = {L.panel.right() - 2 * bw - 0.05f * W, L.panel.bottom() - bhh - 0.03f * H, bw, bhh};
        L.skip = {L.panel.right() - bw - 0.025f * W, L.retry.y, bw, bhh};
    }
    if (mode_ == Mode::Minigame) {
        float pw = 0.5f * W, ph = 0.3f * H;
        L.panel = {(W - pw) / 2, (H - ph) / 2, pw, ph};
        L.mg_cancel = {L.panel.right() - 0.17f * W - 0.025f * W, L.panel.bottom() - 0.07f * H - 0.03f * H, 0.17f * W, 0.07f * H};
    }
    if (mode_ == Mode::Ended) {
        L.back = {(W - 0.26f * W) / 2, 0.62f * H, 0.26f * W, 0.075f * H};
    }
    return L;
}

// ------------------------------------------------------------------------------------------------ construction

Player::Player(std::shared_ptr<Game> game, PlayerOptions opt)
    : game_(std::move(game)), opt_(std::move(opt)), assets_(game_->vfs.get()), theme_(Theme::from_game(*game_, assets_)),
      stage_(*game_, assets_, theme_), vm_(game_->program, opt_.seed >= 0 ? static_cast<uint64_t>(opt_.seed) : 0x5EEDull),
      hub_(*game_, MemoryConfig::from_json(game_->llm["memory"])),
      saves_(opt_.data_dir.empty() ? user_data_dir() : opt_.data_dir, game_->id, game_->save_format, game_->version),
      w_(game_->canvas_w), h_(game_->canvas_h) {
    for (auto& kv : game_->manifest["strings"].members()) strings_[kv.first] = kv.second.as_str();
    vm_.vars() = game_->initial_vars;
    if (opt_.cache_dir.empty()) opt_.cache_dir = (opt_.data_dir.empty() ? user_data_dir() : opt_.data_dir) + "/cache";
    try {
        Json st = saves_.read_settings();
        if (st["text_speed"].is_number()) text_speed_ = st["text_speed"].as_num(1.0);
    } catch (const Error&) {}
    vm_.start(game_->start_label);
}

Player::~Player() = default;

std::vector<std::string> Player::take_messages() {
    for (auto& e : assets_.errors()) if (std::find(messages_.begin(), messages_.end(), e) == messages_.end()) messages_.push_back(e);
    std::vector<std::string> out;
    out.swap(messages_);
    return out;
}

std::vector<std::string> Player::menu_labels() const {
    std::vector<std::string> out;
    if (mode_ == Mode::Menu) for (auto& o : menu_["options"].items()) out.push_back(o["text"].as_str());
    return out;
}

std::string Player::mode_name() const {
    switch (mode_) {
        case Mode::Running: return "running"; case Mode::Say: return "say"; case Mode::Menu: return "menu"; case Mode::Talk: return "talk";
        case Mode::Minigame: return "minigame"; case Mode::Pause: return "pause"; case Mode::Ended: return "ended"; case Mode::Error: return "error";
        case Mode::MenuOverlay: return "menu_overlay"; case Mode::SaveLoad: return "save_load"; case Mode::History: return "history";
    }
    return "?";
}

Color Player::color_of(const std::string& speaker) const {
    if (auto* c = game_->character(speaker)) return Color::parse(c->color, theme_.accent);
    return theme_.accent;
}

std::string Player::name_of(const std::string& speaker, const Json& ev) const {
    if (speaker.empty()) return ev["name"].as_str();
    if (speaker == "player") return vm_.vars()["player"].str_or(opt_.player_name.empty() ? "You" : opt_.player_name);
    if (auto* c = game_->character(speaker)) return c->name;
    return speaker;
}

// ------------------------------------------------------------------------------------------------ flow

void Player::toast(const std::string& text) { toasts_.push_back({text, 0}); dirty_ = true; }

void Player::log_history(const Line& l) {
    if (l.text.empty()) return;
    hist_.push_back({l.name, l.text, l.color});
    while (hist_.size() > 300) hist_.pop_front();
    hist_text_.push_back((l.name.empty() ? "" : l.name + ": ") + l.text);
    if (hist_text_.size() > 300) hist_text_.erase(hist_text_.begin());
}

void Player::show_line(Line l) {
    l.total = cp_count(l.text);
    l.shown = (text_speed_ <= 0 || skipping_) ? static_cast<double>(l.total) : 0.0;
    line_ = std::move(l);
    has_line_ = true;
    dirty_ = true;
}

void Player::show_error(const std::string& title, const std::string& detail, bool can_retry) {
    error_title_ = title; error_detail_ = detail; error_retry_ = can_retry;
    return_mode_ = mode_ == Mode::Error ? return_mode_ : mode_;
    mode_ = Mode::Error;
    messages_.push_back(title + ": " + detail);
    dirty_ = true;
}

void Player::autosave() {
    if (!opt_.autosave) return;
    try { save_slot("auto"); } catch (const Error&) {}
}

void Player::process_events() {
    while (!pending_.empty() && !stage_.transitioning() && mode_ == Mode::Running) {
        Event e = std::move(pending_.front());
        pending_.pop_front();
        const Json& d = e.data;
        const std::string& t = e.type;
        if (t == "scene") { hub_.begin_scene(); stage_.set_speaker(""); stage_.apply(e); has_line_ = false; }
        else if (t == "show" || t == "hide" || t == "pose" || t == "anim" || t == "with") stage_.apply(e);
        else if (t == "say") {
            std::string sp = d["speaker"].as_str();
            Line l;
            l.speaker = sp; l.name = name_of(sp, d); l.text = d["text"].as_str();
            l.kind = sp.empty() && l.name.empty() ? "narration" : "say";
            l.color = sp.empty() ? theme_.accent : color_of(sp);
            stage_.set_speaker(sp);
            hub_.witness(sp, l.text, stage_.present(), "", l.name);
            log_history(l);
            show_line(std::move(l));
            mode_ = Mode::Say;
        }
        else if (t == "menu") {
            menu_ = d;
            if (d.has("caption")) {
                Line l; l.speaker = d["speaker"].as_str(); l.name = name_of(l.speaker, d); l.text = d["caption"].as_str();
                l.kind = l.name.empty() ? "narration" : "say"; l.color = l.speaker.empty() ? theme_.accent : color_of(l.speaker);
                stage_.set_speaker(l.speaker);
                show_line(std::move(l));
                line_.shown = static_cast<double>(line_.total);
            } else has_line_ = false;
            mode_ = Mode::Menu;
        }
        else if (t == "talk") start_talk(d);
        else if (t == "minigame") start_minigame(d);
        else if (t == "pause") { pause_left_ = d["seconds"].as_num(0); mode_ = Mode::Pause; }
        else if (t == "end") { ending_ = d["ending"].as_str(); mode_ = Mode::Ended; has_line_ = false; stage_.set_speaker(""); }
        else if (t == "music") {
            music_ = d["track"].as_str();
            if (opt_.audio) opt_.audio->music(game_->resolve_audio(music_), d["fade"].as_num(0), d["loop"].as_bool(true));
        }
        else if (t == "stop_music") { music_.clear(); if (opt_.audio) opt_.audio->stop_music(d["fade"].as_num(0)); }
        else if (t == "sound") { if (opt_.audio) opt_.audio->sound(game_->resolve_audio(d["sound"].as_str())); }
        else if (t == "notify") toast(d["text"].as_str());
        else if (t == "checkpoint") autosave();
    }
    if (pending_.empty() || mode_ != Mode::Running) stage_.end_batch();
    dirty_ = true;
}

void Player::run_script() {
    for (int guard = 0; guard < 16 && mode_ == Mode::Running; ++guard) {
        if (stage_.transitioning()) break;
        if (pending_.empty()) {
            if (vm_.waiting()) { present_wait(); break; }
            if (vm_.finished()) { mode_ = Mode::Ended; break; }
            try {
                for (auto& e : vm_.run()) pending_.push_back(std::move(e));
            } catch (const Error& e) {
                error_kind_ = "script";
                show_error(tr("script_error"), e.what(), false);
                return;
            }
        }
        process_events();
    }
}

void Player::present_wait() {
    const std::string k = vm_.wait_kind();
    const Json& d = vm_.wait_event();
    pending_.clear();
    if (k == "say") { pending_.push_back({"say", d}); }
    else if (k == "menu") { pending_.push_back({"menu", d}); }
    else if (k == "talk") { pending_.push_back({"talk", d}); }
    else if (k == "minigame") { pending_.push_back({"minigame", d}); }
    else if (k == "pause") { pending_.push_back({"pause", d}); }
    process_events();
}

void Player::advance() {
    if (mode_ == Mode::Say) {
        if (!line_.full()) { line_.shown = static_cast<double>(line_.total); dirty_ = true; return; }
        vm_.advance();
        mode_ = Mode::Running;
        run_script();
    } else if (mode_ == Mode::Pause) {
        vm_.advance();
        mode_ = Mode::Running;
        run_script();
    } else if (mode_ == Mode::Talk) {
        if (!line_.full()) { line_.shown = static_cast<double>(line_.total); dirty_ = true; return; }
        if (!talk_queue_.empty()) {
            Line next = std::move(talk_queue_.front());
            talk_queue_.pop_front();
            stage_.set_speaker(next.speaker);
            show_line(std::move(next));
        } else if (talk_done_) finish_talk(talk_done_outcome_);
    }
}

// ------------------------------------------------------------------------------------------------ talk

LlmBackend* Player::backend() {
    if (backend_) return backend_.get();
    if (opt_.llm) { backend_ = opt_.llm; return backend_.get(); }
    if (game_->llm["enabled"].is_bool() && !game_->llm["enabled"].as_bool()) { backend_error_ = "language model is switched off for this game (\"llm.enabled\": false)"; return nullptr; }
    std::string url = !opt_.llm_url.empty() ? opt_.llm_url : game_->llm["server"].str_or("http://127.0.0.1:8080");
    auto b = std::make_shared<LlamaServerBackend>(url);
    std::string why = b->probe();
    if (!why.empty()) { backend_error_ = why; return nullptr; }
    backend_ = b;
    return backend_.get();
}

void Player::start_talk(const Json& ev) {
    talk_event_ = ev;
    LlmBackend* be = backend();
    if (!be) { error_kind_ = "talk"; mode_ = Mode::Running; show_error(tr("model_offline"), backend_error_ + "\n" + tr("model_offline_help"), true); return; }
    if (!sched_) sched_ = std::make_unique<LlmScheduler>(backend_);
    ConvOptions co = ConvOptions::from_game(*game_, ev);
    co.player_name = name_of("player", Json());
    co.scene = vm_.where();
    co.seed = opt_.seed;
    // Character files may gate poses on game state ("pose_requires": {"cuddle": "rel.hana.status == 'exploring'"}).
    for (auto& cid : ev["cast"].items()) {
        const CharacterDef* cd = game_->character(cid.as_str());
        if (!cd) continue;
        for (auto& kv : cd->raw["pose_requires"].members()) {
            try {
                Json vars = vm_.vars();
                uint64_t rng = 1;
                EvalContext ctx; ctx.vars = &vars; ctx.rng = &rng;
                if (!truthy(eval(parse_expr(kv.second.as_str()), ctx))) co.blocked_poses[cid.as_str()].insert(kv.first);
            } catch (const Error&) { co.blocked_poses[cid.as_str()].insert(kv.first); }   // when in doubt, stay modest
        }
    }
    try { conv_ = std::make_unique<Conversation>(*game_, hub_, *sched_, ev, co); }
    catch (const Error& e) { error_kind_ = "talk"; mode_ = Mode::Running; show_error(tr("script_error"), e.what(), false); return; }
    talk_queue_.clear(); input_.clear();
    talk_thinking_ = talk_done_ = streaming_ = false;
    offered_outcome_.clear(); offered_label_.clear(); player_echo_.clear();
    has_line_ = false;
    stage_.set_speaker(conv_->addressee());
    mode_ = Mode::Talk;
}

void Player::finish_talk(const std::string& outcome) {
    conv_.reset();
    talk_queue_.clear();
    talk_thinking_ = talk_done_ = streaming_ = false;
    vm_.finish_talk(outcome);
    mode_ = Mode::Running;
    has_line_ = false;
    stage_.set_speaker("");
    run_script();
}

void Player::pump_talk(double) {
    if (!conv_) return;
    std::vector<ConvEvent> evs;
    conv_->poll(evs);
    for (auto& e : evs) {
        const Json& d = e.data;
        dirty_ = true;
        if (e.type == "thinking") { talk_thinking_ = true; talk_thinking_who_ = d["speaker"].as_str(); stage_.set_speaker(talk_thinking_who_); }
        else if (e.type == "delta") {
            talk_thinking_ = false; streaming_ = true;
            Line l; l.speaker = d["speaker"].as_str(); l.name = name_of(l.speaker, Json()); l.color = color_of(l.speaker);
            l.kind = "reply"; l.text = d["text"].as_str();
            l.total = cp_count(l.text); l.shown = static_cast<double>(l.total);
            line_ = l; has_line_ = true;
        }
        else if (e.type == "reply") {
            talk_thinking_ = false;
            Line l; l.speaker = d["speaker"].as_str(); l.name = name_of(l.speaker, Json()); l.color = color_of(l.speaker);
            l.kind = d["kind"].as_str(); l.text = d["text"].as_str();
            if (!d["pose"].as_str().empty()) { Json p = Json::object(); p.set("char", l.speaker); p.set("pose", d["pose"].as_str()); stage_.apply({"pose", p}); }
            if (is_known_motion(d["gesture"].as_str())) { Json a = Json::object(); a.set("char", l.speaker); a.set("motion", d["gesture"].as_str()); stage_.apply({"anim", a}); }
            for (auto& m : d["memory"].items()) {
                if (!m["ok"].as_bool()) continue;
                const CharacterMemory& cm = hub_.of(l.speaker);
                const MemoryEntry* me = cm.find(m["id"].as_str());
                std::string text;
                if (m["op"].as_str() == "add" && me && (me->kind == "grudge" || me->kind == "promise")) text = tr("remembers");
                else if (m["op"].as_str() == "resolve") text = tr("lets_go");
                if (!text.empty()) { size_t p = text.find("{name}"); if (p != std::string::npos) text.replace(p, 6, l.name); toast(text); }
            }
            log_history(l);
            if (streaming_ && l.kind == "reply") {             // already on screen while it streamed: keep it, no replay
                l.total = cp_count(l.text); l.shown = static_cast<double>(l.total);
                line_ = l;
            } else if (!has_line_ || (line_.full() && talk_queue_.empty() && !streaming_ && l.kind != "interjection")) {
                stage_.set_speaker(l.speaker);
                show_line(std::move(l));
            } else {                                           // read what is on screen first
                l.total = cp_count(l.text);
                l.shown = text_speed_ <= 0 ? static_cast<double>(l.total) : 0.0;
                talk_queue_.push_back(std::move(l));
            }
            streaming_ = false;
        }
        else if (e.type == "offer") { offered_outcome_ = d["outcome"].as_str(); offered_label_ = d["label"].as_str(); }
        else if (e.type == "done") { talk_done_ = true; talk_done_outcome_ = d["outcome"].as_str(); talk_thinking_ = false; if (!has_line_ || (line_.full() && talk_queue_.empty())) finish_talk(talk_done_outcome_); }
        else if (e.type == "error") { talk_thinking_ = false; streaming_ = false; error_kind_ = "talk_turn"; show_error(tr("model_offline"), d["message"].as_str(), true); }
    }
}

// ------------------------------------------------------------------------------------------------ minigames

void Player::start_minigame(const Json& ev) {
    mg_event_ = ev;
    mg_progress_ = 0; mg_label_.clear();
    Json ctx = Json::object(), chars = Json::array();
    ctx.set("player", name_of("player", Json()));
    ctx.set("vars", vm_.vars());
    for (auto& kv : game_->characters) { Json c = Json::object(); c.set("id", kv.first); c.set("name", kv.second.name); c.set("willingness", hub_.of(kv.first).willingness()); chars.push(c); }
    ctx.set("characters", chars);
    try {
        mg_ = MinigameSession::launch(*game_, ev["id"].as_str(), ev["args"], ctx, opt_.cache_dir);
        mode_ = Mode::Minigame;
        has_line_ = false;
    } catch (const Error& e) {
        mg_.reset();
        error_kind_ = "minigame";
        mode_ = Mode::Running;
        show_error(tr("minigame_failed"), e.what(), true);
    }
}

void Player::answer_minigame_say(int64_t id, const std::string& character, const std::string& prompt) {
    LlmBackend* be = game_->characters.count(character) ? backend() : nullptr;
    if (!be) { mg_->answer_say(id, ""); return; }
    if (!sched_) sched_ = std::make_unique<LlmScheduler>(backend_);
    const CharacterDef& cd = game_->characters.at(character);
    std::string sys = game_->llm["system"].str_or("") + "\nYou are " + cd.name + ". Stay in character. Answer with one short spoken line and nothing else.\n" +
                      hub_.of(character).immutable().render() + "\n" + hub_.of(character).render_contextual();
    LlmRequest r;
    r.prompt = wrap_prompt(game_->llm["template"].str_or("chatml"), sys, prompt);
    r.max_tokens = 80;
    r.temperature = 0.8;
    if (game_->llm["template"].str_or("chatml") != "plain") r.stop.push_back("<|im_end|>");
    uint64_t job = sched_->submit(std::move(r), LlmPriority::Interject);
    mg_say_jobs_[id] = job; mg_say_by_job_[job] = id;
}

void Player::pump_minigame() {
    if (!mg_) return;
    std::vector<MgEvent> evs;
    mg_->poll(evs);
    for (auto& e : evs) {
        dirty_ = true;
        if (e.type == "progress") { mg_progress_ = e.data["value"].as_num(); mg_label_ = e.data["label"].as_str(); }
        else if (e.type == "say") answer_minigame_say(e.data["id"].as_int(), e.data["character"].as_str(), e.data["prompt"].as_str());
        else if (e.type == "result") {
            Json v = minigame_result_value(e.data["outcome"].as_str(), e.data["data"]);
            mg_.reset();
            vm_.finish_minigame(v);
            mode_ = Mode::Running;
            run_script();
            return;
        }
        else if (e.type == "failed") {
            error_kind_ = "minigame";
            mode_ = Mode::Minigame;
            show_error(tr("minigame_failed"), e.data["message"].as_str(), true);
            return;
        }
    }
    if (sched_ && !mg_say_by_job_.empty()) {          // lines for the minigame come back through the scheduler
        std::vector<LlmEvent> le;
        sched_->poll(le);
        for (auto& ev : le) {
            auto it = mg_say_by_job_.find(ev.id);
            if (it == mg_say_by_job_.end() || ev.type == LlmEvent::Delta) continue;
            if (mg_) mg_->answer_say(it->second, ev.type == LlmEvent::Done ? trim(ev.text) : "");
            mg_say_jobs_.erase(it->second); mg_say_by_job_.erase(it);
        }
    }
}

// ------------------------------------------------------------------------------------------------ saves

bool Player::save_slot(const std::string& slot) {
    Json st = Json::object();
    st.set("vm", vm_.save());
    st.set("memory", hub_.save());
    st.set("stage", stage_.save());
    st.set("music", music_);
    std::string summary = has_line_ ? (line_.name.empty() ? "" : line_.name + ": ") + snippet(line_.text, 70) : vm_.where();
    saves_.write(slot, st, summary);
    return true;
}

bool Player::load_slot(const std::string& slot) {
    try {
        Json st = saves_.read(slot);
        bool exact = vm_.load(st["vm"]);
        hub_.load(st["memory"]);
        stage_.load(st["stage"]);
        music_ = st["music"].as_str();
        if (opt_.audio) { if (music_.empty()) opt_.audio->stop_music(0); else opt_.audio->music(game_->resolve_audio(music_), 0, true); }
        pending_.clear(); has_line_ = false; conv_.reset(); mg_.reset(); talk_queue_.clear();
        mode_ = Mode::Running;
        run_script();
        if (!exact) toast(tr("update_warning"));
        else toast(tr("loaded"));
        return true;
    } catch (const Error& e) {
        toast(e.what());
        messages_.push_back(std::string("load failed: ") + e.what());
        return false;
    }
}

// ------------------------------------------------------------------------------------------------ update

bool Player::needs_frame() const {
    return dirty_ || stage_.busy() || !toasts_.empty() || mode_ == Mode::Talk || mode_ == Mode::Minigame || mode_ == Mode::Pause ||
           (has_line_ && !line_.full());
}

void Player::update(double dt) {
    clock_ += dt;
    if (stage_.busy() || !toasts_.empty()) dirty_ = true;
    stage_.update(dt);
    if (has_line_ && !line_.full()) {
        double cps = theme_.chars_per_second * (text_speed_ > 0 ? text_speed_ : 1e6);
        line_.shown = std::min<double>(static_cast<double>(line_.total), line_.shown + dt * cps);
        dirty_ = true;
    }
    for (auto& t : toasts_) t.age += dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [](const Toast& t) { return t.age > 3.6; }), toasts_.end());

    if (mode_ == Mode::Pause && pause_left_ > 0) { pause_left_ -= dt; if (pause_left_ <= 0) { vm_.advance(); mode_ = Mode::Running; } }
    if (mode_ == Mode::Talk) pump_talk(dt);
    if (mode_ == Mode::Minigame || (mode_ == Mode::Error && error_kind_ == "minigame")) pump_minigame();
    if (skipping_ && (mode_ == Mode::Say) && line_.full()) { vm_.advance(); mode_ = Mode::Running; }
    if (mode_ == Mode::Running) run_script();
    else if (mode_ == Mode::Say && skipping_) { line_.shown = static_cast<double>(line_.total); }
}

// ------------------------------------------------------------------------------------------------ input

void Player::handle(const Input& in) {
    dirty_ = true;
    if (in.type == Input::MouseMove) { mx_ = in.x; my_ = in.y; return; }
    if (in.type == Input::Wheel) {
        if (mode_ == Mode::History) history_scroll_ = std::max(0.0, history_scroll_ - in.wheel * 60.0);
        else if (in.wheel > 0 && mode_ == Mode::Say) { /* rollback is not supported yet; the history overlay shows what was said */ }
        return;
    }
    if (in.type == Input::MouseDown) { mx_ = in.x; my_ = in.y; click(in.x, in.y); return; }
    if (in.type == Input::Text) {
        if (mode_ == Mode::Talk && conv_) input_.insert(in.text);
        return;
    }
    if (in.type == Input::Key) key(in);
}

void Player::click(float x, float y) {
    Layout L = layout();
    switch (mode_) {
        case Mode::Say: case Mode::Pause:
            if (L.btn_menu.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::MenuOverlay; return; }
            if (L.btn_history.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::History; history_scroll_ = 1e9; return; }
            if (stage_.transitioning()) { stage_.finish_transition(); return; }
            advance();
            return;
        case Mode::Menu:
            if (L.btn_menu.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::MenuOverlay; return; }
            if (L.btn_history.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::History; history_scroll_ = 1e9; return; }
            for (size_t i = 0; i < L.choices.size(); ++i) if (L.choices[i].contains(x, y)) {
                vm_.choose(static_cast<int>(i)); mode_ = Mode::Running; has_line_ = false; run_script(); return;
            }
            return;
        case Mode::Talk: {
            if (L.btn_menu.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::MenuOverlay; return; }
            if (L.btn_history.contains(x, y)) { return_mode_ = mode_; mode_ = Mode::History; history_scroll_ = 1e9; return; }
            if (!conv_) return;
            if (L.leave.contains(x, y)) { conv_->leave(); return; }
            for (size_t i = 0; i < L.chips.size(); ++i) if (L.chips[i].contains(x, y)) { stage_.set_speaker(conv_->cast()[i]); pending_chip_ = conv_->cast()[i]; return; }
            if (!offered_outcome_.empty() && L.offer.contains(x, y)) { conv_->commit(offered_outcome_); return; }
            if (L.send.contains(x, y)) { { Input e; e.type = Input::Key; e.key = "enter"; key(e); } return; }
            if (L.box.contains(x, y) && !L.input.contains(x, y)) advance();
            return;
        }
        case Mode::Minigame:
            if (L.mg_cancel.contains(x, y)) {
                std::string fb = mg_ ? mg_->fallback_outcome() : "aborted";
                if (mg_) { mg_->cancel(); mg_.reset(); }
                vm_.finish_minigame(minigame_result_value(fb, Json::object()));
                mode_ = Mode::Running; run_script();
            }
            return;
        case Mode::Error:
            if (error_retry_ && L.retry.contains(x, y)) {
                mode_ = return_mode_;
                if (error_kind_ == "talk") { backend_.reset(); start_talk(talk_event_); }
                else if (error_kind_ == "talk_turn" && conv_) conv_->retry();
                else if (error_kind_ == "minigame") { mg_.reset(); start_minigame(mg_event_); }
                return;
            }
            if (L.skip.contains(x, y)) {
                if (error_kind_ == "script") { quit_ = true; return; }
                mode_ = return_mode_;
                if (error_kind_ == "talk" || error_kind_ == "talk_turn") {
                    std::string out = talk_event_["on_end"].as_str();
                    if (out.empty() && talk_event_["outcomes"].size()) out = talk_event_["outcomes"].at(0)["id"].as_str();
                    conv_.reset(); finish_talk(out);
                } else if (error_kind_ == "minigame") {
                    std::string fb = mg_ ? mg_->fallback_outcome() : game_->minigames.count(mg_event_["id"].as_str()) ? game_->minigames.at(mg_event_["id"].as_str()).manifest["fallback_outcome"].str_or("aborted") : "aborted";
                    mg_.reset();
                    vm_.finish_minigame(minigame_result_value(fb, Json::object()));
                    mode_ = Mode::Running; run_script();
                }
            }
            return;
        case Mode::MenuOverlay: {
            if (!L.panel.contains(x, y)) { mode_ = return_mode_; return; }
            for (size_t i = 0; i < L.overlay_buttons.size(); ++i) {
                if (!L.overlay_buttons[i].contains(x, y)) continue;
                switch (i) {
                    case 0: mode_ = return_mode_; break;
                    case 1: save_mode_ = true; mode_ = Mode::SaveLoad; break;
                    case 2: save_mode_ = false; mode_ = Mode::SaveLoad; break;
                    case 3: mode_ = Mode::History; history_scroll_ = 1e9; break;
                    case 4: {
                        static const double speeds[] = {1.0, 2.0, 0.0, 0.5};         // normal -> fast -> instant -> slow
                        size_t k = 0;
                        for (size_t j = 0; j < 4; ++j) if (speeds[j] == text_speed_) k = j;
                        text_speed_ = speeds[(k + 1) % 4];
                        try { Json st = saves_.read_settings(); st.set("text_speed", text_speed_); saves_.write_settings(st); } catch (const Error&) {}
                        break;
                    }
                    case 5: quit_ = true; break;
                }
                return;
            }
            return;
        }
        case Mode::SaveLoad:
            if (L.back.contains(x, y)) { mode_ = Mode::MenuOverlay; return; }
            for (size_t i = 0; i < L.slots.size(); ++i) if (L.slots[i].contains(x, y)) {
                std::string slot = std::to_string(i + 1);
                if (save_mode_) {
                    if (return_mode_ == Mode::Say || return_mode_ == Mode::Menu || return_mode_ == Mode::Talk || return_mode_ == Mode::Pause) {
                        try { save_slot(slot); toast(tr("saved")); } catch (const Error& e) { toast(e.what()); }
                    }
                } else if (saves_.exists(slot)) { load_slot(slot); }
                return;
            }
            return;
        case Mode::History:
            if (L.back.contains(x, y)) { mode_ = return_mode_; }
            return;
        case Mode::Ended:
            if (L.back.contains(x, y)) quit_ = true;
            return;
        default: return;
    }
}

void Player::key(const Input& in) {
    const std::string& k = in.key;
    if (k == "f5") { if (mode_ == Mode::Say || mode_ == Mode::Menu || mode_ == Mode::Pause) { try { save_slot("quick"); toast(tr("saved")); } catch (const Error& e) { toast(e.what()); } } return; }
    if (k == "f9") { if (saves_.exists("quick")) load_slot("quick"); return; }
    if (k == "escape") {
        if (mode_ == Mode::MenuOverlay || mode_ == Mode::History) mode_ = return_mode_;
        else if (mode_ == Mode::SaveLoad) mode_ = Mode::MenuOverlay;
        else if (mode_ == Mode::Say || mode_ == Mode::Menu || mode_ == Mode::Talk || mode_ == Mode::Pause) { return_mode_ = mode_; mode_ = Mode::MenuOverlay; }
        return;
    }
    switch (mode_) {
        case Mode::Say: case Mode::Pause:
            if (k == "enter" || k == "space" || k == "down") { if (stage_.transitioning()) stage_.finish_transition(); else advance(); }
            else if (k == "h") { return_mode_ = mode_; mode_ = Mode::History; history_scroll_ = 1e9; }
            else if (k == "ctrl_down") skipping_ = true;
            else if (k == "ctrl_up") skipping_ = false;
            break;
        case Mode::Menu: {
            Layout L = layout();
            if (k.size() == 1 && k[0] >= '1' && k[0] <= '9') {
                size_t i = static_cast<size_t>(k[0] - '1');
                if (i < L.choices.size()) { vm_.choose(static_cast<int>(i)); mode_ = Mode::Running; has_line_ = false; run_script(); }
            }
            break;
        }
        case Mode::Talk: {
            if (!conv_) break;
            bool can_type = !conv_->busy() && talk_queue_.empty() && line_.full() && !talk_done_;
            if (!has_line_) can_type = !conv_->busy() && !talk_done_;
            if (k == "enter") {
                if (!can_type) { advance(); break; }
                std::string text = trim(input_.text);
                if (text.empty()) { advance(); break; }
                std::string to = pending_chip_.empty() ? conv_->addressee() : pending_chip_;
                if (conv_->player_say(text, to)) {
                    pending_chip_.clear();
                    Line me; me.speaker = "player"; me.name = name_of("player", Json()); me.text = text; me.kind = "say"; me.color = theme_.text_dim;
                    log_history(me);
                    player_echo_ = text;
                    input_.clear();
                }
            }
            else if (k == "backspace") input_.backspace();
            else if (k == "delete") input_.del();
            else if (k == "left") input_.left();
            else if (k == "right") input_.right();
            else if (k == "home") input_.home();
            else if (k == "end") input_.end();
            else if (k == "tab") {
                auto& cast = conv_->cast();
                std::string cur = pending_chip_.empty() ? conv_->addressee() : pending_chip_;
                size_t i = std::find(cast.begin(), cast.end(), cur) - cast.begin();
                pending_chip_ = cast[(i + 1) % cast.size()];
                stage_.set_speaker(pending_chip_);
            }
            break;
        }
        default: break;
    }
}

// ------------------------------------------------------------------------------------------------ drawing

void Player::draw_line_box(Canvas& c, const Layout& L) {
    const bool talk = mode_ == Mode::Talk && conv_;
    const bool thinking = talk && talk_thinking_;
    const bool echo = thinking && !player_echo_.empty();
    if (!has_line_ && !thinking) return;

    Line shown = line_;
    std::string who_name = line_.name; Color who_color = line_.color;
    if (thinking) { who_name = name_of(talk_thinking_who_, Json()); who_color = color_of(talk_thinking_who_); }

    draw_panel(c, theme_, L.box, theme_.radius * 1.2f);
    const bool narration = !thinking && shown.kind == "narration";
    if (!who_name.empty() && !narration) {
        std::string label = who_name;
        if (!thinking && shown.kind == "interjection") label += "  \xC2\xB7  " + tr("interrupts");
        draw_name_tag(c, theme_, L.tag.x, L.tag.y, label, who_color, px_name());
        // thin accent line along the top edge in the speaker's colour
        c.fill({L.box.x + theme_.radius, L.box.y, L.box.w - 2 * theme_.radius, 2.0f}, who_color.with_alpha(0.55f));
    }

    const float px = px_text();
    Rect tr_ = L.text;
    if (thinking) {
        if (echo) {
            c.paragraph(theme_.fonts, player_echo_, px * 0.9f, tr_, theme_.text_dim, 1.2f, 2);
            draw_thinking_dots(c, {tr_.x, tr_.y + px * 3.0f, px * 0.9f, px * 0.7f}, clock_, who_color);
        } else draw_thinking_dots(c, {tr_.x, tr_.y + px * 0.4f, px * 0.9f, px * 0.7f}, clock_, who_color);
        return;
    }
    // Wrap the full text first so words never jump while the typewriter runs.
    auto lines = theme_.fonts.wrap(shown.text, px, tr_.w);
    const float lh = theme_.fonts.line_height(px) * 1.18f;
    size_t budget = static_cast<size_t>(shown.shown);
    float base = tr_.y + theme_.fonts.ascent(px);
    Color col = narration ? theme_.narration : theme_.text;
    for (auto& l : lines) {
        if (base - theme_.fonts.ascent(px) > L.text.bottom() + lh) break;
        size_t n = cp_count(l);
        if (budget == 0) break;
        std::string part = budget >= n ? l : cp_prefix(l, budget);
        c.text(theme_.fonts, part, px, tr_.x, base, col);
        budget = budget >= n ? budget - n : 0;
        base += lh;
    }
    if (shown.full() && (mode_ == Mode::Say || (talk && (!talk_queue_.empty() || talk_done_)))) {
        float ax = L.box.right() - 0.03f * w_, ay = talk ? L.text.bottom() - 2 : L.box.bottom() - 0.04f * h_;
        c.text(theme_.fonts, "\xE2\x96\xBC", px * 0.7f, ax, ay, who_color);
    }
}

void Player::draw_menu(Canvas& c, const Layout& L) {
    for (size_t i = 0; i < L.choices.size(); ++i) {
        const Rect& r = L.choices[i];
        bool hover = r.contains(mx_, my_);
        std::string label = menu_["options"].at(i)["text"].as_str();
        draw_button(c, theme_, r, label, px_text() * 0.92f, hover, true, false);
        // number hint, so choices can also be picked from the keyboard
        c.text(theme_.fonts_bold, std::to_string(i + 1), px_small() * 0.9f, r.x + 14, r.y + r.h / 2 + px_small() * 0.35f, theme_.text_dim);
    }
}

void Player::draw_talk_extras(Canvas& c, const Layout& L) {
    if (!conv_) return;
    const float spx = px_small();
    // who she is talking to
    const std::string cur = pending_chip_.empty() ? conv_->addressee() : pending_chip_;
    for (size_t i = 0; i < L.chips.size(); ++i) {
        const std::string& id = conv_->cast()[i];
        bool active = id == cur;
        bool hover = L.chips[i].contains(mx_, my_);
        Color col = color_of(id);
        c.fill_round(L.chips[i], L.chips[i].h / 2, active ? col.mix({10, 12, 22, 255}, 0.45f).with_alpha(0.95f) : theme_.button.with_alpha(hover ? 0.95f : 0.8f));
        c.stroke_round(L.chips[i], L.chips[i].h / 2, active ? 1.8f : 1.1f, active ? col : theme_.button_border);
        std::string n = name_of(id, Json());
        float w = theme_.fonts_bold.width(n, spx);
        c.text(theme_.fonts_bold, n, spx, L.chips[i].x + (L.chips[i].w - w) / 2, L.chips[i].y + L.chips[i].h / 2 + spx * 0.36f, active ? theme_.text : theme_.text_dim);
    }
    draw_button(c, theme_, L.leave, tr("leave"), spx, L.leave.contains(mx_, my_), true, false);
    if (!offered_outcome_.empty()) draw_button(c, theme_, L.offer, "\xE2\x96\xB6  " + offered_label_, spx, L.offer.contains(mx_, my_), true, true);

    // input row
    const bool can_type = !conv_->busy() && talk_queue_.empty() && (!has_line_ || line_.full()) && !talk_done_;
    c.fill_round(L.input, theme_.radius * 0.7f, theme_.input_bg);
    c.stroke_round(L.input, theme_.radius * 0.7f, can_type ? 1.6f : 1.0f, can_type ? theme_.accent.with_alpha(0.7f) : theme_.button_border);
    const float px = px_text() * 0.86f;
    const float base = L.input.y + L.input.h / 2 + px * 0.36f;
    c.set_clip(L.input.inset(4));
    if (input_.text.empty()) c.text(theme_.fonts, tr("say_something"), px, L.input.x + 16, base, theme_.text_dim.with_alpha(0.7f));
    else {
        // keep the caret visible by scrolling long text
        float cx_ = theme_.fonts.width(input_.text.substr(0, input_.cursor), px);
        float scroll = std::max(0.f, cx_ - (L.input.w - 40));
        c.text(theme_.fonts, input_.text, px, L.input.x + 16 - scroll, base, theme_.text);
        if (can_type) draw_caret(c, L.input.x + 16 + cx_ - scroll, L.input.y + 10, L.input.bottom() - 10, clock_, theme_.accent);
    }
    if (input_.text.empty() && can_type) draw_caret(c, L.input.x + 16, L.input.y + 10, L.input.bottom() - 10, clock_, theme_.accent);
    c.reset_clip();
    draw_button(c, theme_, L.send, tr("send"), spx * 1.05f, L.send.contains(mx_, my_) && can_type, can_type && !trim(input_.text).empty(), true);
}

void Player::draw_overlays(Canvas& c, const Layout& L) {
    const float W = static_cast<float>(w_), H = static_cast<float>(h_);
    auto dim = [&] { c.fill({0, 0, W, H}, theme_.overlay); };
    switch (mode_) {
        case Mode::MenuOverlay: {
            dim();
            draw_panel(c, theme_, L.panel, theme_.radius * 1.4f);
            static const char* keys[] = {"resume", "save", "load", "history", "", "quit"};
            for (size_t i = 0; i < L.overlay_buttons.size(); ++i) {
                std::string label = i == 4 ? tr("text_speed") + ": " + tr(text_speed_ <= 0 ? "speed_instant" : text_speed_ >= 2 ? "speed_fast" : text_speed_ >= 1 ? "speed_normal" : "speed_slow") : tr(keys[i]);
                bool can = !(i == 1 && !(return_mode_ == Mode::Say || return_mode_ == Mode::Menu || return_mode_ == Mode::Talk || return_mode_ == Mode::Pause));
                draw_button(c, theme_, L.overlay_buttons[i], label, px_text() * 0.85f, L.overlay_buttons[i].contains(mx_, my_), can, i == 0);
            }
            break;
        }
        case Mode::SaveLoad: {
            dim();
            std::string title = tr(save_mode_ ? "save" : "load");
            c.text(theme_.fonts_bold, title, px_text() * 1.2f, W / 2 - theme_.fonts_bold.width(title, px_text() * 1.2f) / 2, 0.11f * H, theme_.text);
            auto list = saves_.list();
            for (size_t i = 0; i < L.slots.size(); ++i) {
                const Rect& r = L.slots[i];
                std::string slot = std::to_string(i + 1);
                const SaveInfo* info = nullptr;
                for (auto& s : list) if (s.slot == slot) info = &s;
                bool hover = r.contains(mx_, my_);
                c.soft_shadow(r, theme_.radius, 10.f, {0, 0, 0, 255});
                c.fill_round(r, theme_.radius, hover ? theme_.button_hover : theme_.button);
                c.stroke_round(r, theme_.radius, hover ? 1.8f : 1.1f, hover ? theme_.accent : theme_.button_border);
                c.text(theme_.fonts_bold, slot, px_text(), r.x + 18, r.y + px_text() * 1.5f, theme_.accent);
                if (info && info->ok) {
                    c.text(theme_.fonts, format_time(info->time), px_small(), r.x + 18 + px_text() * 1.6f, r.y + px_text() * 1.5f, theme_.text_dim);
                    c.paragraph(theme_.fonts, info->summary, px_small() * 1.05f, {r.x + 18, r.y + px_text() * 2.1f, r.w - 36, r.h - px_text() * 2.3f}, theme_.text, 1.15f, 3);
                } else c.text(theme_.fonts, tr("empty_slot"), px_small() * 1.1f, r.x + 18, r.y + px_text() * 2.6f, theme_.text_dim.with_alpha(0.7f));
            }
            draw_button(c, theme_, L.back, tr("back"), px_small() * 1.1f, L.back.contains(mx_, my_));
            break;
        }
        case Mode::History: {
            dim();
            std::string title = tr("history");
            c.text(theme_.fonts_bold, title, px_text() * 1.2f, W / 2 - theme_.fonts_bold.width(title, px_text() * 1.2f) / 2, 0.095f * H, theme_.text);
            Rect area{0.16f * W, 0.13f * H, 0.68f * W, 0.74f * H};
            const float px = px_small() * 1.12f;
            // lay out bottom-up so the newest line is visible first
            struct Row { std::string name, text; Color color; std::vector<std::string> lines; float h; };
            std::vector<Row> rows;
            float total = 0;
            for (auto& hline : hist_) {
                Row r{hline.name, hline.text, hline.color, theme_.fonts.wrap(hline.text, px, area.w - 12), 0};
                r.h = (r.name.empty() ? 0 : px * 1.5f) + r.lines.size() * theme_.fonts.line_height(px) * 1.1f + px * 0.9f;
                total += r.h;
                rows.push_back(std::move(r));
            }
            if (rows.empty()) { c.text(theme_.fonts, tr("history_empty"), px, area.x, area.y + px * 2, theme_.text_dim); }
            double maxscroll = std::max(0.0, double(total - area.h));
            history_scroll_ = std::clamp(history_scroll_, 0.0, maxscroll);
            c.set_clip(area);
            float y = area.y - static_cast<float>(history_scroll_);
            for (auto& r : rows) {
                if (y + r.h >= area.y && y <= area.bottom()) {
                    float yy = y;
                    if (!r.name.empty()) { c.text(theme_.fonts_bold, r.name, px, area.x, yy + px * 1.1f, r.color.mix({255, 255, 255, 255}, 0.4f)); yy += px * 1.5f; }
                    for (auto& l : r.lines) { c.text(theme_.fonts, l, px, area.x + 6, yy + theme_.fonts.ascent(px), theme_.text); yy += theme_.fonts.line_height(px) * 1.1f; }
                }
                y += r.h;
            }
            c.reset_clip();
            draw_button(c, theme_, L.back, tr("back"), px_small() * 1.1f, L.back.contains(mx_, my_));
            break;
        }
        case Mode::Error: {
            dim();
            draw_panel(c, theme_, L.panel, theme_.radius * 1.3f);
            c.text(theme_.fonts_bold, error_title_, px_text() * 1.05f, L.panel.x + 0.025f * W, L.panel.y + px_text() * 2.0f, theme_.warn);
            c.paragraph(theme_.fonts, error_detail_, px_small() * 1.1f, {L.panel.x + 0.025f * W, L.panel.y + px_text() * 3.0f, L.panel.w - 0.05f * W, L.panel.h - px_text() * 5.5f}, theme_.text, 1.2f);
            if (error_retry_) draw_button(c, theme_, L.retry, tr("retry"), px_small() * 1.1f, L.retry.contains(mx_, my_), true, true);
            draw_button(c, theme_, L.skip, error_kind_ == "script" ? tr("quit") : tr("skip"), px_small() * 1.1f, L.skip.contains(mx_, my_));
            break;
        }
        case Mode::Minigame: {
            dim();
            draw_panel(c, theme_, L.panel, theme_.radius * 1.3f);
            std::string title = game_->minigames.count(mg_event_["id"].as_str()) ? game_->minigames.at(mg_event_["id"].as_str()).title : mg_event_["id"].as_str();
            c.text(theme_.fonts_bold, tr("minigame_playing") + ": " + title, px_text() * 1.05f, L.panel.x + 0.025f * W, L.panel.y + px_text() * 2.0f, theme_.accent);
            c.text(theme_.fonts, tr("minigame_hint"), px_small() * 1.05f, L.panel.x + 0.025f * W, L.panel.y + px_text() * 3.3f, theme_.text_dim);
            Rect bar{L.panel.x + 0.025f * W, L.panel.y + L.panel.h * 0.58f, L.panel.w - 0.05f * W, 8};
            c.fill_round(bar, 4, theme_.input_bg);
            c.fill_round({bar.x, bar.y, std::max(8.0f, bar.w * static_cast<float>(std::clamp(mg_progress_, 0.0, 1.0))), bar.h}, 4, theme_.accent);
            if (!mg_label_.empty()) c.text(theme_.fonts, mg_label_, px_small(), bar.x, bar.y + 34, theme_.text_dim);
            draw_button(c, theme_, L.mg_cancel, tr("cancel"), px_small() * 1.1f, L.mg_cancel.contains(mx_, my_));
            break;
        }
        case Mode::Ended: {
            c.fill({0, 0, W, H}, theme_.overlay);
            std::string t = ending_.empty() ? tr("the_end") : ending_;
            float px = px_text() * 2.0f;
            c.text(theme_.fonts_bold, t, px, W / 2 - theme_.fonts_bold.width(t, px) / 2, 0.5f * H, theme_.text);
            draw_button(c, theme_, L.back, tr("quit"), px_small() * 1.15f, L.back.contains(mx_, my_), true, true);
            break;
        }
        default: break;
    }
}

void Player::draw_toasts(Canvas& c) {
    float y = 0.075f * h_ + px_small() * 3.0f;
    for (auto& t : toasts_) {
        float in = std::min(1.0, t.age / 0.25), out = t.age > 3.1 ? std::max(0.0, 1.0 - (t.age - 3.1) / 0.5) : 1.0;
        float a = static_cast<float>(in * out);
        float px = px_small() * 1.05f;
        float w = theme_.fonts.width(t.text, px) + px * 2.4f, h = px * 2.5f;
        Rect r{w_ - w - 18 + (1 - static_cast<float>(in)) * 40, y, w, h};
        c.fill_round(r, h / 2, theme_.box.with_alpha(a));
        c.stroke_round(r, h / 2, 1.2f, theme_.accent.with_alpha(a * 0.7f));
        c.text(theme_.fonts, t.text, px, r.x + px * 1.2f, r.y + h / 2 + px * 0.35f, theme_.text.with_alpha(a));
        y += h + 8;
    }
}

void Player::render(Canvas& out) {
    stage_.render(out);
    Layout L = layout();
    const bool overlay = mode_ == Mode::MenuOverlay || mode_ == Mode::SaveLoad || mode_ == Mode::History || mode_ == Mode::Error || mode_ == Mode::Minigame || mode_ == Mode::Ended;
    if (!stage_.transitioning()) {
        if (mode_ == Mode::Say || mode_ == Mode::Talk || mode_ == Mode::Menu || mode_ == Mode::Pause || overlay) {
            if (mode_ != Mode::Pause) draw_line_box(out, L);
        }
        if (mode_ == Mode::Menu) draw_menu(out, L);
        if (mode_ == Mode::Talk) draw_talk_extras(out, L);
    }
    if (mode_ == Mode::Say || mode_ == Mode::Menu || mode_ == Mode::Talk || mode_ == Mode::Pause) {
        draw_button(out, theme_, L.btn_history, tr("history"), px_small() * 0.95f, L.btn_history.contains(mx_, my_));
        draw_button(out, theme_, L.btn_menu, tr("menu"), px_small() * 0.95f, L.btn_menu.contains(mx_, my_));
    }
    draw_overlays(out, L);
    draw_toasts(out);
    dirty_ = false;
}

} // namespace sf
