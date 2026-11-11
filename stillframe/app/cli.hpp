// Command-line tools shared by every build of the `stillframe` program.
#pragma once
#include <string>
#include <vector>

namespace sf {
int cmd_check(const std::vector<std::string>& args);
int cmd_pack(const std::vector<std::string>& args);
int cmd_list(const std::vector<std::string>& args);
int cmd_screenshot(const std::vector<std::string>& args);
int cmd_run(const std::vector<std::string>& args);       // windowed; needs the SDL build
int cmd_library_shot(const std::vector<std::string>& args);   // render the library screen to a PNG
bool window_support_available();
} // namespace sf
