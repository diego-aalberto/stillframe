#include "png.hpp"
#include "../core/inflate.hpp"
#include "../core/platform.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace sf {

static uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }

void Image::premultiply() {
    if (premultiplied) return;
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        unsigned a = px[i + 3];
        if (a == 255) continue;
        px[i] = static_cast<uint8_t>((px[i] * a + 127) / 255);
        px[i + 1] = static_cast<uint8_t>((px[i + 1] * a + 127) / 255);
        px[i + 2] = static_cast<uint8_t>((px[i + 2] * a + 127) / 255);
    }
    premultiplied = true;
}

Image Image::resized(int nw, int nh) const {
    Image src = *this;
    src.premultiply();
    Image out;
    out.w = std::max(1, nw); out.h = std::max(1, nh);
    out.premultiplied = true;
    out.px.assign(static_cast<size_t>(out.w) * out.h * 4, 0);
    if (src.empty()) return out;
    // Separable box filter over the exact source footprint of each destination pixel.
    const double sx = double(src.w) / out.w, sy = double(src.h) / out.h;
    std::vector<float> tmp(static_cast<size_t>(out.w) * src.h * 4);
    for (int y = 0; y < src.h; ++y) {
        for (int x = 0; x < out.w; ++x) {
            double x0 = x * sx, x1 = (x + 1) * sx;
            int i0 = static_cast<int>(std::floor(x0)), i1 = std::min(src.w - 1, static_cast<int>(std::ceil(x1)) - 1);
            double acc[4] = {0, 0, 0, 0}, wsum = 0;
            for (int i = i0; i <= i1; ++i) {
                double w = std::min<double>(i + 1, x1) - std::max<double>(i, x0);
                if (w <= 0) continue;
                const uint8_t* p = src.at(i, y);
                for (int c = 0; c < 4; ++c) acc[c] += p[c] * w;
                wsum += w;
            }
            float* d = &tmp[(static_cast<size_t>(y) * out.w + x) * 4];
            for (int c = 0; c < 4; ++c) d[c] = static_cast<float>(wsum > 0 ? acc[c] / wsum : 0);
        }
    }
    for (int y = 0; y < out.h; ++y) {
        double y0 = y * sy, y1 = (y + 1) * sy;
        int j0 = static_cast<int>(std::floor(y0)), j1 = std::min(src.h - 1, static_cast<int>(std::ceil(y1)) - 1);
        for (int x = 0; x < out.w; ++x) {
            double acc[4] = {0, 0, 0, 0}, wsum = 0;
            for (int j = j0; j <= j1; ++j) {
                double w = std::min<double>(j + 1, y1) - std::max<double>(j, y0);
                if (w <= 0) continue;
                const float* p = &tmp[(static_cast<size_t>(j) * out.w + x) * 4];
                for (int c = 0; c < 4; ++c) acc[c] += p[c] * w;
                wsum += w;
            }
            uint8_t* d = out.at(x, y);
            for (int c = 0; c < 4; ++c) d[c] = static_cast<uint8_t>(std::clamp(wsum > 0 ? acc[c] / wsum + 0.5 : 0.0, 0.0, 255.0));
        }
    }
    return out;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

Image png_decode(const Bytes& d) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (d.size() < 8 || std::memcmp(d.data(), sig, 8) != 0) throw Error("not a PNG file");
    size_t p = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    Bytes idat, plte, trns;
    bool seen_ihdr = false, seen_end = false;
    while (p + 8 <= d.size() && !seen_end) {
        uint32_t len = be32(&d[p]);
        std::string type(reinterpret_cast<const char*>(&d[p + 4]), 4);
        if (p + 12 + static_cast<size_t>(len) > d.size()) throw Error("PNG is cut off");
        const uint8_t* body = &d[p + 8];
        uint32_t crc = be32(body + len);
        if (crc32(&d[p + 4], len + 4) != crc) throw Error("PNG chunk '" + type + "' is damaged");
        if (type == "IHDR") {
            if (len < 13) throw Error("bad PNG header");
            w = static_cast<int>(be32(body)); h = static_cast<int>(be32(body + 4));
            depth = body[8]; ctype = body[9]; interlace = body[12];
            seen_ihdr = true;
        } else if (type == "PLTE") plte.assign(body, body + len);
        else if (type == "tRNS") trns.assign(body, body + len);
        else if (type == "IDAT") idat.insert(idat.end(), body, body + len);
        else if (type == "IEND") seen_end = true;
        p += 12 + len;
    }
    if (!seen_ihdr || idat.empty()) throw Error("PNG has no image data");
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) throw Error("PNG size is not supported");
    if (interlace) throw Error("interlaced PNGs are not supported; re-save the image without interlacing");
    int channels = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : ctype == 6 ? 4 : 0;
    if (!channels) throw Error("unknown PNG colour type");
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16) throw Error("unsupported PNG bit depth");
    size_t bpp_bits = static_cast<size_t>(channels) * depth;
    size_t stride = (static_cast<size_t>(w) * bpp_bits + 7) / 8;
    size_t bpp = std::max<size_t>(1, bpp_bits / 8);
    Bytes raw = zlib_decompress(idat.data(), idat.size(), (stride + 1) * h);
    if (raw.size() < (stride + 1) * h) throw Error("PNG image data is incomplete");

    Bytes cur(stride), prev(stride, 0);
    Image img; img.w = w; img.h = h; img.px.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = &raw[(stride + 1) * y];
        int filter = row[0];
        std::memcpy(cur.data(), row + 1, stride);
        for (size_t i = 0; i < stride; ++i) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            switch (filter) {
                case 0: break;
                case 1: cur[i] = static_cast<uint8_t>(cur[i] + a); break;
                case 2: cur[i] = static_cast<uint8_t>(cur[i] + b); break;
                case 3: cur[i] = static_cast<uint8_t>(cur[i] + ((a + b) >> 1)); break;
                case 4: cur[i] = static_cast<uint8_t>(cur[i] + paeth(a, b, c)); break;
                default: throw Error("bad PNG filter");
            }
        }
        auto sample = [&](int x, int ch) -> int {      // value of channel `ch` at pixel x, scaled to 8 bits (or raw index for palette)
            if (depth == 8) return cur[static_cast<size_t>(x) * channels + ch];
            if (depth == 16) return cur[(static_cast<size_t>(x) * channels + ch) * 2];   // high byte
            size_t bit = static_cast<size_t>(x) * depth;
            int v = (cur[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
            return ctype == 3 ? v : v * 255 / ((1 << depth) - 1);
        };
        auto sample16 = [&](int x, int ch) -> int { const uint8_t* q = &cur[(static_cast<size_t>(x) * channels + ch) * 2]; return (q[0] << 8) | q[1]; };
        for (int x = 0; x < w; ++x) {
            uint8_t* o = img.at(x, y);
            switch (ctype) {
                case 0: {
                    int g = sample(x, 0); o[0] = o[1] = o[2] = static_cast<uint8_t>(g); o[3] = 255;
                    if (trns.size() >= 2) {
                        int key = (trns[0] << 8) | trns[1];
                        int raw_v = depth == 16 ? sample16(x, 0) : (depth == 8 ? g : g * ((1 << depth) - 1) / 255);
                        if (raw_v == key) o[3] = 0;
                    }
                    break;
                }
                case 2:
                    o[0] = static_cast<uint8_t>(sample(x, 0)); o[1] = static_cast<uint8_t>(sample(x, 1)); o[2] = static_cast<uint8_t>(sample(x, 2)); o[3] = 255;
                    if (trns.size() >= 6) {
                        bool match = depth == 16 ? (sample16(x, 0) == ((trns[0] << 8) | trns[1]) && sample16(x, 1) == ((trns[2] << 8) | trns[3]) && sample16(x, 2) == ((trns[4] << 8) | trns[5]))
                                                 : (o[0] == trns[1] && o[1] == trns[3] && o[2] == trns[5] && depth == 8);
                        if (match) o[3] = 0;
                    }
                    break;
                case 3: {
                    size_t idx = static_cast<size_t>(sample(x, 0));
                    if (idx * 3 + 2 >= plte.size()) throw Error("PNG palette index out of range");
                    o[0] = plte[idx * 3]; o[1] = plte[idx * 3 + 1]; o[2] = plte[idx * 3 + 2];
                    o[3] = idx < trns.size() ? trns[idx] : 255;
                    break;
                }
                case 4: { int g = sample(x, 0); o[0] = o[1] = o[2] = static_cast<uint8_t>(g); o[3] = static_cast<uint8_t>(sample(x, 1)); break; }
                case 6: for (int c = 0; c < 4; ++c) o[c] = static_cast<uint8_t>(sample(x, c)); break;
            }
        }
        std::swap(cur, prev);
    }
    return img;
}

