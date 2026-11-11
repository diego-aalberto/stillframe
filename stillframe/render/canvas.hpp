// A software canvas (RGBA8, always opaque) and the drawing operations the visual novel UI needs.
#pragma once
#include "font.hpp"
#include "png.hpp"
#include <algorithm>

namespace sf {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    static Color parse(const std::string& s, Color fallback = {255, 255, 255, 255});   // "#rrggbb", "#rrggbbaa", "#rgb"
    Color with_alpha(float f) const { Color c = *this; c.a = static_cast<uint8_t>(std::clamp(a * f, 0.f, 255.f)); return c; }
    Color mix(Color o, float t) const;
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};

class Canvas {
public:
    Canvas(int w, int h);
    int width() const { return w_; }
    int height() const { return h_; }
    const std::vector<uint8_t>& pixels() const { return px_; }
    Image to_image() const;

    void clear(Color c);
    void copy_from(const Canvas& o);
    void set_clip(const Rect& r);
    void reset_clip();

    void fill(const Rect& r, Color c);
    void fill_round(const Rect& r, float radius, Color c);
    void stroke_round(const Rect& r, float radius, float thickness, Color c);
    void fill_gradient_v(const Rect& r, Color top, Color bottom);
    void soft_shadow(const Rect& r, float radius, float spread, Color c);    // cheap layered shadow behind a box
    void circle(float cx, float cy, float rad, Color c);

    // `src` must be premultiplied. Bilinear, with opacity, brightness (1 = unchanged) and optional mirroring.
    void blit(const Image& src, const Rect& dst, float opacity = 1.f, float brightness = 1.f, bool mirror = false);
    // Draws a region of `src` (premultiplied) 1:1 or scaled into dst.
    void blit_region(const Image& src, const Rect& srcr, const Rect& dst, float opacity = 1.f);

    float text(const FontSet& f, const std::string& s, float px, float x, float baseline, Color c);   // returns the pen x after the text
    // Wrapped paragraph; returns the number of lines drawn. `max_lines` < 0 = unlimited.
    int paragraph(const FontSet& f, const std::string& s, float px, const Rect& box, Color c, float line_height_scale = 1.0f, int max_lines = -1);

    // Transitions between two full frames, written into *this.
    void mix(const Canvas& a, const Canvas& b, float t);                              // dissolve
    void slide(const Canvas& a, const Canvas& b, float t, int dir);                   // dir -1: new comes from the right, +1: from the left
    void wipe(const Canvas& a, const Canvas& b, float t, int dir, float feather);    // soft edge
    void flash(const Canvas& a, const Canvas& b, float t);                            // white flash

private:
    void put(int x, int y, Color c, float cov);
    int w_, h_;
    std::vector<uint8_t> px_;
    int cx0_ = 0, cy0_ = 0, cx1_, cy1_;
};

} // namespace sf
