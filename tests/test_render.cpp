#include "test.hpp"
#include "../render/canvas.hpp"
#include "../core/process.hpp"
#include "../core/platform.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace sf;
namespace fs = std::filesystem;

#ifndef SF_SOURCE_DIR
#define SF_SOURCE_DIR "."
#endif

static Bytes slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw Error("cannot read " + p);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

static std::string fixtures() {
    static std::string dir;
    if (!dir.empty()) return dir;
    dir = (fs::temp_directory_path() / "sf_png_fixtures").string();
    fs::remove_all(dir);
    fs::create_directories(dir);
    SpawnOptions o;
    const char* python = std::getenv("SF_TEST_PYTHON");
    o.argv = {python ? python : "python3", "-I", std::string(SF_SOURCE_DIR) + "/tools/make_png_fixtures.py", dir};
    auto p = Process::spawn(o);
    if (p->wait(20000) != 0) throw Error("could not create PNG fixtures (needs python3 with Pillow)");
    return dir;
}

TEST(png_decodes_every_colour_type_like_pillow) {
    for (auto name : {"RGB", "RGBA", "L", "LA", "P", "L4"}) {
        Image img = png_decode(slurp(fixtures() + "/" + name + ".png"));
        Bytes ref = slurp(fixtures() + "/" + name + ".raw");
        CHECK_EQ(img.px.size(), ref.size());
        size_t bad = 0;
        for (size_t i = 0; i < std::min(img.px.size(), ref.size()); ++i) {
            if (img.px[i] != ref[i]) ++bad;
        }
        if (bad) std::cerr << "  mismatching bytes in " << name << ": " << bad << "\n";
        CHECK_EQ(bad, size_t(0));
    }
}

TEST(png_16_bit_keeps_the_high_byte) {
    Image g = png_decode(slurp(fixtures() + "/G16.png"));
    CHECK_EQ(g.w, 5);
    CHECK_EQ(int(g.at(2, 2)[0]), 40000 >> 8);
    CHECK_EQ(int(g.at(2, 2)[3]), 255);
}

TEST(png_rejects_bad_input_with_readable_errors) {
    CHECK_THROWS(png_decode(to_bytes("not a png")));
    Bytes b = slurp(fixtures() + "/RGB.png");
    Bytes cut(b.begin(), b.begin() + static_cast<long>(b.size() / 2));
    CHECK_THROWS(png_decode(cut));
    Bytes bad = b; bad[b.size() / 2] ^= 0xFF;
    CHECK_THROWS(png_decode(bad));
    bool msg = false;
    try { png_decode(slurp(fixtures() + "/interlaced.png")); } catch (const Error& e) { msg = std::string(e.what()).find("interlaced") != std::string::npos; }
    CHECK(msg);
}

TEST(png_encode_round_trip) {
    Image a = png_decode(slurp(fixtures() + "/RGBA.png"));
    Image b = png_decode(png_encode(a));
    CHECK_EQ(a.w, b.w); CHECK_EQ(a.h, b.h);
    CHECK(a.px == b.px);
    // premultiplied images are stored as straight alpha again
    Image p = a; p.premultiply();
    Image c = png_decode(png_encode(p));
    size_t off = 0;
    for (size_t i = 0; i + 3 < a.px.size(); i += 4) if (a.px[i + 3] > 128) for (int k = 0; k < 3; ++k) off = std::max<size_t>(off, std::abs(int(a.px[i + k]) - int(c.px[i + k])));
    CHECK(off <= 2);
}

TEST(image_resize_preserves_average_colour) {
    Image im; im.w = 8; im.h = 8; im.px.assign(8 * 8 * 4, 0);
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) { uint8_t* p = im.at(x, y); p[0] = (x < 4) ? 200 : 0; p[1] = 100; p[2] = 50; p[3] = 255; }
    Image s = im.resized(2, 2);
    CHECK_EQ(s.w, 2);
    CHECK_EQ(int(s.at(0, 0)[0]), 200); CHECK_EQ(int(s.at(1, 0)[0]), 0);
    Image odd = im.resized(3, 3);                       // non-integer ratio: the middle column mixes both halves
    CHECK(odd.at(1, 1)[0] > 0 && odd.at(1, 1)[0] < 200);
    CHECK_EQ(int(odd.at(1, 1)[1]), 100);
}

static std::shared_ptr<Font> font(const char* name) {
    return Font::load(slurp(std::string(SF_SOURCE_DIR) + "/assets/fonts/" + name));
}

