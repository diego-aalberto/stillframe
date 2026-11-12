#include "cli.hpp"
#include "../core/common.hpp"
#include <iostream>

static void usage() {
    std::cout <<
        "Stillframe 2.0 - visual novel engine\n\n"
        "  stillframe [run] [game]              open the library, or a game (folder or .stillgame)\n"
        "  stillframe check <game>              validate scripts, characters and assets\n"
        "  stillframe pack <folder> [out]       build a single-file .stillgame\n"
        "  stillframe list [folder]             list installed games\n"
        "  stillframe screenshot <game> ...     render frames headlessly (see docs)\n";
}

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return sf::cmd_run({});
    std::string cmd = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());
    if (cmd == "check") return sf::cmd_check(rest);
    if (cmd == "pack") return sf::cmd_pack(rest);
    if (cmd == "list") return sf::cmd_list(rest);
    if (cmd == "screenshot") return sf::cmd_screenshot(rest);
    if (cmd == "library-shot") return sf::cmd_library_shot(rest);
    if (cmd == "run") return sf::cmd_run(rest);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }
    return sf::cmd_run(args);      // `stillframe path/to/game`
}
