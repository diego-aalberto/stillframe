#include "common.hpp"
#include <algorithm>

namespace sf {

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return std::string(s.substr(a, b - a));
}
bool starts_with(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(0, p.size()) == p; }
bool ends_with(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(s.size() - p.size()) == p; }

std::vector<std::string> split(std::string_view s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i)
        if (i == s.size() || s[i] == sep) { out.emplace_back(s.substr(start, i - start)); start = i + 1; }
    return out;
}

std::string lower(std::string_view s) {
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; });
    return r;
}

std::string normalize_path(std::string_view p) {
    std::string tmp(p);
    std::replace(tmp.begin(), tmp.end(), '\\', '/');
    std::vector<std::string> out;
    for (auto& part : split(tmp, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (out.empty()) throw Error("path escapes the package root: " + std::string(p));
            out.pop_back();
        } else out.push_back(part);
    }
    std::string r;
    for (size_t i = 0; i < out.size(); ++i) { if (i) r += '/'; r += out[i]; }
    return r;
}

std::string dir_of(std::string_view path) {
    auto i = path.rfind('/');
    return i == std::string_view::npos ? std::string() : std::string(path.substr(0, i));
}

std::string join_path(std::string_view base_dir, std::string_view rel) {
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return normalize_path(rel); // package-absolute
    std::string s(base_dir);
    if (!s.empty()) s += '/';
    s += rel;
    return normalize_path(s);
}

} // namespace sf