TEST(font_loads_and_measures) {
    auto f = font("Poppins-Regular.ttf");
    CHECK_EQ(f->family(), std::string("Poppins"));
    CHECK(f->has('A')); CHECK(f->has(0xE9)); CHECK(f->has(0x00F1));   // é ñ
    CHECK(!f->has(0x4E2D));                                            // CJK is not in Poppins
    float a = f->advance('i', 32), m = f->advance('m', 32);
    CHECK(m > a * 2);
    CHECK(f->ascent(32) > 20 && f->descent(32) > 0);
    auto dj = font("DejaVuSans.ttf");
    CHECK(dj->has(0x2764) || dj->has(0x2605));                        // symbols come from the fallback font
    CHECK_THROWS(Font::load(to_bytes("this is not a font at all, sorry")));
}

TEST(font_glyphs_have_sane_coverage) {
    auto f = font("Poppins-Regular.ttf");
    const Glyph& o = f->glyph('o', 64);
    CHECK(o.w > 20 && o.h > 20);
    // an 'o' is a ring: the centre is empty, the stroke is solid
    CHECK_EQ(int(o.a[(o.h / 2) * o.w + o.w / 2]), 0);
    bool solid = false;
    for (int x = 0; x < o.w; ++x) if (o.a[(o.h / 2) * o.w + x] > 250) solid = true;
    CHECK(solid);
    // total ink of 'l' scales with area; sub-pixel offsets shift the bitmap but keep the ink
    auto ink = [](const Glyph& g) { double s = 0; for (auto v : g.a) s += v; return s; };
    double i0 = ink(f->glyph('l', 40, 0)), i2 = ink(f->glyph('l', 40, 2));
    CHECK_NEAR(i0 / i2, 1.0, 0.02);
    CHECK(f->glyph(' ', 40).w == 0);
    CHECK(f->glyph(0xE9, 40).h > f->glyph('e', 40).h);                // the accent adds height (composite glyph)
}

TEST(fontset_wraps_text_and_falls_back) {
    FontSet fs;
    fs.add(font("Poppins-Regular.ttf"));
    fs.add(font("DejaVuSans.ttf"));
    auto lines = fs.wrap("The quick brown fox jumps over the lazy dog and keeps running far away", 24, 300);
    CHECK(lines.size() >= 3);
    for (auto& l : lines) CHECK(fs.width(l, 24) <= 300.5f);
    auto forced = fs.wrap("one\ntwo\n\nthree", 20, 500);
    CHECK_EQ(forced.size(), size_t(4));
    CHECK_EQ(forced[2], std::string(""));
    auto longword = fs.wrap("Supercalifragilisticexpialidocious", 24, 120);
    CHECK(longword.size() >= 2);
    for (auto& l : longword) CHECK(fs.width(l, 24) <= 120.5f);
    CHECK(fs.pick(0x2605) == fs.pick(0x2605));
    CHECK(fs.width("\xE2\x98\x85", 24) > 0);                           // ★ via DejaVu
    CHECK_EQ(utf8_decode("a\xC3\xA9\xE2\x82\xAC").size(), size_t(3));
    CHECK_EQ(utf8_encode(0x20AC), std::string("\xE2\x82\xAC"));
    std::string s = "a\xC3\xA9";
    CHECK_EQ(utf8_prev(s, s.size()), size_t(1));
}

TEST(canvas_primitives_and_clipping) {
    Canvas c(40, 30);
    c.clear({10, 20, 30, 255});
    c.fill({5, 5, 10, 10}, {255, 0, 0, 255});
    CHECK_EQ(int(c.pixels()[(7 * 40 + 7) * 4]), 255);
    CHECK_EQ(int(c.pixels()[(1 * 40 + 1) * 4]), 10);
    c.fill({5, 5, 10, 10}, {0, 0, 255, 128});                      // half-transparent blue over red
    int r = c.pixels()[(7 * 40 + 7) * 4], b = c.pixels()[(7 * 40 + 7) * 4 + 2];
    CHECK(r > 100 && r < 140 && b > 100 && b < 140);
    c.set_clip({20, 0, 5, 30});
    c.fill({0, 0, 40, 30}, {0, 255, 0, 255});
    CHECK_EQ(int(c.pixels()[(10 * 40 + 22) * 4 + 1]), 255);
    CHECK_EQ(int(c.pixels()[(10 * 40 + 30) * 4 + 1]), 20);
    c.reset_clip();
    Canvas rr(40, 40);
    rr.clear({0, 0, 0, 255});
    rr.fill_round({4, 4, 32, 32}, 10, {255, 255, 255, 255});
    CHECK_EQ(int(rr.pixels()[(20 * 40 + 20) * 4]), 255);
    CHECK(rr.pixels()[(4 * 40 + 4) * 4] < 20);                      // corner is cut away
    int edge = rr.pixels()[(20 * 40 + 4) * 4];                      // straight edge pixel is fully covered
    CHECK(edge > 240);
    Color col = Color::parse("#336699cc");
    CHECK_EQ(int(col.r), 0x33); CHECK_EQ(int(col.a), 0xcc);
    CHECK_EQ(int(Color::parse("#fff").g), 255);
    CHECK_EQ(int(Color::parse("garbage", {1, 2, 3, 4}).b), 3);
}