static void put32(Bytes& b, uint32_t v) { b.push_back(v >> 24); b.push_back(v >> 16); b.push_back(v >> 8); b.push_back(v); }
static void chunk(Bytes& out, const char* type, const Bytes& body) {
    put32(out, static_cast<uint32_t>(body.size()));
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());
    put32(out, crc32(&out[start], out.size() - start));
}

Bytes png_encode(const Image& src) {
    Image img = src;
    if (img.premultiplied) {        // un-premultiply for storage
        for (size_t i = 0; i + 3 < img.px.size(); i += 4) {
            unsigned a = img.px[i + 3];
            if (a == 0 || a == 255) continue;
            for (int c = 0; c < 3; ++c) img.px[i + c] = static_cast<uint8_t>(std::min(255u, (img.px[i + c] * 255u + a / 2) / a));
        }
    }
    Bytes raw;
    raw.reserve((static_cast<size_t>(img.w) * 4 + 1) * img.h);
    for (int y = 0; y < img.h; ++y) { raw.push_back(0); raw.insert(raw.end(), img.at(0, y), img.at(0, y) + static_cast<size_t>(img.w) * 4); }
    Bytes z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); i += 5552) {
        size_t n = std::min<size_t>(5552, raw.size() - i);
        for (size_t k = 0; k < n; ++k) { a += raw[i + k]; b += a; }
        a %= 65521; b %= 65521;
    }
    for (size_t i = 0; i < raw.size() || i == 0;) {
        size_t n = std::min<size_t>(65535, raw.size() - i);
        bool last = i + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
        i += n;
        if (last) break;
    }
    put32(z, (b << 16) | a);
    Bytes out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    Bytes ihdr; put32(ihdr, img.w); put32(ihdr, img.h);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

void png_save(const std::string& path, const Image& img) {
    Bytes b = png_encode(img);
    std::ofstream f(path, std::ios::binary);
    if (!f) throw Error("cannot write " + path);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

} // namespace sf
