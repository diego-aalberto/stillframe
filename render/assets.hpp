// Loads and caches images (and the fonts a theme asks for) from a game package or the engine's assets.
#pragma once
#include "canvas.hpp"
#include "../core/vfs.hpp"
#include <list>
#include <map>
#include <memory>
#include <mutex>

namespace sf {

class AssetCache {
public:
    explicit AssetCache(const Vfs* vfs, size_t budget_bytes = size_t(768) << 20) : vfs_(vfs), budget_(budget_bytes) {}

    // Premultiplied image. A missing or broken file yields a visible placeholder (and an entry in errors()).
    std::shared_ptr<const Image> image(const std::string& path);
    // The same image resized to exactly w x h with area averaging, cached. Use for large shrinks.
    std::shared_ptr<const Image> scaled(const std::string& path, int w, int h);
    bool exists(const std::string& path) const { return vfs_ && vfs_->exists(path); }
    const std::vector<std::string>& errors() const { return errors_; }
    size_t bytes() const { return bytes_; }

    // Fonts from the game package (path inside it) or from the engine's assets folder.
    std::shared_ptr<Font> font_from_package(const std::string& path);
    static std::shared_ptr<Font> font_from_engine(const std::string& file_name);

private:
    struct Entry { std::shared_ptr<const Image> img; std::list<std::string>::iterator lru; };
    std::shared_ptr<const Image> store(const std::string& key, Image img);
    void evict();
    const Vfs* vfs_;
    size_t budget_, bytes_ = 0;
    std::map<std::string, Entry> cache_;
    std::list<std::string> lru_;
    std::vector<std::string> errors_;
};

} // namespace sf
