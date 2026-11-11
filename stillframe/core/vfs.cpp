#include "vfs.hpp"
#include "inflate.hpp"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace sf {

std::vector<std::string> Vfs::list_dir(const std::string& dir, const std::string& ext) const {
    std::string prefix = dir.empty() ? "" : normalize_path(dir) + "/";
    std::vector<std::string> out;
    for (auto& f : list())
        if (starts_with(f, prefix) && (ext.empty() || ends_with(f, ext))) out.push_back(f);
    return out;
}

std::unique_ptr<Vfs> Vfs::open(const std::string& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) return open_directory(path);
    if (fs::is_regular_file(path, ec)) return open_zip(path);
    throw Error("game not found: " + path);
}

// ------------------------------------------------------------------ folder
namespace {
class DirVfs : public Vfs {
public:
    explicit DirVfs(std::string root) : root_(fs::absolute(root).lexically_normal().string()) {}
    bool exists(const std::string& p) const override {
        std::error_code ec;
        return fs::is_regular_file(full(p), ec);
    }
    Bytes read(const std::string& p) const override {
        std::ifstream f(full(p), std::ios::binary);
        if (!f) throw Error("missing file: " + p);
        return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    std::vector<std::string> list() const override {
        std::vector<std::string> out;
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(root_, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file(ec)) out.push_back(fs::relative(it->path(), root_).generic_string());
        std::sort(out.begin(), out.end());
        return out;
    }
    std::string describe() const override { return root_; }
    bool is_archive() const override { return false; }
    std::string disk_path(const std::string& p) const override { return full(p); }
private:
    std::string root_;
    std::string full(const std::string& p) const { return (fs::path(root_) / normalize_path(p)).string(); }
};

// ------------------------------------------------------------------ zip
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

class ZipVfs : public Vfs {
public:
    explicit ZipVfs(std::string file) : file_(std::move(file)), in_(file_, std::ios::binary) {
        if (!in_) throw Error("cannot open package: " + file_);
        in_.seekg(0, std::ios::end);
        uint64_t size = static_cast<uint64_t>(in_.tellg());
        if (size < 22) throw Error("not a ZIP package: " + file_);
        size_t tail = static_cast<size_t>(std::min<uint64_t>(size, 22 + 65535));
        Bytes buf(tail);
        in_.seekg(static_cast<std::streamoff>(size - tail));
        in_.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(tail));
        size_t e = std::string::npos;
        for (size_t i = tail - 22 + 1; i-- > 0;)
            if (le32(&buf[i]) == 0x06054b50u) { e = i; break; }
        if (e == std::string::npos) throw Error("not a ZIP package (no end record): " + file_);
        uint32_t count = le16(&buf[e + 10]), cdsize = le32(&buf[e + 12]), cdoff = le32(&buf[e + 16]);
        if (count == 0xFFFF || cdsize == 0xFFFFFFFFu || cdoff == 0xFFFFFFFFu) throw Error("ZIP64 packages are not supported");
        if (uint64_t(cdoff) + cdsize > size) throw Error("corrupt ZIP directory");
        Bytes cd(cdsize);
        in_.seekg(cdoff);
        in_.read(reinterpret_cast<char*>(cd.data()), cdsize);
        size_t p = 0;
        for (uint32_t i = 0; i < count; ++i) {
            if (p + 46 > cd.size() || le32(&cd[p]) != 0x02014b50u) throw Error("corrupt ZIP directory entry");
            Entry en;
            uint16_t flags = le16(&cd[p + 8]);
            en.method = le16(&cd[p + 10]);
            en.crc = le32(&cd[p + 16]);
            en.csize = le32(&cd[p + 20]);
            en.usize = le32(&cd[p + 24]);
            uint16_t nl = le16(&cd[p + 28]), el = le16(&cd[p + 30]), cl = le16(&cd[p + 32]);
            en.offset = le32(&cd[p + 42]);
            if (p + 46 + nl > cd.size()) throw Error("corrupt ZIP directory entry");
            std::string name(reinterpret_cast<const char*>(&cd[p + 46]), nl);
            p += 46 + nl + el + cl;
            if (flags & 1) throw Error("encrypted ZIP entries are not supported");
            if (name.empty() || name.back() == '/') continue;
            try { name = normalize_path(name); } catch (const Error&) { throw Error("unsafe path in package: " + name); }
            entries_[name] = en;
        }
    }
    bool exists(const std::string& p) const override { return entries_.count(safe(p)) > 0; }
    Bytes read(const std::string& p) const override {
        auto it = entries_.find(safe(p));
        if (it == entries_.end()) throw Error("missing file in package: " + p);
        const Entry& en = it->second;
        std::lock_guard<std::mutex> lock(m_);
        in_.clear();
        in_.seekg(en.offset);
        uint8_t h[30];
        in_.read(reinterpret_cast<char*>(h), 30);
        if (!in_ || le32(h) != 0x04034b50u) throw Error("corrupt ZIP local header for " + p);
        in_.seekg(en.offset + 30 + le16(h + 26) + le16(h + 28));
        Bytes raw(en.csize);
        in_.read(reinterpret_cast<char*>(raw.data()), en.csize);
        if (!in_ && en.csize) throw Error("truncated ZIP entry " + p);
        Bytes out;
        if (en.method == 0) out = std::move(raw);
        else if (en.method == 8) out = inflate_raw(raw.data(), raw.size(), en.usize, std::max<size_t>(en.usize, 1));
        else throw Error("unsupported ZIP compression method for " + p);
        if (out.size() != en.usize || crc32(out.data(), out.size()) != en.crc) throw Error("ZIP checksum mismatch for " + p);
        return out;
    }
    std::vector<std::string> list() const override {
        std::vector<std::string> out;
        for (auto& kv : entries_) out.push_back(kv.first);
        return out; // std::map => sorted
    }
    std::string describe() const override { return file_; }
    bool is_archive() const override { return true; }
private:
    struct Entry { uint16_t method; uint32_t crc, csize, usize, offset; };
    std::string file_;
    mutable std::ifstream in_;
    mutable std::mutex m_;
    std::map<std::string, Entry> entries_;
    static std::string safe(const std::string& p) { return normalize_path(p); }
};
} // namespace

