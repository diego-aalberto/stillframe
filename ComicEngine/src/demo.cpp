#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace comic {
namespace {
struct Canvas {
    Image image;
    Canvas(int w, int h, Color c) : image{uint32_t(w), uint32_t(h), std::vector<uint8_t>(size_t(w) * h * 4)} {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                pixel(x, y, c);
    }
    void pixel(int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= int(image.width) || y >= int(image.height))
            return;
        size_t i = (size_t(y) * image.width + x) * 4;
        image.rgba[i] = c.r;
        image.rgba[i + 1] = c.g;
        image.rgba[i + 2] = c.b;
        image.rgba[i + 3] = c.a;
    }
    void rect(int x, int y, int w, int h, Color c) {
        for (int j = y; j < y + h; j++)
            for (int i = x; i < x + w; i++)
                pixel(i, j, c);
    }
    void circle(int x, int y, int r, Color c) {
        for (int j = -r; j <= r; j++)
            for (int i = -r; i <= r; i++)
                if (i * i + j * j <= r * r)
                    pixel(x + i, y + j, c);
    }
    void line(int x, int y, int xx, int yy, int r, Color c) {
        int steps = std::max(std::abs(xx - x), std::abs(yy - y));
        for (int i = 0; i <= steps; i++) {
            float t = steps ? float(i) / steps : 0;
            circle(int(x + (xx - x) * t), int(y + (yy - y) * t), r, c);
        }
    }
};
} // namespace
Scene demo(Font font) {
    Scene s;
    s.title = "MIDNIGHT / a living comic";
    s.fonts.push_back(std::move(font));
    Color ink{25, 27, 44, 255}, paper{242, 223, 177, 255}, teal{88, 191, 178, 255}, orange{250, 155, 92, 255};
    Canvas bg(960, 640, ink);
    for (int y = 0; y < 640; y++) {
        float t = float(y) / 640;
        bg.rect(0, y, 960, 1, {uint8_t(32 + 25 * t), uint8_t(39 + 24 * t), uint8_t(61 + 17 * t), 255});
    }
    bg.rect(45, 42, 870, 466, {16, 24, 43, 255});
    bg.rect(51, 48, 858, 454, {48, 71, 99, 255});
    bg.circle(737, 142, 54, paper);
    bg.circle(716, 126, 49, {48, 71, 99, 255});
    for (int k = 0; k < 18; k++) {
        int x = 57 + k * 49, h = 80 + (k * 73) % 160;
        bg.rect(x, 486 - h, 43, h, {25, 36, 57, 255});
        for (int yy = 490 - h; yy < 472; yy += 21)
            for (int xx = x + 7; xx < x + 35; xx += 14)
                if ((xx + yy + k) % 3)
                    bg.rect(xx, yy, 5, 9, {125, 123, 123, 255});
    }
    for (int x = 66; x < 920; x += 151)
        bg.rect(x, 48, 6, 454, ink);
    bg.rect(48, 280, 862, 7, ink);
    bg.rect(0, 503, 960, 10, ink);
    bg.rect(0, 513, 960, 127, {61, 60, 72, 255});
    for (int y = 540; y < 640; y += 30)
        bg.rect(0, y, 960, 2, {79, 74, 83, 255});
    bg.rect(82, 583, 310, 7, ink);
    bg.rect(101, 590, 11, 50, ink);
    bg.rect(355, 590, 11, 50, ink);
    s.images.push_back(std::move(bg.image));
    s.backgrounds.push_back({0, {0, 0}, {960, 640}});
    Canvas body(256, 384, {0, 0, 0, 0});
    auto limb = [&](int x, int y, int xx, int yy, int r, Color c) {
        body.line(x, y, xx, yy, r + 4, ink);
        body.line(x, y, xx, yy, r, c);
    };
    limb(109, 236, 96, 292, 13, teal);
    limb(96, 292, 88, 347, 12, teal);
    limb(147, 236, 161, 292, 13, teal);
    limb(161, 292, 177, 347, 12, teal);
    body.line(70, 354, 94, 354, 14, ink);
    body.line(172, 354, 198, 354, 14, ink);
    limb(80, 157, 49, 202, 12, orange);
    limb(49, 202, 42, 255, 10, teal);
    limb(176, 157, 211, 201, 12, orange);
    limb(211, 201, 225, 153, 10, teal);
    body.circle(42, 255, 15, ink);
    body.circle(42, 255, 11, paper);
    body.circle(225, 148, 15, ink);
    body.circle(225, 148, 11, paper);
    body.rect(83, 141, 89, 101, ink);
    body.rect(89, 146, 77, 90, teal);
    body.rect(99, 167, 58, 47, ink);
    body.rect(104, 172, 48, 37, paper);
    body.line(111, 194, 120, 184, 2, orange);
    body.line(120, 184, 131, 199, 2, orange);
    body.line(131, 199, 149, 180, 2, orange);
    body.circle(128, 134, 16, ink);
    body.rect(73, 44, 110, 83, ink);
    body.rect(79, 50, 98, 71, orange);
    body.rect(86, 65, 84, 39, ink);
    body.circle(109, 84, 7, paper);
    body.circle(149, 84, 7, paper);
    body.line(118, 112, 141, 112, 2, ink);
    limb(128, 44, 133, 22, 3, teal);
    body.circle(133, 18, 7, paper);
    s.images.push_back(std::move(body.image));
    Character c;
    c.id = "atlas";
    c.position = {568, 233};
    c.text = "The city is sleeping.\nMy imagination isn't.";
    c.bubble.offset = {-382, 56};
    c.bubble.width = 320;
    Layer l;
    l.id = "body";
    l.image = 1;
    l.grid = 24;
    l.joints = {
        {"hips", -1, {128, 236}},          {"chest", 0, {128, 151}},       {"head", 1, {128, 80}},
        {"left shoulder", 1, {80, 157}},   {"left elbow", 3, {49, 202}},   {"left hand", 4, {42, 255}},
        {"right shoulder", 1, {176, 157}}, {"right elbow", 6, {211, 201}}, {"right hand", 7, {225, 148}},
        {"left knee", 0, {96, 292}},       {"left foot", 9, {88, 347}},    {"right knee", 0, {161, 292}},
        {"right foot", 11, {177, 347}}};
    std::vector<Vec2> bind;
    for (auto &j : l.joints)
        bind.push_back(j.bind);
    Clip idle;
    idle.name = "breathe";
    idle.duration = 3;
    auto inhale = bind;
    for (size_t j = 1; j < 9; j++)
        inhale[j].y -= 4;
    idle.keys = {{0, bind}, {1.5f, inhale}, {3, bind}};
    l.clips.push_back(idle);
    Clip wave;
    wave.name = "wave";
    wave.duration = 1.4f;
    auto a = bind, b = bind;
    a[8] = {200, 132};
    b[8] = {242, 157};
    wave.keys = {{0, bind}, {.35f, a}, {.7f, b}, {1.05f, a}, {1.4f, bind}};
    l.clips.push_back(wave);
    Clip nod;
    nod.name = "think";
    nod.duration = 2;
    auto tilt = bind;
    tilt[2] = {144, 87};
    tilt[5] = {80, 227};
    nod.keys = {{0, bind}, {.7f, tilt}, {1.3f, tilt}, {2, bind}};
    l.clips.push_back(nod);
    c.layers.push_back(std::move(l));
    s.characters.push_back(std::move(c));
    prepare(s);
    return s;
}
} // namespace comic
