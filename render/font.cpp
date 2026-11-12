#include "font.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace sf {

std::vector<uint32_t> utf8_decode(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp; int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
        else { out.push_back(0xFFFD); ++i; continue; }
        if (i + n > s.size()) { out.push_back(0xFFFD); break; }
        bool ok = true;
        for (int k = 1; k < n; ++k) { unsigned char cc = s[i + k]; if ((cc >> 6) != 2) { ok = false; break; } cp = (cp << 6) | (cc & 0x3F); }
        if (!ok) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += n;
    }
    return out;
}

std::string utf8_encode(uint32_t cp) {
    std::string s;
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    return s;
}

size_t utf8_prev(const std::string& s, size_t pos) {
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) >> 6) == 2) --pos;
    return pos;
}

// ------------------------------------------------------------------------------------ parsing

uint32_t Font::u16(uint32_t o) const { if (o + 2 > data_.size()) throw Error("font file is damaged"); return (uint32_t(data_[o]) << 8) | data_[o + 1]; }
int16_t Font::s16(uint32_t o) const { return static_cast<int16_t>(u16(o)); }
uint32_t Font::u32(uint32_t o) const { return (u16(o) << 16) | u16(o + 2); }
uint32_t Font::table(const char* t) const { auto it = tables_.find(t); return it == tables_.end() ? 0 : it->second.first; }

std::shared_ptr<Font> Font::load(Bytes data) {
    std::shared_ptr<Font> f(new Font);
    f->data_ = std::move(data);
    if (f->data_.size() < 12) throw Error("not a font file");
    uint32_t tag = f->u32(0);
    uint32_t base = 0;
    if (tag == 0x74746366) base = f->u32(12);                       // 'ttcf': take the first font in the collection
    else if (tag != 0x00010000 && tag != 0x74727565) throw Error("unsupported font format (only TrueType outlines are supported; convert OpenType/CFF fonts to .ttf)");
    int n = static_cast<int>(f->u16(base + 4));
    for (int i = 0; i < n; ++i) {
        uint32_t e = base + 12 + 16 * i;
        std::string name(reinterpret_cast<const char*>(&f->data_[e]), 4);
        f->tables_[name] = {f->u32(e + 8), f->u32(e + 12)};
    }
    for (auto t : {"head", "maxp", "hhea", "hmtx", "cmap", "loca", "glyf"})
        if (!f->tables_.count(t)) throw Error(std::string("font has no '") + t + "' table (only TrueType outlines are supported)");
    uint32_t head = f->table("head");
    f->upem_ = static_cast<int>(f->u16(head + 18));
    f->loca_format_ = f->s16(head + 50);
    f->num_glyphs_ = static_cast<int>(f->u16(f->table("maxp") + 4));
    uint32_t hhea = f->table("hhea");
    f->asc_ = f->s16(hhea + 4); f->desc_ = f->s16(hhea + 6); f->gap_ = f->s16(hhea + 8);
    f->num_hmetrics_ = static_cast<int>(f->u16(hhea + 34));
    if (f->upem_ <= 0 || f->num_glyphs_ <= 0) throw Error("font header is damaged");

    // cmap: prefer a full Unicode table (format 12), else format 4 for the BMP
    uint32_t cm = f->table("cmap");
    int nt = static_cast<int>(f->u16(cm + 2));
    uint32_t best12 = 0, best4 = 0;
    for (int i = 0; i < nt; ++i) {
        uint32_t plat = f->u16(cm + 4 + 8 * i), enc = f->u16(cm + 6 + 8 * i), off = cm + f->u32(cm + 8 + 8 * i);
        bool unicode = plat == 0 || (plat == 3 && (enc == 1 || enc == 10));
        if (!unicode) continue;
        uint32_t fmt = f->u16(off);
        if (fmt == 12) best12 = off; else if (fmt == 4 && !best4) best4 = off;
    }
    f->cmap_off_ = best12 ? best12 : best4;
    f->cmap_fmt_ = best12 ? 12 : 4;
    if (!f->cmap_off_) throw Error("font has no Unicode character map");

    // family name (name id 1), best effort
    if (f->tables_.count("name")) {
        uint32_t nm = f->table("name");
        int cnt = static_cast<int>(f->u16(nm + 2));
        uint32_t so = nm + f->u16(nm + 4);
        for (int i = 0; i < cnt; ++i) {
            uint32_t r = nm + 6 + 12 * i;
            if (f->u16(r + 6) != 1) continue;
            uint32_t plat = f->u16(r), len = f->u16(r + 8), off = so + f->u16(r + 10);
            if (off + len > f->data_.size()) continue;
            std::string s;
            if (plat == 3 || plat == 0) { for (uint32_t k = 0; k + 1 < len; k += 2) s += static_cast<char>(f->data_[off + k + 1]); }
            else s.assign(reinterpret_cast<const char*>(&f->data_[off]), len);
            f->family_ = s;
            if (plat == 3) break;
        }
    }
    return f;
}

