#include "platform.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace fs = std::filesystem;

namespace sf {

std::string user_data_dir() {
    if (const char* o = std::getenv("STILLFRAME_DATA")) return o;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA")) return (fs::path(a) / "Stillframe").string();
    return (fs::current_path() / "stillframe-data").string();
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) return (fs::path(h) / "Library/Application Support/Stillframe").string();
    return (fs::current_path() / "stillframe-data").string();
#else
    if (const char* x = std::getenv("XDG_DATA_HOME")) if (*x) return (fs::path(x) / "stillframe").string();
    if (const char* h = std::getenv("HOME")) return (fs::path(h) / ".local/share/stillframe").string();
    return (fs::current_path() / "stillframe-data").string();
#endif
}

std::string exe_dir() {
    std::error_code ec;
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 4);
    if (n > 0 && n < MAX_PATH * 4) return fs::path(std::wstring(buf, n)).parent_path().string();
#elif defined(__APPLE__)
    char buf[4096]; uint32_t sz = sizeof buf;
    if (_NSGetExecutablePath(buf, &sz) == 0) return fs::weakly_canonical(fs::path(buf), ec).parent_path().string();
#else
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.parent_path().string();
#endif
    return fs::current_path().string();
}

std::string default_games_dir() {
    if (const char* o = std::getenv("STILLFRAME_GAMES")) return o;
    std::error_code ec;
    fs::path beside = fs::path(exe_dir()) / "games";
    if (fs::is_directory(beside, ec)) return beside.string();
    return (fs::current_path() / "games").string();
}

std::string assets_dir() {
    std::error_code ec;
    std::vector<fs::path> c;
    if (const char* o = std::getenv("STILLFRAME_ASSETS")) c.push_back(o);
    c.push_back(fs::path(exe_dir()) / "assets");
    c.push_back(fs::path(exe_dir()) / ".." / "share" / "stillframe" / "assets");
#ifdef SF_ASSETS_DIR
    c.push_back(SF_ASSETS_DIR);
#endif
    for (auto& p : c) if (fs::is_regular_file(p / "fonts" / "Poppins-Regular.ttf", ec)) return fs::weakly_canonical(p, ec).string();
    throw Error("cannot find the engine's assets folder (fonts). Set STILLFRAME_ASSETS or reinstall.");
}

bool file_exists(const std::string& path) { std::error_code ec; return fs::is_regular_file(path, ec); }

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("cannot read " + path);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void write_file_atomic(const std::string& path, const std::string& data) {
    fs::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw Error("cannot write " + tmp);
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        f.flush();
        if (!f) throw Error("write failed for " + tmp);
    }
#ifndef _WIN32
    if (int fd = ::open(tmp.c_str(), 0); fd >= 0) { ::fsync(fd); ::close(fd); }
#endif
    if (fs::exists(p, ec)) fs::copy_file(p, path + ".bak", fs::copy_options::overwrite_existing, ec);
    fs::rename(tmp, p, ec);
    if (ec) {   // Windows cannot rename over an existing file in some cases
        fs::remove(p, ec);
        fs::rename(tmp, p, ec);
        if (ec) throw Error("cannot replace " + path + ": " + ec.message());
    }
}

int64_t unix_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

} // namespace sf