std::unique_ptr<Vfs> open_directory(const std::string& dir) { return std::make_unique<DirVfs>(dir); }
std::unique_ptr<Vfs> open_zip(const std::string& file) { return std::make_unique<ZipVfs>(file); }

// ------------------------------------------------------------------ writer
ZipWriter::ZipWriter(const std::string& out_file) : out_(out_file, std::ios::binary | std::ios::trunc) {
    if (!out_) throw Error("cannot write package: " + out_file);
}
ZipWriter::~ZipWriter() = default;
void ZipWriter::u16(uint16_t v) { char b[2] = {char(v & 0xFF), char(v >> 8)}; out_.write(b, 2); }
void ZipWriter::u32(uint32_t v) { char b[4] = {char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF), char(v >> 24)}; out_.write(b, 4); }

void ZipWriter::add(const std::string& raw_name, const Bytes& data) {
    std::string name = normalize_path(raw_name);
    if (data.size() > 0xFFFFFFF0u) throw Error("file too large for ZIP: " + name);
    Entry e{name, crc32(data.data(), data.size()), static_cast<uint32_t>(data.size()), static_cast<uint32_t>(out_.tellp())};
    u32(0x04034b50u); u16(20); u16(0x0800 /*UTF-8 names*/); u16(0); u16(0); u16(0x21 /*1980-01-01*/);
    u32(e.crc); u32(e.size); u32(e.size); u16(static_cast<uint16_t>(name.size())); u16(0);
    out_.write(name.data(), static_cast<std::streamsize>(name.size()));
    out_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    entries_.push_back(std::move(e));
}

void ZipWriter::finish() {
    if (finished_) return;
    finished_ = true;
    uint32_t cd_off = static_cast<uint32_t>(out_.tellp());
    for (auto& e : entries_) {
        u32(0x02014b50u); u16(20); u16(20); u16(0x0800); u16(0); u16(0); u16(0x21);
        u32(e.crc); u32(e.size); u32(e.size); u16(static_cast<uint16_t>(e.name.size()));
        u16(0); u16(0); u16(0); u16(0); u32(0); u32(e.offset);
        out_.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
    }
    uint32_t cd_size = static_cast<uint32_t>(out_.tellp()) - cd_off;
    u32(0x06054b50u); u16(0); u16(0); u16(static_cast<uint16_t>(entries_.size())); u16(static_cast<uint16_t>(entries_.size()));
    u32(cd_size); u32(cd_off); u16(0);
    out_.flush();
    if (!out_) throw Error("failed writing package");
}

size_t pack_directory(const std::string& dir, const std::string& out_file) {
    auto vfs = open_directory(dir);
    ZipWriter zip(out_file);
    size_t n = 0;
    for (auto& f : vfs->list()) {
        bool skip = false;
        for (auto& part : split(f, '/'))
            if (!part.empty() && (part[0] == '.' || part == "saves" || part == "build" || part == "__pycache__")) skip = true;
        if (skip) continue;
        zip.add(f, vfs->read(f));
        ++n;
    }
    if (n > 65535) throw Error("too many files for a ZIP package");
    zip.finish();
    return n;
}

} // namespace sf