uint32_t Font::glyph_index(uint32_t cp) const {
    uint32_t o = cmap_off_;
    if (cmap_fmt_ == 12) {
        uint32_t groups = u32(o + 12);
        uint32_t lo = 0, hi = groups;
        while (lo < hi) {
            uint32_t mid = (lo + hi) / 2, g = o + 16 + 12 * mid;
            uint32_t s = u32(g), e = u32(g + 4);
            if (cp < s) hi = mid; else if (cp > e) lo = mid + 1; else return u32(g + 8) + (cp - s);
        }
        return 0;
    }
    if (cp > 0xFFFF) return 0;
    uint32_t segx2 = u16(o + 6), ends = o + 14, starts = ends + segx2 + 2, deltas = starts + segx2, ranges = deltas + segx2;
    for (uint32_t i = 0; i < segx2; i += 2) {
        if (cp > u16(ends + i)) continue;
        uint32_t start = u16(starts + i);
        if (cp < start) return 0;
        uint32_t ro = u16(ranges + i);
        if (ro == 0) return (cp + u16(deltas + i)) & 0xFFFF;
        uint32_t g = u16(ranges + i + ro + 2 * (cp - start));
        return g ? ((g + u16(deltas + i)) & 0xFFFF) : 0;
    }
    return 0;
}

float Font::advance(uint32_t cp, float px) const {
    uint32_t gid = glyph_index(cp);
    uint32_t hm = table("hmtx");
    uint32_t idx = std::min<uint32_t>(gid, static_cast<uint32_t>(num_hmetrics_) - 1);
    return u16(hm + 4 * idx) * px / upem_;
}
float Font::ascent(float px) const { return asc_ * px / upem_; }
float Font::descent(float px) const { return -desc_ * px / upem_; }
float Font::line_gap(float px) const { return gap_ * px / upem_; }

// ------------------------------------------------------------------------------------ outlines

bool Font::outline(uint32_t gid, std::vector<std::vector<Pt>>& contours, int depth) const {
    if (depth > 6 || gid >= static_cast<uint32_t>(num_glyphs_)) return false;
    uint32_t loca = table("loca"), glyf = table("glyf");
    uint32_t o0, o1;
    if (loca_format_ == 0) { o0 = u16(loca + 2 * gid) * 2; o1 = u16(loca + 2 * gid + 2) * 2; }
    else { o0 = u32(loca + 4 * gid); o1 = u32(loca + 4 * gid + 4); }
    if (o1 <= o0) return true;                                     // empty glyph (space)
    uint32_t g = glyf + o0;
    int nc = s16(g);
    if (nc >= 0) {
        std::vector<uint32_t> ends(static_cast<size_t>(nc));
        for (int i = 0; i < nc; ++i) ends[i] = u16(g + 10 + 2 * i);
        uint32_t npts = nc ? ends.back() + 1 : 0;
        uint32_t p = g + 10 + 2 * nc;
        uint32_t ilen = u16(p);
        p += 2 + ilen;
        std::vector<uint8_t> flags(npts);
        for (uint32_t i = 0; i < npts;) {
            if (p >= data_.size()) return false;
            uint8_t fl = data_[p++];
            flags[i++] = fl;
            if (fl & 8) { uint8_t rep = data_[p++]; while (rep-- && i < npts) flags[i++] = fl; }
        }
        std::vector<int> xs(npts), ys(npts);
        int v = 0;
        for (uint32_t i = 0; i < npts; ++i) {
            uint8_t fl = flags[i];
            if (fl & 2) { int d = data_[p++]; v += (fl & 16) ? d : -d; }
            else if (!(fl & 16)) { v += s16(p); p += 2; }
            xs[i] = v;
        }
        v = 0;
        for (uint32_t i = 0; i < npts; ++i) {
            uint8_t fl = flags[i];
            if (fl & 4) { int d = data_[p++]; v += (fl & 32) ? d : -d; }
            else if (!(fl & 32)) { v += s16(p); p += 2; }
            ys[i] = v;
        }
        uint32_t start = 0;
        for (int c = 0; c < nc; ++c) {
            std::vector<Pt> pts;
            for (uint32_t i = start; i <= ends[c]; ++i) pts.push_back({float(xs[i]), float(ys[i]), (flags[i] & 1) != 0});
            start = ends[c] + 1;
            if (pts.size() >= 2) contours.push_back(std::move(pts));
        }
        return true;
    }
    // composite
    uint32_t p = g + 10;
    while (true) {
        uint32_t fl = u16(p), sub = u16(p + 2);
        p += 4;
        float dx = 0, dy = 0;
        if (fl & 1) { if (fl & 2) { dx = s16(p); dy = s16(p + 2); } p += 4; }
        else { if (fl & 2) { dx = static_cast<int8_t>(data_[p]); dy = static_cast<int8_t>(data_[p + 1]); } p += 2; }
        float a = 1, b = 0, c = 0, d = 1;
        auto f2 = [&](uint32_t o) { return s16(o) / 16384.0f; };
        if (fl & 8) { a = d = f2(p); p += 2; }
        else if (fl & 0x40) { a = f2(p); d = f2(p + 2); p += 4; }
        else if (fl & 0x80) { a = f2(p); b = f2(p + 2); c = f2(p + 4); d = f2(p + 6); p += 8; }
        std::vector<std::vector<Pt>> part;
        if (!outline(sub, part, depth + 1)) return false;
        for (auto& ct : part) {
            for (auto& pt : ct) { float x = pt.x * a + pt.y * c + dx, y = pt.x * b + pt.y * d + dy; pt.x = x; pt.y = y; }
            contours.push_back(std::move(ct));
        }
        if (!(fl & 0x20)) break;
    }
    return true;
}

