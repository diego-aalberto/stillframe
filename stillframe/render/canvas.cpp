#include "canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

namespace sf {

Color Color::parse(const std::string& s, Color fb) {
    if (s.empty() || s[0] != '#') return fb;
    auto hex = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; c |= 0x20; if (c >= 'a' && c <= 'f') return c - 'a' + 10; return -1; };
    std::string h = s.substr(1);
    for (char c : h) if (hex(c) < 0) return fb;
    if (h.size() == 3) { h = std::string{h[0], h[0], h[1], h[1], h[2], h[2]}; }
    if (h.size() != 6 && h.size() != 8) return fb;
    auto byte = [&](size_t i) { return static_cast<uint8_t>(hex(h[i]) * 16 + hex(h[i + 1])); };
    return {byte(0), byte(2), byte(4), h.size() == 8 ? byte(6) : uint8_t(255)};
}

Color Color::mix(Color o, float t) const {
    auto m = [&](int a, int b) { return static_cast<uint8_t>(std::clamp(a + (b - a) * t + 0.5f, 0.f, 255.f)); };
    return {m(r, o.r), m(g, o.g), m(b, o.b), m(a, o.a)};
}

Canvas::Canvas(int w, int h) : w_(w), h_(h), px_(static_cast<size_t>(w) * h * 4, 0), cx1_(w), cy1_(h) {
    for (size_t i = 3; i < px_.size(); i += 4) px_[i] = 255;
}

Image Canvas::to_image() const { Image i; i.w = w_; i.h = h_; i.px = px_; return i; }

void Canvas::clear(Color c) {
    for (size_t i = 0; i < px_.size(); i += 4) { px_[i] = c.r; px_[i + 1] = c.g; px_[i + 2] = c.b; px_[i + 3] = 255; }
}
void Canvas::copy_from(const Canvas& o) { if (o.w_ == w_ && o.h_ == h_) px_ = o.px_; }
void Canvas::set_clip(const Rect& r) {
    cx0_ = std::max(0, static_cast<int>(std::floor(r.x))); cy0_ = std::max(0, static_cast<int>(std::floor(r.y)));
    cx1_ = std::min(w_, static_cast<int>(std::ceil(r.right()))); cy1_ = std::min(h_, static_cast<int>(std::ceil(r.bottom())));
}
void Canvas::reset_clip() { cx0_ = cy0_ = 0; cx1_ = w_; cy1_ = h_; }

void Canvas::put(int x, int y, Color c, float cov) {
    if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
    float a = cov * c.a / 255.0f;
    if (a <= 0) return;
    uint8_t* d = &px_[(static_cast<size_t>(y) * w_ + x) * 4];
    if (a >= 0.999f) { d[0] = c.r; d[1] = c.g; d[2] = c.b; return; }
    d[0] = static_cast<uint8_t>(d[0] + (c.r - d[0]) * a + 0.5f);
    d[1] = static_cast<uint8_t>(d[1] + (c.g - d[1]) * a + 0.5f);
    d[2] = static_cast<uint8_t>(d[2] + (c.b - d[2]) * a + 0.5f);
}

void Canvas::fill(const Rect& r, Color c) {
    int x0 = std::max(cx0_, static_cast<int>(std::floor(r.x + 0.5f))), x1 = std::min(cx1_, static_cast<int>(std::floor(r.right() + 0.5f)));
    int y0 = std::max(cy0_, static_cast<int>(std::floor(r.y + 0.5f))), y1 = std::min(cy1_, static_cast<int>(std::floor(r.bottom() + 0.5f)));
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) put(x, y, c, 1.0f);
}

static float round_cov(float px, float py, const Rect& r, float rad) {
    // signed distance to a rounded rectangle, converted to 1-pixel anti-aliased coverage
    float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    float qx = std::fabs(px - cx) - (r.w / 2 - rad), qy = std::fabs(py - cy) - (r.h / 2 - rad);
    float d = std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) + std::max(qy, 0.f) * std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - rad;
    return std::clamp(0.5f - d, 0.f, 1.f);
}

