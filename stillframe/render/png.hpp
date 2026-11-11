// PNG reading and writing, no external libraries. Pixels are RGBA8.
#pragma once
#include "../core/common.hpp"

namespace sf {

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px;       // w*h*4, RGBA
    bool premultiplied = false;    // colour already multiplied by alpha (what the blitter wants)
    bool empty() const { return w == 0 || h == 0; }
    uint8_t* at(int x, int y) { return &px[(static_cast<size_t>(y) * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[(static_cast<size_t>(y) * w + x) * 4]; }
    void premultiply();
    // Area-averaged resize (good for shrinking); premultiplies first if needed.
    Image resized(int nw, int nh) const;
};

Image png_decode(const Bytes& data);               // throws Error with a readable message
Bytes png_encode(const Image& img);                // straight alpha expected; uses stored deflate blocks
void png_save(const std::string& path, const Image& img);

} // namespace sf
