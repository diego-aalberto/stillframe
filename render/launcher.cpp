#include "launcher.hpp"
#include "../core/vfs.hpp"
#include <algorithm>
#include <cmath>

namespace sf {

Launcher::Launcher(std::string games_dir, int w, int h) : dir_(std::move(games_dir)), w_(w), h_(h), theme_(Theme::engine_default()) { refresh(); }

void Launcher::refresh() { games_ = scan_games(dir_); covers_.clear(); }

std::vector<Launcher::Card> Launcher::cards() const {
    std::vector<Card> out;
    const float W = static_cast<float>(w_), H = static_cast<float>(h_);
    const int cols = 3;
    const float gap = 0.022f * W, cw = (W * 0.84f - gap * (cols - 1)) / cols, ch = cw * 0.78f;
    const float x0 = (W - (cw * cols + gap * (cols - 1))) / 2, y0 = 0.25f * H - static_cast<float>(scroll_);
    for (size_t i = 0; i < games_.size(); ++i) out.push_back({{x0 + (i % cols) * (cw + gap), y0 + (i / cols) * (ch + gap), cw, ch}, i});
    return out;
}

std::shared_ptr<const Image> Launcher::cover(const GameEntry& g) {
    auto it = covers_.find(g.path);
    if (it != covers_.end()) return it->second;
    std::shared_ptr<const Image> img;
    try {
        auto vfs = Vfs::open(g.path);
        Json m = Json::parse(vfs->read_text("game.json"));
        std::string c = m["cover"].as_str();
        if (!c.empty() && vfs->exists(c)) {
            Image full = png_decode(vfs->read(c));
            double s = std::max(0.25, 480.0 / full.w);
            img = std::make_shared<const Image>(full.resized(std::max(1, static_cast<int>(full.w * s)), std::max(1, static_cast<int>(full.h * s))));
        }
    } catch (const Error&) {}
    covers_[g.path] = img;
    return img;
}

void Launcher::handle(const Input& in) {
    if (in.type == Input::MouseMove) { mx_ = in.x; my_ = in.y; }
    else if (in.type == Input::Wheel) { scroll_ = std::max(0.0, scroll_ - in.wheel * 50.0); }
    else if (in.type == Input::MouseDown) {
        mx_ = in.x; my_ = in.y;
        for (auto& c : cards()) if (c.r.contains(in.x, in.y) && c.r.y > 0.2f * h_ - 10) { chosen_ = games_[c.index].path; return; }
    }
    else if (in.type == Input::Key) {
        if (in.key == "escape") quit_ = true;
        else if (in.key == "f5") refresh();
        else if (in.key == "enter" && games_.size() == 1) chosen_ = games_[0].path;
    }
}

void Launcher::render(Canvas& c) {
    const float W = static_cast<float>(w_), H = static_cast<float>(h_);
    // soft vertical gradient with a faint accent glow
    c.fill_gradient_v({0, 0, W, H}, {18, 20, 36, 255}, {34, 28, 52, 255});
    for (int i = 0; i < 6; ++i) c.circle(W * 0.85f, -H * 0.05f, H * (0.7f - i * 0.1f), theme_.accent.with_alpha(0.012f));

    c.text(theme_.fonts_bold, "Stillframe", 0.072f * H, 0.08f * W, 0.135f * H, theme_.text);
    c.text(theme_.fonts, "Your games", 0.026f * H, 0.08f * W + 4, 0.185f * H, theme_.text_dim);

    c.set_clip({0, 0.2f * H, W, H * 0.8f});
    if (games_.empty()) {
        c.paragraph(theme_.fonts, "No games found yet.\nDrop a game folder or a .stillgame file into:\n" + dir_, 0.03f * H, {0.08f * W, 0.3f * H, 0.8f * W, 0.3f * H}, theme_.text_dim, 1.3f);
    }
    for (auto& card : cards()) {
        const Rect& r = card.r;
        if (r.bottom() < 0.2f * H || r.y > H) continue;
        const GameEntry& g = games_[card.index];
        bool hover = r.contains(mx_, my_) && my_ > 0.2f * H;
        float lift = hover ? -4.f : 0.f;
        Rect rr{r.x, r.y + lift, r.w, r.h};
        c.soft_shadow(rr, theme_.radius, hover ? 26.f : 16.f, {0, 0, 0, 255});
        c.fill_round(rr, theme_.radius, theme_.button);
        // cover art or a generated colour field
        Rect art{rr.x + 8, rr.y + 8, rr.w - 16, rr.h * 0.62f};
        c.fill_round(art, theme_.radius * 0.7f, {60, 64, 100, 255});
        auto img = cover(g);
        if (img) {
            c.set_clip(art);
            double s = std::max(art.w / img->w, art.h / img->h);
            float dw = static_cast<float>(img->w * s), dh = static_cast<float>(img->h * s);
            c.blit(*img, {art.x + (art.w - dw) / 2, art.y + (art.h - dh) / 2, dw, dh});
            c.set_clip({0, 0.2f * H, W, H * 0.8f});
        } else {
            uint32_t h = 2166136261u;
            for (char ch : g.id) h = (h ^ static_cast<uint8_t>(ch)) * 16777619u;
            Color a{static_cast<uint8_t>(70 + h % 90), static_cast<uint8_t>(70 + (h >> 8) % 90), static_cast<uint8_t>(90 + (h >> 16) % 110), 255};
            c.fill_gradient_v(art.inset(1), a, a.mix({14, 16, 30, 255}, 0.7f));
            std::string initial = g.title.empty() ? "?" : utf8_encode(utf8_decode(g.title)[0]);
            float px = art.h * 0.5f;
            c.text(theme_.fonts_bold, initial, px, art.x + (art.w - theme_.fonts_bold.width(initial, px)) / 2, art.y + art.h * 0.72f, {255, 255, 255, 90});
        }
        c.stroke_round(rr, theme_.radius, hover ? 2.0f : 1.2f, hover ? theme_.accent : theme_.button_border);
        c.text(theme_.fonts_bold, ellipsize(theme_.fonts_bold, g.title, 0.03f * H, rr.w - 32), 0.03f * H, rr.x + 16, rr.y + rr.h * 0.62f + 0.075f * H, theme_.text);
        std::string meta = (g.author.empty() ? "" : g.author + "  \xC2\xB7  ") + "v" + g.version;
        c.text(theme_.fonts, ellipsize(theme_.fonts, meta, 0.022f * H, rr.w - 32), 0.022f * H, rr.x + 16, rr.y + rr.h * 0.62f + 0.112f * H, theme_.text_dim);
    }
    c.reset_clip();
    if (!notice_.empty()) {
        float px = 0.022f * H, w = theme_.fonts.width(notice_, px) + px * 3;
        Rect r{(W - w) / 2, H - 0.09f * H, w, px * 2.6f};
        c.fill_round(r, r.h / 2, theme_.box);
        c.stroke_round(r, r.h / 2, 1.3f, theme_.warn);
        c.text(theme_.fonts, notice_, px, r.x + px * 1.5f, r.y + r.h / 2 + px * 0.35f, theme_.warn);
    }
}

} // namespace sf
