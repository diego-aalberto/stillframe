#include "ui.hpp"
#include <cmath>

namespace sf {

void TextInput::insert(const std::string& s) {
    std::string clean;
    for (char ch : s) if (ch != '\n' && ch != '\r' && ch != '\t') clean += ch; else clean += ' ';
    if (text.size() + clean.size() > max_bytes) return;
    text.insert(cursor, clean);
    cursor += clean.size();
}
void TextInput::backspace() { if (cursor == 0) return; size_t p = utf8_prev(text, cursor); text.erase(p, cursor - p); cursor = p; }
void TextInput::del() {
    if (cursor >= text.size()) return;
    size_t n = cursor + 1;
    while (n < text.size() && (static_cast<unsigned char>(text[n]) >> 6) == 2) ++n;
    text.erase(cursor, n - cursor);
}
void TextInput::left() { cursor = utf8_prev(text, cursor); }
void TextInput::right() {
    if (cursor >= text.size()) return;
    ++cursor;
    while (cursor < text.size() && (static_cast<unsigned char>(text[cursor]) >> 6) == 2) ++cursor;
}

void draw_panel(Canvas& c, const Theme& t, const Rect& r, float radius, bool shadow) {
    if (shadow) c.soft_shadow(r, radius, 22.f, {0, 0, 0, 255});
    c.fill_round(r, radius, t.box);
    c.stroke_round(r, radius, 1.3f, t.box_border);
}

void draw_button(Canvas& c, const Theme& t, const Rect& r, const std::string& label, float px, bool hover, bool enabled, bool primary) {
    Color bg = hover && enabled ? t.button_hover : t.button;
    if (primary) bg = t.button_hover.mix(bg, hover ? 0.0f : 0.25f);
    if (!enabled) bg = bg.with_alpha(0.5f);
    c.soft_shadow(r, t.radius * 0.7f, 8.f, {0, 0, 0, hover ? uint8_t(200) : uint8_t(150)});
    c.fill_round(r, t.radius * 0.7f, bg);
    c.stroke_round(r, t.radius * 0.7f, hover && enabled ? 1.8f : (primary ? 1.6f : 1.2f), (hover || primary) && enabled ? t.accent.with_alpha(0.85f) : t.button_border);
    std::string text = ellipsize(t.fonts_bold, label, px, r.w - 2 * px);
    float w = t.fonts_bold.width(text, px);
    float base = r.y + r.h / 2 + (t.fonts_bold.ascent(px) - (t.fonts_bold.line_height(px) - t.fonts_bold.ascent(px))) / 2;
    c.text(t.fonts_bold, text, px, r.x + (r.w - w) / 2, base, enabled ? t.text : t.text_dim);
}

Rect draw_name_tag(Canvas& c, const Theme& t, float x, float y, const std::string& name, Color color, float px) {
    float w = t.fonts_bold.width(name, px) + px * 1.6f, h = px * 1.9f;
    Rect r{x, y, w, h};
    c.soft_shadow(r, h / 2, 8.f, {0, 0, 0, 255});
    c.fill_round(r, h / 2, color.mix({12, 14, 24, 255}, 0.55f));
    c.stroke_round(r, h / 2, 1.5f, color);
    float base = y + h / 2 + (t.fonts_bold.ascent(px) - (t.fonts_bold.line_height(px) - t.fonts_bold.ascent(px))) / 2;
    c.text(t.fonts_bold, name, px, x + px * 0.8f, base, color.mix({255, 255, 255, 255}, 0.55f));
    return r;
}

void draw_thinking_dots(Canvas& c, const Rect& r, double time, Color col) {
    float rad = r.h * 0.5f;
    for (int i = 0; i < 3; ++i) {
        double ph = std::sin(time * 5.0 - i * 0.9);
        float a = static_cast<float>(0.35 + 0.65 * (ph * 0.5 + 0.5));
        c.circle(r.x + rad + i * rad * 3.0f, r.y + r.h / 2 - static_cast<float>(ph) * rad * 0.35f, rad * 0.75f, col.with_alpha(a));
    }
}

void draw_caret(Canvas& c, float x, float y0, float y1, double time, Color col) {
    if (std::fmod(time, 1.0) < 0.55) c.fill({x, y0, 1.6f, y1 - y0}, col);
}

std::string ellipsize(const FontSet& f, const std::string& s, float px, float max_w) {
    if (f.width(s, px) <= max_w) return s;
    std::string cur = s;
    while (!cur.empty() && f.width(cur + "\xE2\x80\xA6", px) > max_w) cur.erase(utf8_prev(cur, cur.size()));
    return cur + "\xE2\x80\xA6";
}

} // namespace sf