void Canvas::fill_round(const Rect& r, float radius, Color c) {
    radius = std::clamp(radius, 0.f, std::min(r.w, r.h) / 2);
    int x0 = std::max(cx0_, static_cast<int>(std::floor(r.x)) - 1), x1 = std::min(cx1_, static_cast<int>(std::ceil(r.right())) + 1);
    int y0 = std::max(cy0_, static_cast<int>(std::floor(r.y)) - 1), y1 = std::min(cy1_, static_cast<int>(std::ceil(r.bottom())) + 1);
    // Pixels inside the "core" rectangle are fully covered: blend them as plain spans. Only the ring around it
    // (the rounded edge, ~radius+1 px wide) needs per-pixel coverage. This makes big translucent panels ~6x cheaper.
    const float m = radius + 1.5f;
    const int cx_a = static_cast<int>(std::ceil(r.x + m)), cx_b = static_cast<int>(std::floor(r.right() - m));
    const int cy_a = static_cast<int>(std::ceil(r.y + m)), cy_b = static_cast<int>(std::floor(r.bottom() - m));
    const bool has_core = cx_b > cx_a && cy_b > cy_a;
    const int a = c.a;
    for (int y = y0; y < y1; ++y) {
        if (!has_core || y < cy_a || y >= cy_b) {
            for (int x = x0; x < x1; ++x) put(x, y, c, round_cov(x + 0.5f, y + 0.5f, r, radius));
            continue;
        }
        const int xa = std::clamp(cx_a, x0, x1), xb = std::clamp(cx_b, x0, x1);
        for (int x = x0; x < xa; ++x) put(x, y, c, round_cov(x + 0.5f, y + 0.5f, r, radius));
        uint8_t* d = &px_[(static_cast<size_t>(y) * w_ + xa) * 4];
        if (a == 255) {
            for (int x = xa; x < xb; ++x, d += 4) { d[0] = c.r; d[1] = c.g; d[2] = c.b; }
        } else if (a > 0) {
            const int inv = 255 - a;
            for (int x = xa; x < xb; ++x, d += 4) {
                d[0] = static_cast<uint8_t>((d[0] * inv + c.r * a + 127) / 255);
                d[1] = static_cast<uint8_t>((d[1] * inv + c.g * a + 127) / 255);
                d[2] = static_cast<uint8_t>((d[2] * inv + c.b * a + 127) / 255);
            }
        }
        for (int x = xb; x < x1; ++x) put(x, y, c, round_cov(x + 0.5f, y + 0.5f, r, radius));
    }
}

void Canvas::stroke_round(const Rect& r, float radius, float t, Color c) {
    radius = std::clamp(radius, 0.f, std::min(r.w, r.h) / 2);
    Rect in = r.inset(t);
    int x0 = std::max(cx0_, static_cast<int>(std::floor(r.x)) - 1), x1 = std::min(cx1_, static_cast<int>(std::ceil(r.right())) + 1);
    int y0 = std::max(cy0_, static_cast<int>(std::floor(r.y)) - 1), y1 = std::min(cy1_, static_cast<int>(std::ceil(r.bottom())) + 1);
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
        float outer = round_cov(x + 0.5f, y + 0.5f, r, radius);
        float inner = in.w > 0 && in.h > 0 ? round_cov(x + 0.5f, y + 0.5f, in, std::max(0.f, radius - t)) : 0.f;
        put(x, y, c, std::max(0.f, outer - inner));
    }
}

void Canvas::fill_gradient_v(const Rect& r, Color top, Color bottom) {
    int y0 = std::max(cy0_, static_cast<int>(r.y)), y1 = std::min(cy1_, static_cast<int>(std::ceil(r.bottom())));
    for (int y = y0; y < y1; ++y) {
        float t = r.h > 0 ? (y + 0.5f - r.y) / r.h : 0;
        Color c = top.mix(bottom, std::clamp(t, 0.f, 1.f));
        int x0 = std::max(cx0_, static_cast<int>(r.x)), x1 = std::min(cx1_, static_cast<int>(std::ceil(r.right())));
        for (int x = x0; x < x1; ++x) put(x, y, c, 1.f);
    }
}

