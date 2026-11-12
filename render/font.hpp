// TrueType text: parses .ttf files, rasterises glyphs with exact-area anti-aliasing, lays text out.
// No FreeType. Handles simple + composite outlines, cmap formats 4 and 12, UTF-8 input, quarter-pixel
// horizontal positioning, and a fallback chain for characters one font lacks.
#pragma once
#include "../core/common.hpp"
#include <map>
#include <memory>
#include <unordered_map>

namespace sf {

struct Glyph {
    int w = 0, h = 0;          // bitmap size
    int left = 0, top = 0;     // offset from the pen position (top is distance above the baseline)
    float advance = 0;         // in pixels at the requested size
    std::vector<uint8_t> a;    // coverage, w*h
};

class Font {
public:
    static std::shared_ptr<Font> load(Bytes data);              // throws Error
    uint32_t glyph_index(uint32_t codepoint) const;             // 0 = missing
    bool has(uint32_t cp) const { return glyph_index(cp) != 0; }
    float advance(uint32_t cp, float px) const;
    float ascent(float px) const;
    float descent(float px) const;                              // positive number
    float line_gap(float px) const;
    // quarter = 0..3 selects the horizontal sub-pixel offset. The returned reference stays valid.
    const Glyph& glyph(uint32_t cp, float px, int quarter = 0);
    const std::string& family() const { return family_; }

private:
    Font() = default;
    struct Pt { float x, y; bool on; };
    bool outline(uint32_t gid, std::vector<std::vector<Pt>>& contours, int depth = 0) const;
    Bytes data_;
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> tables_;
    int upem_ = 1000, loca_format_ = 0, num_glyphs_ = 0, num_hmetrics_ = 0;
    int asc_ = 0, desc_ = 0, gap_ = 0;
    uint32_t cmap_off_ = 0, cmap_fmt_ = 0;
    std::string family_;
    std::unordered_map<uint64_t, Glyph> cache_;
    uint32_t u16(uint32_t o) const; int16_t s16(uint32_t o) const; uint32_t u32(uint32_t o) const;
    uint32_t table(const char* t) const;
};

// A primary font followed by fallbacks. Text is UTF-8.
class FontSet {
public:
    void add(std::shared_ptr<Font> f) { fonts_.push_back(std::move(f)); }
    bool empty() const { return fonts_.empty(); }
    Font* pick(uint32_t cp) const;
    float width(const std::string& text, float px) const;
    float line_height(float px) const;                          // distance between baselines
    float ascent(float px) const;
    // Greedy word wrap. '\n' forces a break. A single word longer than the line is split.
    std::vector<std::string> wrap(const std::string& text, float px, float max_width) const;
    // Draws one line; the callback receives one glyph at a time (coverage bitmap + integer origin).
    template <class F> void layout_line(const std::string& text, float px, float x, float baseline, F&& put) const;

private:
    std::vector<std::shared_ptr<Font>> fonts_;
};

// UTF-8 helpers
std::vector<uint32_t> utf8_decode(const std::string& s);
std::string utf8_encode(uint32_t cp);
size_t utf8_prev(const std::string& s, size_t pos);             // byte index of the previous code point

template <class F> void FontSet::layout_line(const std::string& text, float px, float x, float baseline, F&& put) const {
    for (uint32_t cp : utf8_decode(text)) {
        Font* f = pick(cp);
        if (!f) continue;
        int ix = static_cast<int>(x);
        int q = static_cast<int>((x - ix) * 4);
        const Glyph& g = f->glyph(cp, px, q);
        if (g.w > 0) put(g, ix + g.left, static_cast<int>(baseline) - g.top);
        x += g.advance;
    }
}

} // namespace sf
