// Shared basics for the Stillframe core: error type and small string helpers.
#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace sf {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

using Bytes = std::vector<uint8_t>;

inline std::string to_string(const Bytes& b) { return std::string(b.begin(), b.end()); }
inline Bytes to_bytes(std::string_view s) { return Bytes(s.begin(), s.end()); }

std::string trim(std::string_view s);
bool starts_with(std::string_view s, std::string_view p);
bool ends_with(std::string_view s, std::string_view p);
std::vector<std::string> split(std::string_view s, char sep);
std::string lower(std::string_view s);
// Collapses "a/./b//c", resolves "..", rejects escaping the root. Always '/' separated.
std::string normalize_path(std::string_view p);
std::string join_path(std::string_view base_dir, std::string_view rel);
std::string dir_of(std::string_view path);

} // namespace sf
