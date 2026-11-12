#include <numbers>
#include "stage.hpp"
#include <algorithm>
#include <cmath>

namespace sf {

bool is_known_motion(const std::string& n) { for (auto m : kMotionNames) if (n == m) return true; return false; }
bool is_known_transition(const std::string& n) {
    for (auto t : {"none", "cut", "fade", "dissolve", "slide_left", "slide_right", "wipe_left", "wipe_right", "flash"}) if (n == t) return true;
    return false;
}

namespace {
struct Offset { double dx = 0, dy = 0, scale = 1.0; };      // dx, dy in fractions of canvas width / height

double bump(double t) { return std::sin(std::clamp(t, 0.0, 1.0) * std::numbers::pi); }
double decay_wave(double t, double cycles, double amp) { return std::sin(t * cycles * 2 * std::numbers::pi) * amp * (1 - t); }

// Motion curves. t runs 0..1 over the motion's length. All amplitudes are small on purpose.
Offset motion_offset(const std::string& m, double t) {
    Offset o;
    if (m == "nod")            { o.dy = 0.014 * std::fabs(std::sin(t * 2 * std::numbers::pi)); }
    else if (m == "shake_head" || m == "shake") { o.dx = decay_wave(t, 3, 0.008); }
    else if (m == "tilt")      { o.dx = 0.006 * bump(t); o.dy = 0.004 * bump(t); }
    else if (m == "emphasize") { o.scale = 1.0 + 0.035 * bump(t); }
    else if (m == "chuckle")   { o.dy = -std::fabs(decay_wave(t, 3, 0.012)); }
    else if (m == "bashful")   { o.dy = 0.02 * bump(t); o.dx = -0.008 * bump(t); }
    else if (m == "reassure")  { o.scale = 1.0 + 0.025 * bump(t); o.dy = 0.006 * bump(t); }
    else if (m == "sigh")      { o.scale = 1.0 - 0.012 * bump(t); o.dy = 0.010 * bump(t); }
    else if (m == "glance")    { o.dx = 0.010 * std::sin(t * 2 * std::numbers::pi); }
    else if (m == "hop")       { o.dy = -0.05 * bump(t); }
    else if (m == "bob")       { o.dy = -0.012 * std::fabs(std::sin(t * 2 * std::numbers::pi)); }
    else if (m == "breathe")   { o.scale = 1.0 + 0.012 * bump(t); }
    return o;
}
double motion_length(const std::string& m) {
    if (m == "shake_head" || m == "shake" || m == "chuckle") return 0.9;
    if (m == "sigh" || m == "breathe" || m == "reassure") return 1.4;
    if (m == "hop") return 0.5;
    return 0.8;
}
}

Stage::Stage(const Game& game, AssetCache& assets, const Theme& theme) : game_(game), assets_(assets), theme_(theme), w_(game.canvas_w), h_(game.canvas_h) {
    live_ = std::make_unique<Canvas>(w_, h_);
}

Stage::Sprite* Stage::find(const std::string& id) { for (auto& s : sprites_) if (s.id == id) return &s; return nullptr; }

std::vector<std::string> Stage::present() const { std::vector<std::string> v; for (auto& s : sprites_) v.push_back(s.id); return v; }

void Stage::begin_change() {
    if (snap_taken_ || trans_active_) return;
    if (!snap_) snap_ = std::make_unique<Canvas>(w_, h_);
    render_scene(*snap_, clock_);
    snap_taken_ = true;
}

void Stage::apply(const Event& e) {
    const Json& d = e.data;
    if (e.type == "scene") {
        begin_change();
        bg_ = d["bg"].as_str();
        sprites_.clear();                       // a new scene starts empty, like in Ren'Py
    } else if (e.type == "show") {
        begin_change();
        std::string id = d["char"].as_str();
        const CharacterDef* cd = game_.character(id);
        Sprite* s = find(id);
        if (!s) { sprites_.push_back({}); s = &sprites_.back(); s->id = id; s->order = next_order_++; s->pose = cd ? cd->default_pose : ""; s->x = 0.5; }
        if (d.has("pose") && d["pose"].as_str() != s->pose) { s->prev_pose = s->pose; s->pose = d["pose"].as_str(); s->pose_t = 0; }
        if (d.has("at")) { double x = game_.position(d["at"].as_str()); if (x >= 0) s->x = x; }
    } else if (e.type == "hide") {
        begin_change();
        std::string id = d["char"].as_str();
        sprites_.erase(std::remove_if(sprites_.begin(), sprites_.end(), [&](const Sprite& s) { return s.id == id; }), sprites_.end());
    } else if (e.type == "pose") {
        if (Sprite* s = find(d["char"].as_str())) {
            if (d["pose"].as_str() != s->pose) { s->prev_pose = s->pose; s->pose = d["pose"].as_str(); s->pose_t = 0; }
        }
    } else if (e.type == "anim") {
        if (Sprite* s = find(d["char"].as_str())) { s->motion = d["motion"].as_str(); s->motion_t = 0; s->motion_len = motion_length(s->motion); }
    } else if (e.type == "with") {
        start_transition(d["transition"].as_str(), d["duration"].as_num(-1));
    }
}

void Stage::start_transition(const std::string& kind, double seconds) {
    if (!snap_taken_) return;                                   // nothing changed: nothing to transition
    snap_taken_ = false;
    if (kind == "none" || kind == "cut" || kind.empty() || seconds == 0) return;
    if (!from_) from_ = std::make_unique<Canvas>(w_, h_);
    from_->copy_from(*snap_);
    trans_kind_ = kind;
    trans_len_ = seconds > 0 ? seconds : (kind == "flash" ? 0.7 : kind.rfind("slide", 0) == 0 || kind.rfind("wipe", 0) == 0 ? 0.6 : 0.5);
    trans_t_ = 0;
    trans_active_ = true;
}

void Stage::end_batch() { snap_taken_ = false; }

void Stage::finish_transition() { trans_active_ = false; }

void Stage::set_speaker(const std::string& id) {
    speaker_ = id;
    for (auto& s : sprites_) s.bright_target = (id.empty() || s.id == id) ? 1.0 : 0.74;
}

bool Stage::busy() const {
    if (trans_active_) return true;
    for (auto& s : sprites_) {
        if (!s.motion.empty() || s.pose_t < 1.0 || std::fabs(s.bright - s.bright_target) > 0.004) return true;
        if (theme_.idle_motion) return true;
    }
    return false;
}

void Stage::update(double dt) {
    clock_ += dt;
    if (trans_active_) { trans_t_ += dt / std::max(0.05, trans_len_); if (trans_t_ >= 1.0) trans_active_ = false; }
    for (auto& s : sprites_) {
        if (s.pose_t < 1.0) s.pose_t = std::min(1.0, s.pose_t + dt / 0.14);
        if (!s.motion.empty()) { s.motion_t += dt; if (s.motion_t >= s.motion_len) { s.motion.clear(); s.motion_t = 0; } }
        double k = 1.0 - std::exp(-dt * 12.0);
        s.bright += (s.bright_target - s.bright) * k;
        if (std::fabs(s.bright - s.bright_target) < 0.004) s.bright = s.bright_target;
    }
}

void Stage::draw_sprite(Canvas& out, Sprite& s) {
    const CharacterDef* cd = game_.character(s.id);
    if (!cd) return;
    auto pose_path = [&](const std::string& pose) -> std::string {
        auto it = cd->poses.find(pose);
        if (it != cd->poses.end()) return it->second;
        it = cd->poses.find(cd->default_pose);
        return it == cd->poses.end() ? std::string() : it->second;
    };
    std::string path = pose_path(s.pose);
    if (path.empty()) return;
    auto img = assets_.image(path);
    double target_h = h_ * theme_.sprite_height * cd->scale;
    double scale = target_h / img->h;
    double dw = img->w * scale, dh = target_h;

    Offset off;
    if (!s.motion.empty()) off = motion_offset(s.motion, s.motion_t / s.motion_len);
    if (theme_.idle_motion) off.scale *= 1.0 + 0.004 * std::sin(clock_ * 1.6 + s.order);
    dw *= off.scale; dh *= off.scale;

    double ax = s.x * w_ + off.dx * w_;
    double ay = h_ + (cd->y_offset + off.dy) * h_;
    Rect dst{static_cast<float>(ax - cd->anchor_x * dw), static_cast<float>(ay - cd->anchor_y * dh), static_cast<float>(dw), static_cast<float>(dh)};

    // resize the source to its resting size once (cached), instead of resampling every frame
    std::shared_ptr<const Image> use = img;
    if (std::fabs(img->h - target_h) >= 1.0) use = assets_.scaled(path, std::max(1, static_cast<int>(img->w * target_h / img->h + 0.5)), std::max(1, static_cast<int>(target_h + 0.5)));
    // At rest the pre-scaled image already has its final size: draw it 1:1 on whole pixels, which skips resampling entirely.
    // (Sprites with a scale motion fall back to the resampling path; that is only for the length of the motion.)
    auto snap = [](Rect& r, const Image& im) {
        if (std::fabs(r.w - im.w) < 1.0f && std::fabs(r.h - im.h) < 1.0f) { r.x = std::round(r.x); r.y = std::round(r.y); r.w = static_cast<float>(im.w); r.h = static_cast<float>(im.h); }
    };
    snap(dst, *use);
    if (s.pose_t < 1.0 && !s.prev_pose.empty()) {
        std::string pp = pose_path(s.prev_pose);
        if (!pp.empty()) {
            auto old = assets_.image(pp);
            std::shared_ptr<const Image> old_use = old;
            if (std::fabs(old->h - target_h) >= 1.0) old_use = assets_.scaled(pp, std::max(1, static_cast<int>(old->w * target_h / old->h + 0.5)), std::max(1, static_cast<int>(target_h + 0.5)));
            double ow = old->w * (target_h / old->h) * off.scale;
            Rect odst{static_cast<float>(ax - cd->anchor_x * ow), dst.y, static_cast<float>(ow), dst.h};
            snap(odst, *old_use);
            out.blit(*old_use, odst, static_cast<float>(1.0 - s.pose_t), static_cast<float>(s.bright));
        }
        out.blit(*use, dst, static_cast<float>(s.pose_t), static_cast<float>(s.bright));
        return;
    }
    out.blit(*use, dst, 1.f, static_cast<float>(s.bright));
}

void Stage::render_scene(Canvas& out, double) {
    out.clear({0, 0, 0, 255});
    if (!bg_.empty()) {
        std::string path = game_.resolve_background(bg_);
        if (!path.empty()) {
            auto img = assets_.image(path);
            double s = std::max(double(w_) / img->w, double(h_) / img->h);
            int sw = std::max(w_, static_cast<int>(std::ceil(img->w * s))), sh = std::max(h_, static_cast<int>(std::ceil(img->h * s)));
            auto use = (sw < img->w || sh < img->h) ? assets_.scaled(path, sw, sh) : img;
            double dx = (w_ - sw) / 2.0, dy = (h_ - sh) / 2.0;
            out.blit(*use, {static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(sw), static_cast<float>(sh)});
        }
    }

    std::vector<Sprite*> order;
    for (auto& s : sprites_) order.push_back(&s);
    std::sort(order.begin(), order.end(), [](Sprite* a, Sprite* b) { return a->order < b->order; });
    for (auto* s : order) draw_sprite(out, *s);
}

void Stage::render(Canvas& out) {
    if (!trans_active_) { render_scene(out, clock_); return; }
    if (!scratch_) scratch_ = std::make_unique<Canvas>(w_, h_);
    render_scene(*scratch_, clock_);
    float t = static_cast<float>(trans_t_);
    if (trans_kind_ == "fade") {                            // dip through black, like a film cut
        if (!black_) { black_ = std::make_unique<Canvas>(w_, h_); black_->clear({0, 0, 0, 255}); }
        if (t < 0.5f) out.mix(*from_, *black_, t * 2); else out.mix(*black_, *scratch_, (t - 0.5f) * 2);
    }
    else if (trans_kind_ == "slide_left")  out.slide(*from_, *scratch_, t, -1);
    else if (trans_kind_ == "slide_right") out.slide(*from_, *scratch_, t, +1);
    else if (trans_kind_ == "wipe_left")   out.wipe(*from_, *scratch_, t, -1, w_ * 0.12f);
    else if (trans_kind_ == "wipe_right")  out.wipe(*from_, *scratch_, t, +1, w_ * 0.12f);
    else if (trans_kind_ == "flash")       out.flash(*from_, *scratch_, t);
    else                                   out.mix(*from_, *scratch_, t);   // dissolve
}

Json Stage::save() const {
    Json j = Json::object(), arr = Json::array();
    j.set("bg", bg_);
    for (auto& s : sprites_) { Json o = Json::object(); o.set("id", s.id); o.set("pose", s.pose); o.set("x", s.x); o.set("order", s.order); arr.push(o); }
    j.set("sprites", arr);
    return j;
}

void Stage::load(const Json& j) {
    bg_ = j["bg"].as_str();
    sprites_.clear();
    for (auto& o : j["sprites"].items()) {
        Sprite s; s.id = o["id"].as_str(); s.pose = o["pose"].as_str(); s.x = o["x"].as_num(0.5); s.order = static_cast<int>(o["order"].as_int());
        next_order_ = std::max(next_order_, s.order + 1);
        if (game_.character(s.id)) sprites_.push_back(std::move(s));
    }
    snap_taken_ = trans_active_ = false;
}

} // namespace sf
