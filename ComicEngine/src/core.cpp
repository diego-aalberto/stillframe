#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace comic {
namespace {
constexpr size_t maxFile = 256u * 1024u * 1024u;
void require(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
bool finite(float x) {
    return std::isfinite(x) && std::abs(x) < 1000000;
}
void check(Vec2 p) {
    require(finite(p.x) && finite(p.y), "Invalid coordinate");
}
float length(Vec2 p) {
    return std::sqrt(p.x * p.x + p.y * p.y);
}
float distance(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    float q = d.x * d.x + d.y * d.y;
    float t = q > 0 ? std::clamp(((p - a).x * d.x + (p - a).y * d.y) / q, 0.f, 1.f) : 0;
    return length(p - (a + d * t));
}
struct Writer {
    std::vector<uint8_t> data;
    void u(uint32_t n) {
        for (int i = 0; i < 4; i++)
            data.push_back(uint8_t(n >> (8 * i)));
    }
    void f(float n) {
        uint32_t b;
        std::memcpy(&b, &n, 4);
        u(b);
    }
    void s(const std::string &s) {
        u(uint32_t(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }
    void v(Vec2 p) {
        f(p.x);
        f(p.y);
    }
    void c(Color c) {
        data.insert(data.end(), {c.r, c.g, c.b, c.a});
    }
    void magic(const char *p) {
        data.insert(data.end(), p, p + 4);
        u(1);
    }
};
struct Reader {
    std::vector<uint8_t> data;
    size_t at = 0;
    explicit Reader(const std::filesystem::path &p) {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        require(bool(f), "Cannot open file");
        auto n = f.tellg();
        require(n >= 8 && n <= std::streamoff(maxFile), "Invalid file size (limit 256 MiB)");
        data.resize(size_t(n));
        f.seekg(0);
        require(bool(f.read(reinterpret_cast<char *>(data.data()), n)), "Cannot read file");
    }
    void need(size_t n) {
        require(n <= data.size() - at, "Truncated file");
    }
    uint32_t u() {
        need(4);
        uint32_t n = 0;
        for (int i = 0; i < 4; i++)
            n |= uint32_t(data[at++]) << (8 * i);
        return n;
    }
    uint32_t count(uint32_t max) {
        auto n = u();
        require(n <= max, "Count exceeds format limit");
        return n;
    }
    float f() {
        auto b = u();
        float n;
        std::memcpy(&n, &b, 4);
        require(finite(n), "Invalid float");
        return n;
    }
    std::string s() {
        auto n = count(65536);
        need(n);
        std::string s(reinterpret_cast<char *>(data.data() + at), n);
        at += n;
        return s;
    }
    Vec2 v() {
        float x = f(), y = f();
        return {x, y};
    }
    Color c() {
        need(4);
        Color c{data[at], data[at + 1], data[at + 2], data[at + 3]};
        at += 4;
        return c;
    }
    void magic(const char *p) {
        need(8);
        require(std::memcmp(data.data(), p, 4) == 0, "Wrong file type");
        at = 4;
        require(u() == 1, "Unsupported format version");
    }
    void end() {
        require(at == data.size(), "Unexpected trailing data");
    }
    bool boolean() {
        auto n = u();
        require(n <= 1, "Invalid boolean");
        return n != 0;
    }
};
void commit(const Writer &w, const std::filesystem::path &p) {
    require(w.data.size() <= maxFile, "Scene exceeds 256 MiB limit");
    auto temp = p;
    temp += L".tmp";
    {
        std::ofstream f(temp, std::ios::binary | std::ios::trunc);
        require(bool(f), "Cannot create save file");
        f.write(reinterpret_cast<const char *>(w.data.data()), std::streamsize(w.data.size()));
        f.flush();
        require(bool(f), "Could not write save file");
    }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace destination file");
#else
    std::filesystem::rename(temp, p);
#endif
}
void validImage(const Image &i) {
    require(i.width > 0 && i.height > 0 && i.width <= 4096 && i.height <= 4096,
            "Image dimensions must be 1..4096");
    require(i.rgba.size() == size_t(i.width) * i.height * 4, "Invalid image bytes");
}
void writeImage(Writer &w, const Image &i) {
    validImage(i);
    w.u(i.width);
    w.u(i.height);
    w.data.insert(w.data.end(), i.rgba.begin(), i.rgba.end());
}
Image readImage(Reader &r) {
    Image i;
    i.width = r.count(4096);
    i.height = r.count(4096);
    require(i.width && i.height, "Empty image");
    size_t n = size_t(i.width) * i.height * 4;
    r.need(n);
    i.rgba.assign(r.data.begin() + r.at, r.data.begin() + r.at + n);
    r.at += n;
    return i;
}
void validFont(const Font &f) {
    validImage(f.atlas);
    require(!f.name.empty() && f.name.size() <= 65536, "Invalid font name");
    require(finite(f.cellWidth) && finite(f.cellHeight) && finite(f.lineHeight) && f.cellWidth > 0 &&
                f.cellHeight > 0 && f.lineHeight > 0 && f.cellWidth * 16 <= f.atlas.width &&
                f.cellHeight * 14 <= f.atlas.height,
            "Invalid font metrics");
    for (auto g : f.glyphs)
        require(finite(g.advance) && g.advance >= 0 && g.advance <= f.cellWidth, "Invalid glyph advance");
}
void writeFont(Writer &w, const Font &f) {
    validFont(f);
    w.s(f.name);
    w.f(f.cellWidth);
    w.f(f.cellHeight);
    w.f(f.lineHeight);
    for (auto g : f.glyphs)
        w.f(g.advance);
    writeImage(w, f.atlas);
}
Font readFont(Reader &r) {
    Font f;
    f.name = r.s();
    f.cellWidth = r.f();
    f.cellHeight = r.f();
    f.lineHeight = r.f();
    for (auto &g : f.glyphs)
        g.advance = r.f();
    f.atlas = readImage(r);
    validFont(f);
    return f;
}
void name(const std::string &s) {
    require(!s.empty() && s.size() <= 65536, "Invalid or empty name");
}
template <class T> void unique(const std::vector<T> &list) {
    for (size_t i = 0; i < list.size(); i++)
        for (size_t j = 0; j < i; j++)
            require(list[i].id != list[j].id, "Duplicate ID");
}
std::string escaped(const std::string &s) {
    std::string o = "\"";
    const char *hex = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
            o += char(c);
        } else if (c < 32) {
            o += "\\u00";
            o += hex[c >> 4];
            o += hex[c & 15];
        } else
            o += char(c);
    }
    return o + '"';
}
} // namespace
void validate(const Scene &s) {
    size_t payload = 64 + s.title.size(), vertices = 0;
    name(s.title);
    require(s.width >= 64 && s.height >= 64 && s.width <= 8192 && s.height <= 8192,
            "Invalid scene dimensions");
    require(finite(s.poseRate) && s.poseRate >= 0 && s.poseRate <= 120, "Invalid pose rate");
    require(s.images.size() <= 128 && s.fonts.size() <= 32 && s.backgrounds.size() <= 32 &&
                s.characters.size() <= 32,
            "Scene count limit exceeded");
    for (auto &i : s.images) {
        validImage(i);
        payload += 8 + i.rgba.size();
    }
    for (auto &f : s.fonts) {
        validFont(f);
        payload += 920 + f.name.size() + f.atlas.rgba.size();
    }
    unique(s.characters);
    for (auto &b : s.backgrounds) {
        payload += 20;
        require(b.image < s.images.size(), "Missing background image");
        check(b.position);
        check(b.size);
        require(b.size.x > 0 && b.size.y > 0, "Invalid background size");
    }
    for (auto &c : s.characters) {
        payload += 52 + c.id.size() + c.text.size();
        name(c.id);
        check(c.position);
        require(c.text.size() <= 65536, "Dialogue too long");
        require(c.font < s.fonts.size(), "Missing character font");
        check(c.bubble.offset);
        require(finite(c.bubble.width) && c.bubble.width >= 80 && c.bubble.width <= 4096,
                "Invalid bubble width");
        require(!c.layers.empty() && c.layers.size() <= 64, "Character needs 1..64 image layers");
        unique(c.layers);
        for (auto &l : c.layers) {
            payload += 44 + l.id.size();
            name(l.id);
            require(l.image < s.images.size(), "Missing layer image");
            check(l.offset);
            require(finite(l.scale) && l.scale > 0 && l.scale <= 100, "Invalid image scale");
            require(l.grid >= 2 && l.grid <= 64, "Grid must be 2..64");
            vertices += size_t(l.grid + 1) * (l.grid + 1);
            require(vertices <= 1000000, "Scene exceeds one million mesh vertices");
            require(!l.joints.empty() && l.joints.size() <= 64, "Rig needs 1..64 joints");
            require(!l.clips.empty() && l.clips.size() <= 128 && l.active < l.clips.size(),
                    "Invalid clip selection");
            for (size_t j = 0; j < l.joints.size(); j++) {
                auto &x = l.joints[j];
                payload += 16 + x.name.size();
                name(x.name);
                check(x.bind);
                require((j == 0 && x.parent == -1) || (j > 0 && x.parent >= 0 && x.parent < int(j)),
                        "Joint parent must precede child; only joint 0 is root");
            }
            for (size_t a = 0; a < l.clips.size(); a++) {
                auto &clip = l.clips[a];
                payload += 16 + clip.name.size();
                name(clip.name);
                for (size_t b = 0; b < a; b++)
                    require(clip.name != l.clips[b].name, "Duplicate clip name");
                require(finite(clip.duration) && clip.duration > 0 && clip.duration <= 3600,
                        "Duration must be 0..3600 seconds");
                require(clip.keys.size() <= 4096, "Too many keys");
                float last = -1;
                for (auto &k : clip.keys) {
                    payload += 4 + k.points.size() * 8;
                    require(finite(k.time) && k.time >= 0 && k.time <= clip.duration && k.time > last,
                            "Keys must be ordered, unique and inside clip duration");
                    last = k.time;
                    require(k.points.size() == l.joints.size(), "Key joint count differs from rig");
                    for (auto p : k.points)
                        check(p);
                }
            }
        }
        require(payload <= maxFile, "Scene exceeds 256 MiB serialized size limit");
    }
    require(payload <= maxFile, "Scene exceeds 256 MiB serialized size limit");
}

void compactAssets(Scene &s) {
    validate(s);
    std::vector<uint32_t> imageMap(s.images.size(), UINT32_MAX), fontMap(s.fonts.size(), UINT32_MAX);
    for (auto &b : s.backgrounds)
        imageMap[b.image] = 0;
    for (auto &c : s.characters) {
        fontMap[c.font] = 0;
        for (auto &l : c.layers)
            imageMap[l.image] = 0;
    }
    // Keep one default font for creating the first character in an empty editor scene.
    if (s.characters.empty() && !s.fonts.empty())
        fontMap[0] = 0;
    std::vector<Image> images;
    std::vector<Font> fonts;
    for (size_t i = 0; i < s.images.size(); ++i)
        if (imageMap[i] == 0) {
            imageMap[i] = uint32_t(images.size());
            images.push_back(std::move(s.images[i]));
        }
    for (size_t i = 0; i < s.fonts.size(); ++i)
        if (fontMap[i] == 0) {
            fontMap[i] = uint32_t(fonts.size());
            fonts.push_back(std::move(s.fonts[i]));
        }
    for (auto &b : s.backgrounds)
        b.image = imageMap[b.image];
    for (auto &c : s.characters) {
        c.font = fontMap[c.font];
        for (auto &l : c.layers)
            l.image = imageMap[l.image];
    }
    s.images = std::move(images);
    s.fonts = std::move(fonts);
}
void buildMesh(Layer &l, const Image &image) {
    l.mesh.clear();
    l.indices.clear();
    auto n = l.grid;
    l.mesh.reserve((n + 1) * (n + 1));
    for (uint32_t y = 0; y <= n; y++)
        for (uint32_t x = 0; x <= n; x++) {
            Vertex v;
            v.uv = {float(x) / n, float(y) / n};
            v.bind = {v.uv.x * image.width, v.uv.y * image.height};
            float d1 = 1e30f, d2 = 1e30f;
            uint16_t a = 0, b = 0;
            for (uint16_t j = 1; j < l.joints.size(); j++) {
                auto &joint = l.joints[j];
                float d = distance(v.bind, l.joints[joint.parent].bind, joint.bind);
                if (d < d1) {
                    d2 = d1;
                    b = a;
                    d1 = d;
                    a = j;
                } else if (d < d2) {
                    d2 = d;
                    b = j;
                }
            }
            if (l.joints.size() < 3) {
                b = a;
                v.weight = 1;
            } else {
                float w1 = 1.f / ((d1 + 4) * (d1 + 4)), w2 = 1.f / ((d2 + 4) * (d2 + 4));
                v.weight = w1 / (w1 + w2);
            }
            v.a = a;
            v.b = b;
            l.mesh.push_back(v);
        }
    for (uint32_t y = 0; y < n; y++)
        for (uint32_t x = 0; x < n; x++) {
            uint32_t a = y * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
            l.indices.insert(l.indices.end(), {a, c, b, b, c, d});
        }
    l.deformed.resize(l.mesh.size());
}
std::vector<Vec2> sample(const Layer &l, float t, float rate) {
    const auto &c = l.clips.at(l.active);
    t = std::max(0.f, t);
    if (c.loop)
        t = std::fmod(t, c.duration);
    else
        t = std::min(t, c.duration);
    if (rate > 0 && t < c.duration)
        t = std::floor(t * rate + 0.00001f) / rate;
    if (c.keys.empty()) {
        std::vector<Vec2> p;
        for (auto &j : l.joints)
            p.push_back(j.bind);
        return p;
    }
    auto next =
        std::upper_bound(c.keys.begin(), c.keys.end(), t, [](float v, const Key &k) { return v < k.time; });
    if (next == c.keys.begin())
        return next->points;
    if (next == c.keys.end())
        return c.keys.back().points;
    auto &a = *(next - 1);
    auto &b = *next;
    float f = (t - a.time) / (b.time - a.time);
    std::vector<Vec2> p(a.points.size());
    for (size_t i = 0; i < p.size(); i++)
        p[i] = a.points[i] * (1 - f) + b.points[i] * f;
    return p;
}
void deform(Layer &l, const std::vector<Vec2> &p) {
    require(p.size() == l.joints.size(), "Pose size differs from rig");
    struct Transform {
        Vec2 bind, pos;
        float xx = 1, xy = 0, yx = 0, yy = 1;
    };
    std::array<Transform, 64> tr{};
    for (size_t j = 0; j < l.joints.size(); j++) {
        auto &a = l.joints[j];
        size_t parent = a.parent < 0 ? j : size_t(a.parent);
        auto &m = tr[j];
        m.bind = l.joints[parent].bind;
        m.pos = p[parent];
        Vec2 old = a.bind - m.bind, now = p[j] - m.pos;
        float before = length(old), after = length(now);
        if (before > .001f) {
            Vec2 e = old * (1 / before), q = now * (1 / before);
            Vec2 n = after > .001f ? Vec2{-now.y / after, now.x / after} : Vec2{-e.y, e.x};
            m.xx = q.x * e.x - n.x * e.y;
            m.xy = q.x * e.y + n.x * e.x;
            m.yx = q.y * e.x - n.y * e.y;
            m.yy = q.y * e.y + n.y * e.x;
        }
    }
    auto apply = [&](uint16_t j, Vec2 v) {
        const auto &m = tr[j];
        v = v - m.bind;
        return m.pos + Vec2{v.x * m.xx + v.y * m.xy, v.x * m.yx + v.y * m.yy};
    };
    for (size_t i = 0; i < l.mesh.size(); i++) {
        auto &v = l.mesh[i];
        l.deformed[i] = apply(v.a, v.bind) * v.weight + apply(v.b, v.bind) * (1 - v.weight);
    }
}
void prepare(Scene &s) {
    validate(s);
    for (auto &c : s.characters)
        for (auto &l : c.layers) {
            buildMesh(l, s.images[l.image]);
            deform(l, sample(l, 0, s.poseRate));
        }
}
void saveScene(const Scene &s, const std::filesystem::path &p) {
    validate(s);
    Writer w;
    w.magic("CSCN");
    w.s(s.title);
    w.u(s.width);
    w.u(s.height);
    w.f(s.poseRate);
    w.c(s.clear);
    w.u(uint32_t(s.images.size()));
    for (auto &i : s.images)
        writeImage(w, i);
    w.u(uint32_t(s.fonts.size()));
    for (auto &f : s.fonts)
        writeFont(w, f);
    w.u(uint32_t(s.backgrounds.size()));
    for (auto &b : s.backgrounds) {
        w.u(b.image);
        w.v(b.position);
        w.v(b.size);
    }
    w.u(uint32_t(s.characters.size()));
    for (auto &c : s.characters) {
        w.s(c.id);
        w.s(c.text);
        w.v(c.position);
        w.u(c.font);
        w.c(c.bubble.fill);
        w.c(c.bubble.edge);
        w.c(c.bubble.text);
        w.f(c.bubble.width);
        w.v(c.bubble.offset);
        w.u(uint32_t(c.layers.size()));
        for (auto &l : c.layers) {
            w.s(l.id);
            w.u(l.image);
            w.u(l.visible);
            w.v(l.offset);
            w.f(l.scale);
            w.u(l.grid);
            w.u(l.active);
            w.u(uint32_t(l.joints.size()));
            for (auto &j : l.joints) {
                w.s(j.name);
                w.u(uint32_t(j.parent + 1));
                w.v(j.bind);
            }
            w.u(uint32_t(l.clips.size()));
            for (auto &a : l.clips) {
                w.s(a.name);
                w.f(a.duration);
                w.u(a.loop);
                w.u(uint32_t(a.keys.size()));
                for (auto &k : a.keys) {
                    w.f(k.time);
                    for (auto v : k.points)
                        w.v(v);
                }
            }
        }
    }
    commit(w, p);
}
Scene loadScene(const std::filesystem::path &p) {
    Reader r(p);
    r.magic("CSCN");
    Scene s;
    s.title = r.s();
    s.width = r.u();
    s.height = r.u();
    s.poseRate = r.f();
    s.clear = r.c();
    auto n = r.count(128);
    for (uint32_t i = 0; i < n; i++)
        s.images.push_back(readImage(r));
    n = r.count(32);
    for (uint32_t i = 0; i < n; i++)
        s.fonts.push_back(readFont(r));
    n = r.count(32);
    for (uint32_t i = 0; i < n; i++) {
        Background b;
        b.image = r.u();
        b.position = r.v();
        b.size = r.v();
        s.backgrounds.push_back(b);
    }
    n = r.count(32);
    for (uint32_t i = 0; i < n; i++) {
        Character c;
        c.id = r.s();
        c.text = r.s();
        c.position = r.v();
        c.font = r.u();
        c.bubble.fill = r.c();
        c.bubble.edge = r.c();
        c.bubble.text = r.c();
        c.bubble.width = r.f();
        c.bubble.offset = r.v();
        auto layers = r.count(64);
        for (uint32_t j = 0; j < layers; j++) {
            Layer l;
            l.id = r.s();
            l.image = r.u();
            l.visible = r.boolean();
            l.offset = r.v();
            l.scale = r.f();
            l.grid = r.u();
            l.active = r.u();
            auto joints = r.count(64);
            for (uint32_t k = 0; k < joints; k++) {
                Joint b;
                b.name = r.s();
                auto parent = r.count(64);
                b.parent = int(parent) - 1;
                b.bind = r.v();
                l.joints.push_back(b);
            }
            auto clips = r.count(128);
            for (uint32_t k = 0; k < clips; k++) {
                Clip a;
                a.name = r.s();
                a.duration = r.f();
                a.loop = r.boolean();
                auto keys = r.count(4096);
                for (uint32_t q = 0; q < keys; q++) {
                    Key key;
                    key.time = r.f();
                    for (uint32_t z = 0; z < joints; z++)
                        key.points.push_back(r.v());
                    a.keys.push_back(std::move(key));
                }
                l.clips.push_back(std::move(a));
            }
            c.layers.push_back(std::move(l));
        }
        s.characters.push_back(std::move(c));
    }
    r.end();
    prepare(s);
    return s;
}
Image loadImage(const std::filesystem::path &p) {
    Reader r(p);
    r.magic("CIMG");
    auto i = readImage(r);
    r.end();
    return i;
}
void saveImage(const Image &i, const std::filesystem::path &p) {
    Writer w;
    w.magic("CIMG");
    writeImage(w, i);
    commit(w, p);
}
Font loadFont(const std::filesystem::path &p) {
    Reader r(p);
    r.magic("CFNT");
    auto f = readFont(r);
    r.end();
    return f;
}
void saveFont(const Font &f, const std::filesystem::path &p) {
    Writer w;
    w.magic("CFNT");
    writeFont(w, f);
    commit(w, p);
}
bool play(Scene &s, const std::string &c, const std::string &l, const std::string &a) {
    for (auto &ch : s.characters)
        if (ch.id == c)
            for (auto &layer : ch.layers)
                if (layer.id == l)
                    for (size_t i = 0; i < layer.clips.size(); i++)
                        if (layer.clips[i].name == a) {
                            layer.active = uint32_t(i);
                            return true;
                        }
    return false;
}
bool say(Scene &s, const std::string &id, const std::string &text) {
    if (text.size() > 65536)
        return false;
    for (auto &c : s.characters)
        if (c.id == id) {
            c.text = text;
            return true;
        }
    return false;
}
std::string manifest(const Scene &s) {
    std::ostringstream o;
    o << "{\"title\":" << escaped(s.title) << ",\"characters\":[";
    bool first = true;
    for (auto &c : s.characters) {
        if (!first)
            o << ',';
        first = false;
        o << "{\"id\":" << escaped(c.id) << ",\"layers\":[";
        bool lf = true;
        for (auto &l : c.layers) {
            if (!lf)
                o << ',';
            lf = false;
            o << "{\"id\":" << escaped(l.id) << ",\"clips\":[";
            for (size_t i = 0; i < l.clips.size(); i++) {
                if (i)
                    o << ',';
                o << escaped(l.clips[i].name);
            }
            o << "]}";
        }
        o << "]}";
    }
    o << "]}";
    return o.str();
}
} // namespace comic