namespace {
// A soft shadow is the same picture every frame, so it is rendered once into an alpha mask and reused.
struct ShadowMask { int w = 0, h = 0, ox = 0, oy = 0; std::vector<uint8_t> a; };
using ShadowKey = std::tuple<int, int, int, int>;     // width, height, radius*10, spread*10

const ShadowMask& shadow_mask(int rw, int rh, float radius, float spread) {
    static std::map<ShadowKey, ShadowMask> cache;
    ShadowKey key{rw, rh, static_cast<int>(radius * 10), static_cast<int>(spread * 10)};
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 48) cache.clear();
    const int layers = 8;
    const int pad = static_cast<int>(std::ceil(spread)) + 3;
    const int yshift = static_cast<int>(std::ceil(spread * 0.35f));
    ShadowMask m;
    m.ox = -pad; m.oy = -pad;
    m.w = rw + 2 * pad;
    m.h = rh + 2 * pad + yshift;
    std::vector<float> acc(static_cast<size_t>(m.w) * m.h, 0.f);      // accumulated opacity, 0..1
    for (int i = layers; i >= 1; --i) {
        float g = spread * i / layers;
        Rect rr{-g, -g + spread * 0.35f, rw + 2 * g, rh + 2 * g};
        float f = 1.0f - float(i - 1) / layers;
        float alpha = f * f / layers * 1.6f;
        float rad = radius + g;
        rad = std::clamp(rad, 0.f, std::min(rr.w, rr.h) / 2);
        for (int y = 0; y < m.h; ++y) for (int x = 0; x < m.w; ++x) {
            float cov = round_cov(x + m.ox + 0.5f, y + m.oy + 0.5f, rr, rad) * alpha;
            if (cov <= 0) continue;
            float& A = acc[static_cast<size_t>(y) * m.w + x];
            A = A + (1.f - A) * cov;
        }
    }
    m.a.resize(acc.size());
    for (size_t i = 0; i < acc.size(); ++i) m.a[i] = static_cast<uint8_t>(std::clamp(acc[i] * 255.f + 0.5f, 0.f, 255.f));
    return cache.emplace(key, std::move(m)).first->second;
}
} // namespace

void Canvas::soft_shadow(const Rect& r, float radius, float spread, Color c) {
    const ShadowMask& m = shadow_mask(static_cast<int>(std::lround(r.w)), static_cast<int>(std::lround(r.h)), radius, spread);
    const int bx = static_cast<int>(std::lround(r.x)) + m.ox, by = static_cast<int>(std::lround(r.y)) + m.oy;
    const int x0 = std::max(cx0_, bx), x1 = std::min(cx1_, bx + m.w);
    const int y0 = std::max(cy0_, by), y1 = std::min(cy1_, by + m.h);
    for (int y = y0; y < y1; ++y) {
        const uint8_t* src = &m.a[static_cast<size_t>(y - by) * m.w + (x0 - bx)];
        uint8_t* d = &px_[(static_cast<size_t>(y) * w_ + x0) * 4];
        for (int x = x0; x < x1; ++x, ++src, d += 4) {
            int a = (*src * c.a + 127) / 255;
            if (!a) continue;
            int inv = 255 - a;
            d[0] = static_cast<uint8_t>((d[0] * inv + c.r * a + 127) / 255);
            d[1] = static_cast<uint8_t>((d[1] * inv + c.g * a + 127) / 255);
            d[2] = static_cast<uint8_t>((d[2] * inv + c.b * a + 127) / 255);
        }
    }
}

void Canvas::circle(float cx, float cy, float rad, Color c) {
    int x0 = std::max(cx0_, static_cast<int>(cx - rad) - 1), x1 = std::min(cx1_, static_cast<int>(cx + rad) + 2);
    int y0 = std::max(cy0_, static_cast<int>(cy - rad) - 1), y1 = std::min(cy1_, static_cast<int>(cy + rad) + 2);
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) {
        float d = std::hypot(x + 0.5f - cx, y + 0.5f - cy) - rad;
        put(x, y, c, std::clamp(0.5f - d, 0.f, 1.f));
    }
}