// Exact-area coverage accumulation: every edge adds signed area/cover to a float grid; a running sum along each row gives coverage.
namespace {
struct Raster {
    int w, h;
    std::vector<float> acc;
    Raster(int w_, int h_) : w(w_), h(h_), acc(static_cast<size_t>(w_) * h_ + 4, 0.f) {}
    void line(float x0, float y0, float x1, float y1) {
        if (y0 == y1) return;
        float dir = 1;
        if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); dir = -1; }
        float dxdy = (x1 - x0) / (y1 - y0);
        float x = x0;
        if (y0 < 0) x -= y0 * dxdy;
        int ys = std::max(0, static_cast<int>(y0)), ye = std::min(h, static_cast<int>(std::ceil(y1)));
        for (int y = ys; y < ye; ++y) {
            float dy = std::min<float>(y + 1, y1) - std::max<float>(y, y0);
            float xn = x + dxdy * dy;
            float d = dy * dir;
            float xa = std::min(x, xn), xb = std::max(x, xn);
            float x0f = std::floor(xa); int x0i = static_cast<int>(x0f);
            float x1c = std::ceil(xb); int x1i = static_cast<int>(x1c);
            float* row = &acc[static_cast<size_t>(y) * w];
            auto add = [&](int xi, float v) { if (xi < 0) xi = 0; if (xi >= w) { return; } row[xi] += v; };
            if (x1i <= x0i + 1) {
                float xmf = 0.5f * (x + xn) - x0f;
                add(x0i, d - d * xmf);
                add(x0i + 1, d * xmf);
            } else {
                float s = 1.0f / (xb - xa);
                float x0f2 = xa - x0f;
                float a0 = 0.5f * s * (1.0f - x0f2) * (1.0f - x0f2);
                float x1f = xb - x1c + 1.0f;
                float am = 0.5f * s * x1f * x1f;
                add(x0i, d * a0);
                if (x1i == x0i + 2) {
                    add(x0i + 1, d * (1.0f - a0 - am));
                } else {
                    float a1 = s * (1.5f - x0f2);
                    add(x0i + 1, d * (a1 - a0));
                    for (int xi = x0i + 2; xi < x1i - 1; ++xi) add(xi, d * s);
                    float a2 = a1 + (x1i - x0i - 3) * s;
                    add(x1i - 1, d * (1.0f - a2 - am));
                }
                add(x1i, d * am);
            }
            x = xn;
        }
    }
};
void flatten_quad(Raster& r, float x0, float y0, float cx, float cy, float x1, float y1) {
    float dd = std::fabs(x0 - 2 * cx + x1) + std::fabs(y0 - 2 * cy + y1);
    int n = std::clamp(static_cast<int>(std::ceil(std::sqrt(dd * 2.0f))) , 1, 24);
    float px = x0, py = y0;
    for (int i = 1; i <= n; ++i) {
        float t = float(i) / n, mt = 1 - t;
        float x = mt * mt * x0 + 2 * mt * t * cx + t * t * x1, y = mt * mt * y0 + 2 * mt * t * cy + t * t * y1;
        r.line(px, py, x, y);
        px = x; py = y;
    }
}
}

