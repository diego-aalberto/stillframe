#include "assets.hpp"
#include "../core/platform.hpp"
#include <algorithm>

namespace sf {

static Image placeholder(int w, int h) {
    Image im; im.w = w; im.h = h; im.premultiplied = true; im.px.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        bool on = ((x / 16) + (y / 16)) % 2 == 0;
        uint8_t* p = im.at(x, y);
        p[0] = on ? 220 : 90; p[1] = 20; p[2] = on ? 200 : 90; p[3] = 255;
    }
    return im;
}

std::shared_ptr<const Image> AssetCache::store(const std::string& key, Image img) {
    img.premultiply();
    bytes_ += img.px.size();
    auto sp = std::make_shared<const Image>(std::move(img));
    lru_.push_front(key);
    cache_[key] = {sp, lru_.begin()};
    evict();
    return sp;
}

void AssetCache::evict() {
    while (bytes_ > budget_ && lru_.size() > 1) {
        auto it = cache_.find(lru_.back());
        if (it != cache_.end()) { bytes_ -= it->second.img->px.size(); cache_.erase(it); }
        lru_.pop_back();
    }
}

std::shared_ptr<const Image> AssetCache::image(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) { lru_.splice(lru_.begin(), lru_, it->second.lru); return it->second.img; }
    Image img;
    try {
        if (!vfs_) throw Error("no game loaded");
        img = png_decode(vfs_->read(path));
    } catch (const Error& e) {
        errors_.push_back(path + ": " + e.what());
        img = placeholder(256, 256);
    }
    return store(path, std::move(img));
}

std::shared_ptr<const Image> AssetCache::scaled(const std::string& path, int w, int h) {
    auto base = image(path);
    if (base->w == w && base->h == h) return base;
    std::string key = path + "@" + std::to_string(w) + "x" + std::to_string(h);
    auto it = cache_.find(key);
    if (it != cache_.end()) { lru_.splice(lru_.begin(), lru_, it->second.lru); return it->second.img; }
    return store(key, base->resized(w, h));
}

std::shared_ptr<Font> AssetCache::font_from_package(const std::string& path) {
    if (!vfs_) throw Error("no game loaded");
    return Font::load(vfs_->read(path));
}

std::shared_ptr<Font> AssetCache::font_from_engine(const std::string& file) {
    return Font::load(to_bytes(read_file(assets_dir() + "/fonts/" + file)));
}

} // namespace sf