void Canvas::blit(const Image& src, const Rect& dst, float opacity, float brightness, bool mirror) {
    if (src.empty() || dst.w <= 0 || dst.h <= 0 || opacity <= 0) return;
    int x0 = std::max(cx0_, static_cast<int>(std::floor(dst.x))), x1 = std::min(cx1_, static_cast<int>(std::ceil(dst.right())));
    int y0 = std::max(cy0_, static_cast<int>(std::floor(dst.y))), y1 = std::min(cy1_, static_cast<int>(std::ceil(dst.bottom())));
    // Common case (pre-scaled art at its final size, whole-pixel position): no resampling needed.
    if (!mirror && dst.w == static_cast<float>(src.w) && dst.h == static_cast<float>(src.h) && dst.x == std::floor(dst.x) && dst.y == std::floor(dst.y)) {
        const int ox = static_cast<int>(dst.x), oy = static_cast<int>(dst.y);
        const int k = static_cast<int>(opacity * brightness * 256.f + 0.5f), ko = static_cast<int>(opacity * 256.f + 0.5f);
        for (int y = y0; y < y1; ++y) {
            const uint8_t* s = src.at(x0 - ox, y - oy);
            uint8_t* d = &px_[(static_cast<size_t>(y) * w_ + x0) * 4];
            for (int x = x0; x < x1; ++x, s += 4, d += 4) {
                int sa = s[3];
                if (!sa) continue;
                if (sa == 255 && ko == 256 && k == 256) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; continue; }   // opaque pixel: plain copy
                int a = (sa * ko) >> 8;                                   // 0..255
                int inv = 255 - a;
                d[0] = static_cast<uint8_t>(std::min(255, (d[0] * inv + 127) / 255 + ((s[0] * k) >> 8)));
                d[1] = static_cast<uint8_t>(std::min(255, (d[1] * inv + 127) / 255 + ((s[1] * k) >> 8)));
                d[2] = static_cast<uint8_t>(std::min(255, (d[2] * inv + 127) / 255 + ((s[2] * k) >> 8)));
            }
        }
        return;
    }
    float sx = src.w / dst.w, sy = src.h / dst.h;
    for (int y = y0; y < y1; ++y) {
        float fy = (y + 0.5f - dst.y) * sy - 0.5f;
        int iy = static_cast<int>(std::floor(fy));
        float ty = fy - iy;
        int ya = std::clamp(iy, 0, src.h - 1), yb = std::clamp(iy + 1, 0, src.h - 1);
        uint8_t* d = &px_[(static_cast<size_t>(y) * w_ + x0) * 4];
        for (int x = x0; x < x1; ++x, d += 4) {
            float fx = (x + 0.5f - dst.x) * sx - 0.5f;
            if (mirror) fx = src.w - 1 - fx;
            int ix = static_cast<int>(std::floor(fx));
            float tx = fx - ix;
            int xa = std::clamp(ix, 0, src.w - 1), xb = std::clamp(ix + 1, 0, src.w - 1);
            const uint8_t* p00 = src.at(xa, ya); const uint8_t* p10 = src.at(xb, ya);
            const uint8_t* p01 = src.at(xa, yb); const uint8_t* p11 = src.at(xb, yb);
            float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
            float a = (p00[3] * w00 + p10[3] * w10 + p01[3] * w01 + p11[3] * w11) * opacity * (1.f / 255.f);
            if (a <= 0.002f) continue;
            float k = opacity * brightness;
            float inv = 1.0f - a;
            for (int c = 0; c < 3; ++c) {
                float s = (p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11) * k;
                d[c] = static_cast<uint8_t>(std::clamp(d[c] * inv + s + 0.5f, 0.f, 255.f));
            }
        }
    }
}

void Canvas::blit_region(const Image& src, const Rect& sr, const Rect& dst, float opacity) {
    Image part;
    part.w = std::max(1, static_cast<int>(sr.w)); part.h = std::max(1, static_cast<int>(sr.h)); part.premultiplied = true;
    part.px.assign(static_cast<size_t>(part.w) * part.h * 4, 0);
    for (int y = 0; y < part.h; ++y) {
        int yy = static_cast<int>(sr.y) + y;
        if (yy < 0 || yy >= src.h) continue;
        for (int x = 0; x < part.w; ++x) {
            int xx = static_cast<int>(sr.x) + x;
            if (xx < 0 || xx >= src.w) continue;
            std::memcpy(part.at(x, y), src.at(xx, yy), 4);
        }
    }
    blit(part, dst, opacity);
}