TEST(canvas_blit_scales_and_blends_premultiplied) {
    Image src; src.w = 2; src.h = 2; src.premultiplied = true;
    src.px = {255, 0, 0, 255,   0, 0, 0, 0,
              255, 0, 0, 255,   0, 0, 0, 0};
    Canvas c(20, 20);
    c.clear({0, 0, 255, 255});
    c.blit(src, {0, 0, 20, 20});
    CHECK_EQ(int(c.pixels()[(10 * 20 + 1) * 4]), 255);               // left half is the opaque red column
    CHECK_EQ(int(c.pixels()[(10 * 20 + 18) * 4 + 2]), 255);          // right half shows the blue background
    c.clear({0, 0, 255, 255});
    c.blit(src, {0, 0, 20, 20}, 0.5f);
    int r = c.pixels()[(10 * 20 + 1) * 4];
    CHECK(r > 120 && r < 135);
    c.clear({0, 0, 255, 255});
    c.blit(src, {0, 0, 20, 20}, 1.f, 1.f, true);                     // mirrored: red on the right
    CHECK_EQ(int(c.pixels()[(10 * 20 + 18) * 4]), 255);
}

TEST(canvas_transitions_endpoints) {
    Canvas a(8, 4), b(8, 4), out(8, 4);
    a.clear({255, 0, 0, 255}); b.clear({0, 0, 255, 255});
    out.mix(a, b, 0.0f);   CHECK_EQ(int(out.pixels()[0]), 255);
    out.mix(a, b, 1.0f);   CHECK_EQ(int(out.pixels()[2]), 255 - 1 > 250 ? int(out.pixels()[2]) : 0);
    CHECK(out.pixels()[2] > 250 && out.pixels()[0] < 5);
    out.slide(a, b, 0.0f, -1);  CHECK_EQ(int(out.pixels()[0]), 255);
    out.slide(a, b, 1.0f, -1);  CHECK(out.pixels()[2] > 250);
    out.wipe(a, b, 0.0f, 1, 4); CHECK_EQ(int(out.pixels()[0]), 255);
    out.wipe(a, b, 1.0f, 1, 4); CHECK(out.pixels()[2] > 250);
    out.flash(a, b, 0.25f);     CHECK(out.pixels()[1] > 250);          // peak of the flash is white
    out.flash(a, b, 1.0f);      CHECK(out.pixels()[2] > 250 && out.pixels()[0] < 5);
}

TEST(render_fill_round_span_path_matches_per_pixel_blend) {
    Canvas c(120, 80);
    c.clear({200, 40, 10, 255});
    c.fill_round({10, 10, 100, 60}, 12, {20, 100, 240, 128});
    // inside the fully covered core: plain 50% blend, rounded like Canvas::put does
    const uint8_t* p = &c.pixels()[(static_cast<size_t>(40) * 120 + 60) * 4];
    CHECK_NEAR(p[0], 200 + (20 - 200) * (128 / 255.0), 1.01);
    CHECK_NEAR(p[1], 40 + (100 - 40) * (128 / 255.0), 1.01);
    CHECK_NEAR(p[2], 10 + (240 - 10) * (128 / 255.0), 1.01);
    // outside the rectangle and in the clipped-away corner: untouched
    const uint8_t* o = &c.pixels()[(static_cast<size_t>(5) * 120 + 5) * 4];
    CHECK_EQ(int(o[0]), 200);
    const uint8_t* k = &c.pixels()[(static_cast<size_t>(10) * 120 + 10) * 4];
    CHECK_EQ(int(k[0]), 200);                       // the very corner pixel is outside the rounded shape
    // clip rectangles are honoured by the span path
    Canvas d(120, 80);
    d.clear({0, 0, 0, 255});
    d.set_clip({50, 30, 20, 10});
    d.fill_round({10, 10, 100, 60}, 12, {255, 255, 255, 255});
    CHECK_EQ(int(d.pixels()[(static_cast<size_t>(35) * 120 + 60) * 4]), 255);
    CHECK_EQ(int(d.pixels()[(static_cast<size_t>(35) * 120 + 40) * 4]), 0);
    CHECK_EQ(int(d.pixels()[(static_cast<size_t>(50) * 120 + 60) * 4]), 0);
}