const Glyph& Font::glyph(uint32_t cp, float px, int quarter) {
    uint64_t key = (uint64_t(cp) << 32) ^ (uint64_t(static_cast<uint32_t>(px * 16)) << 4) ^ uint64_t(quarter & 3);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    Glyph gl;
    gl.advance = advance(cp, px);
    uint32_t gid = glyph_index(cp);
    std::vector<std::vector<Pt>> contours;
    if (gid || cp == 0) {
        try { outline(gid, contours); } catch (const Error&) { contours.clear(); }
    }
    if (!contours.empty()) {
        float s = px / upem_;
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (auto& c : contours) for (auto& p : c) { minx = std::min(minx, p.x * s); maxx = std::max(maxx, p.x * s); miny = std::min(miny, -p.y * s); maxy = std::max(maxy, -p.y * s); }
        float sub = quarter / 4.0f;
        int ox = static_cast<int>(std::floor(minx + sub)) - 1, oy = static_cast<int>(std::floor(miny)) - 1;
        int w = static_cast<int>(std::ceil(maxx + sub)) - ox + 2, h = static_cast<int>(std::ceil(maxy)) - oy + 2;
        if (w > 0 && h > 0 && w < 4096 && h < 4096) {
            Raster r(w, h);
            auto X = [&](float v) { return v * s + sub - ox; };
            auto Y = [&](float v) { return -v * s - oy; };
            for (auto& c : contours) {
                // start on an on-curve point (or synthesise one between two off-curve points)
                size_t n = c.size(), st = 0;
                bool found = false;
                for (size_t i = 0; i < n; ++i) if (c[i].on) { st = i; found = true; break; }
                Pt start = found ? c[st] : Pt{(c[0].x + c[n - 1].x) / 2, (c[0].y + c[n - 1].y) / 2, true};
                size_t first = found ? st + 1 : 0, count = found ? n - 1 : n;
                Pt cur = start;
                bool have_ctrl = false; Pt ctrl{};
                auto emit_to = [&](Pt to) {
                    if (have_ctrl) { flatten_quad(r, X(cur.x), Y(cur.y), X(ctrl.x), Y(ctrl.y), X(to.x), Y(to.y)); have_ctrl = false; }
                    else r.line(X(cur.x), Y(cur.y), X(to.x), Y(to.y));
                    cur = to;
                };
                for (size_t k = 0; k < count; ++k) {
                    Pt p = c[(first + k) % n];
                    if (p.on) emit_to(p);
                    else {
                        if (have_ctrl) emit_to({(ctrl.x + p.x) / 2, (ctrl.y + p.y) / 2, true});
                        ctrl = p; have_ctrl = true;
                    }
                }
                emit_to(start);
            }
            gl.w = w; gl.h = h; gl.left = ox; gl.top = -oy;
            gl.a.resize(static_cast<size_t>(w) * h);
            for (int y = 0; y < h; ++y) {
                float sum = 0;
                for (int x = 0; x < w; ++x) {
                    sum += r.acc[static_cast<size_t>(y) * w + x];
                    gl.a[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(std::min(255.0f, std::fabs(sum) * 255.0f + 0.5f));
                }
            }
        }
    }
    return cache_.emplace(key, std::move(gl)).first->second;
}

// ------------------------------------------------------------------------------------ FontSet

Font* FontSet::pick(uint32_t cp) const {
    for (auto& f : fonts_) if (f->has(cp)) return f.get();
    if (cp == ' ' || cp == '\t' || cp == 0xA0) return fonts_.empty() ? nullptr : fonts_[0].get();
    return fonts_.empty() ? nullptr : fonts_[0].get();     // shows the font's own "missing" glyph
}

float FontSet::width(const std::string& text, float px) const {
    float w = 0;
    for (uint32_t cp : utf8_decode(text)) if (Font* f = pick(cp)) w += f->advance(cp, px);
    return w;
}
float FontSet::ascent(float px) const { return fonts_.empty() ? px : fonts_[0]->ascent(px); }
float FontSet::line_height(float px) const {
    if (fonts_.empty()) return px * 1.3f;
    auto& f = *fonts_[0];
    return f.ascent(px) + f.descent(px) + f.line_gap(px);
}

std::vector<std::string> FontSet::wrap(const std::string& text, float px, float max_width) const {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string para = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        std::string line;
        size_t i = 0;
        while (i < para.size()) {
            size_t j = para.find(' ', i);
            std::string word = para.substr(i, j == std::string::npos ? std::string::npos : j - i);
            i = j == std::string::npos ? para.size() : j + 1;
            std::string trial = line.empty() ? word : line + " " + word;
            if (width(trial, px) <= max_width || line.empty()) {
                line = trial;
                // a word wider than the box: split it by characters
                while (width(line, px) > max_width && utf8_decode(line).size() > 1) {
                    size_t cut = line.size();
                    while (cut > 0 && width(line.substr(0, cut), px) > max_width) cut = utf8_prev(line, cut);
                    if (cut == 0) break;
                    lines.push_back(line.substr(0, cut));
                    line = line.substr(cut);
                }
            } else {
                lines.push_back(line);
                line = word;
            }
        }
        lines.push_back(line);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return lines;
}

} // namespace sf