float Canvas::text(const FontSet& f, const std::string& s, float px, float x, float baseline, Color c) {
    float pen = x;
    f.layout_line(s, px, x, baseline, [&](const Glyph& g, int gx, int gy) {
        for (int yy = 0; yy < g.h; ++yy) {
            const uint8_t* row = &g.a[static_cast<size_t>(yy) * g.w];
            for (int xx = 0; xx < g.w; ++xx) if (row[xx]) put(gx + xx, gy + yy, c, row[xx] / 255.0f);
        }
    });
    pen += f.width(s, px);
    return pen;
}

int Canvas::paragraph(const FontSet& f, const std::string& s, float px, const Rect& box, Color c, float lh_scale, int max_lines) {
    auto lines = f.wrap(s, px, box.w);
    float lh = f.line_height(px) * lh_scale;
    float base = box.y + f.ascent(px);
    int n = 0;
    for (auto& l : lines) {
        if (max_lines >= 0 && n >= max_lines) break;
        if (base - f.ascent(px) > box.bottom()) break;
        text(f, l, px, box.x, base, c);
        base += lh;
        ++n;
    }
    return n;
}

// ----------------------------------------------------------------------------------- transitions

void Canvas::mix(const Canvas& a, const Canvas& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    int k = static_cast<int>(t * 256);
    size_t n = std::min({px_.size(), a.px_.size(), b.px_.size()});
    for (size_t i = 0; i < n; i += 4) {
        px_[i] = static_cast<uint8_t>((a.px_[i] * (256 - k) + b.px_[i] * k) >> 8);
        px_[i + 1] = static_cast<uint8_t>((a.px_[i + 1] * (256 - k) + b.px_[i + 1] * k) >> 8);
        px_[i + 2] = static_cast<uint8_t>((a.px_[i + 2] * (256 - k) + b.px_[i + 2] * k) >> 8);
        px_[i + 3] = 255;
    }
}

static float ease(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); }

void Canvas::slide(const Canvas& a, const Canvas& b, float t, int dir) {
    int off = static_cast<int>(ease(t) * w_);
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            const Canvas* src; int sx;
            if (dir < 0) { if (x < w_ - off) { src = &a; sx = x + off; } else { src = &b; sx = x - (w_ - off); } }
            else { if (x >= off) { src = &a; sx = x - off; } else { src = &b; sx = x + (w_ - off); } }
            std::memcpy(&px_[(static_cast<size_t>(y) * w_ + x) * 4], &src->px_[(static_cast<size_t>(y) * w_ + sx) * 4], 4);
        }
    }
}

void Canvas::wipe(const Canvas& a, const Canvas& b, float t, int dir, float feather) {
    float edge = ease(t) * (w_ + feather) - feather;
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            float pos = dir < 0 ? float(w_ - 1 - x) : float(x);
            float k = std::clamp((edge - pos) / std::max(feather, 1.f) + 1.0f, 0.f, 1.f);
            size_t i = (static_cast<size_t>(y) * w_ + x) * 4;
            for (int c = 0; c < 3; ++c) px_[i + c] = static_cast<uint8_t>(a.px_[i + c] + (b.px_[i + c] - a.px_[i + c]) * k + 0.5f);
            px_[i + 3] = 255;
        }
    }
}

void Canvas::flash(const Canvas& a, const Canvas& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    float white = t < 0.25f ? t / 0.25f : 1.0f - (t - 0.25f) / 0.75f;       // quick burst, slow fade
    const Canvas& base = t < 0.25f ? a : b;
    for (size_t i = 0; i + 3 < px_.size(); i += 4) {
        for (int c = 0; c < 3; ++c) px_[i + c] = static_cast<uint8_t>(base.px_[i + c] + (255 - base.px_[i + c]) * white + 0.5f);
        px_[i + 3] = 255;
    }
}

} // namespace sf
