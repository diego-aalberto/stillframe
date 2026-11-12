// Virtual file system: a game is either a folder or a single .stillgame (ZIP) file.
// Everything the engine reads from a game goes through here, so both behave identically.
#pragma once
#include "common.hpp"
#include <fstream>
#include <map>
#include <memory>
#include <mutex>

namespace sf {

class Vfs {
public:
    virtual ~Vfs() = default;
    virtual bool exists(const std::string& path) const = 0;
    virtual Bytes read(const std::string& path) const = 0;      // throws Error if missing
    virtual std::vector<std::string> list() const = 0;         // every file, '/'-separated, sorted
    virtual std::string describe() const = 0;
    virtual bool is_archive() const = 0;
    // Real filesystem path of a file, if the file lives on disk (needed to launch minigame executables).
    virtual std::string disk_path(const std::string& path) const { (void)path; return {}; }
    std::string read_text(const std::string& path) const { return to_string(read(path)); }
    std::vector<std::string> list_dir(const std::string& dir, const std::string& ext = "") const;

    // Opens a directory or a ZIP/.stillgame file.
    static std::unique_ptr<Vfs> open(const std::string& path);
};

std::unique_ptr<Vfs> open_directory(const std::string& dir);
std::unique_ptr<Vfs> open_zip(const std::string& file);

// Writes a ZIP ("stored" entries, deterministic timestamps). Used by `stillframe pack`.
class ZipWriter {
public:
    explicit ZipWriter(const std::string& out_file);
    ~ZipWriter();
    void add(const std::string& name, const Bytes& data);
    void finish();
private:
    struct Entry { std::string name; uint32_t crc, size, offset; };
    std::ofstream out_;
    std::vector<Entry> entries_;
    bool finished_ = false;
    void u16(uint16_t v);
    void u32(uint32_t v);
};

// Packs every file under `dir` (skipping dotfiles and `build`/`saves` folders). Returns file count.
size_t pack_directory(const std::string& dir, const std::string& out_file);

} // namespace sf
