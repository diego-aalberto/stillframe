// Small OS abstraction: user data location, time, atomic file writes.
#pragma once
#include "common.hpp"

namespace sf {
std::string user_data_dir();                       // where saves and settings live (never inside a game)
std::string exe_dir();                             // folder of the running program
std::string default_games_dir();                   // <exe dir>/games (or ./games), overridable by STILLFRAME_GAMES
std::string assets_dir();                          // engine assets (fonts, UI sounds): STILLFRAME_ASSETS, <exe>/assets, <exe>/../share/stillframe/assets, build-time source dir
void write_file_atomic(const std::string& path, const std::string& data);   // tmp + rename, keeps previous as .bak
std::string read_file(const std::string& path);    // throws Error
bool file_exists(const std::string& path);
int64_t unix_time();
void sleep_ms(int ms);
} // namespace sf
