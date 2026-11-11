#include "cli.hpp"
#include "../core/game.hpp"
#include "../core/platform.hpp"
#include "../core/vfs.hpp"
#include "../render/app.hpp"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace sf {
namespace fs = std::filesystem;

static std::shared_ptr<Game> open_game(const std::string& path) {
    return Game::load(Vfs::open(path));
}

int cmd_check(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "usage: stillframe check <game folder or .stillgame>\n"; return 2; }
    try {
        auto g = open_game(args[0]);
        auto issues = g->validate();
        int errors = 0, warnings = 0;
        for (auto& i : issues) { std::cout << i.str() << "\n"; (i.level == Issue::Error ? errors : warnings)++; }
        std::cout << g->title << " " << g->version << ": " << errors << " error(s), " << warnings << " warning(s)\n";
        return errors ? 1 : 0;
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int cmd_pack(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "usage: stillframe pack <game folder> [output.stillgame]\n"; return 2; }
    try {
        auto g = open_game(args[0]);
        int errors = 0;
        for (auto& i : g->validate()) if (i.level == Issue::Error) { std::cerr << i.str() << "\n"; ++errors; }
        if (errors) { std::cerr << "not packing: fix the " << errors << " error(s) above first\n"; return 1; }
        std::string out = args.size() > 1 ? args[1] : (fs::path(args[0]).parent_path() / (g->id + "-" + g->version + ".stillgame")).string();
        size_t n = pack_directory(args[0], out);
        std::cout << "wrote " << out << " (" << n << " files)\n";
        return 0;
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int cmd_list(const std::vector<std::string>& args) {
    std::string dir = args.empty() ? default_games_dir() : args[0];
    auto games = scan_games(dir);
    if (games.empty()) { std::cout << "no games found in " << dir << "\n"; return 0; }
    for (auto& g : games) std::cout << g.title << "  " << g.version << (g.author.empty() ? "" : "  by " + g.author) << "\n    " << g.path << "\n";
    return 0;
}

// ---------------------------------------------------------------------------------------- screenshot

namespace {
std::vector<std::string> split_steps(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) { if (c == ';') { out.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
}

// stillframe screenshot <game> --steps "wait:1;shot:a.png;click;wait:2;shot:b.png" [--size 1280x720] [--fake-llm file.txt]
// Steps: wait:S  click  click:x,y  move:x,y  key:name  text:string  shot:file.png  save:slot  load:slot  state
int cmd_screenshot(const std::vector<std::string>& args) {
    if (args.empty()) { std::cerr << "usage: stillframe screenshot <game> --steps \"...\" [--data DIR] [--fake-llm FILE]\n"; return 2; }
    std::string steps, data = (fs::temp_directory_path() / "stillframe-shot-data").string(), fake;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (args[i] == "--steps") steps = args[i + 1];
        else if (args[i] == "--data") data = args[i + 1];
        else if (args[i] == "--fake-llm") fake = args[i + 1];
    }
    try {
        auto g = open_game(args[0]);
        PlayerOptions po;
        po.data_dir = data;
        po.autosave = false;
        po.seed = 1;
        std::shared_ptr<ScriptedBackend> sb;
        if (!fake.empty()) {            // one canned model answer per line (JSON), in order
            sb = std::make_shared<ScriptedBackend>();
            std::istringstream in(read_file(fake));
            std::string line;
            while (std::getline(in, line)) if (!trim(line).empty()) sb->push(line);
            sb->set_delay_ms(4);
            po.llm = sb;
        }
        Player p(g, po);
        Canvas canvas(g->canvas_w, g->canvas_h);
        auto tick = [&](double secs) {
            for (double t = 0; t < secs; t += 1.0 / 60) { p.update(1.0 / 60); sleep_ms(16); }
        };
        for (auto& st : split_steps(steps)) {
            auto colon = st.find(':');
            std::string name = st.substr(0, colon), arg = colon == std::string::npos ? "" : st.substr(colon + 1);
            Input in;
            if (name == "wait") tick(std::atof(arg.c_str()));
            else if (name == "click") {
                in.type = Input::MouseDown; in.x = g->canvas_w * 0.5f; in.y = g->canvas_h * 0.8f;
                if (!arg.empty()) { in.x = static_cast<float>(std::atof(arg.c_str())); auto c2 = arg.find(','); in.y = static_cast<float>(std::atof(arg.c_str() + c2 + 1)); }
                p.handle(in); tick(0.02);
            }
            else if (name == "move") { in.type = Input::MouseMove; in.x = static_cast<float>(std::atof(arg.c_str())); in.y = static_cast<float>(std::atof(arg.c_str() + arg.find(',') + 1)); p.handle(in); }
            else if (name == "key") { in.type = Input::Key; in.key = arg; p.handle(in); tick(0.02); }
            else if (name == "text") { in.type = Input::Text; in.text = arg; p.handle(in); }
            else if (name == "save") p.save_slot(arg);
            else if (name == "load") p.load_slot(arg);
            else if (name == "state") std::cout << "mode=" << p.mode_name() << "\n";
            else if (name == "shot") {
                p.render(canvas);
                png_save(arg, canvas.to_image());
                std::cout << "wrote " << arg << " (mode " << p.mode_name() << ")\n";
            }
            else { std::cerr << "unknown step '" << name << "'\n"; return 2; }
        }
        for (auto& m : p.take_messages()) std::cerr << "note: " << m << "\n";
        return 0;
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// stillframe library-shot <games dir> <out.png> [hover x,y]
int cmd_library_shot(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "usage: stillframe library-shot <games dir> <out.png> [x,y]\n"; return 2; }
    try {
        AppOptions o;
        o.games_dir = args[0];
        App app(o);
        Canvas c(o.width, o.height);
        if (args.size() > 2) {
            Input in; in.type = Input::MouseMove;
            in.x = static_cast<float>(std::atof(args[2].c_str()));
            in.y = static_cast<float>(std::atof(args[2].c_str() + args[2].find(',') + 1));
            app.handle(in);
        }
        app.render(c);
        png_save(args[1], c.to_image());
        std::cout << "wrote " << args[1] << " (" << app.launcher().games().size() << " games)\n";
        return 0;
    } catch (const Error& e) { std::cerr << "error: " << e.what() << "\n"; return 1; }
}

#ifndef SF_WITH_SDL
bool window_support_available() { return false; }
int cmd_run(const std::vector<std::string>&) {
    std::cerr << "This build of Stillframe has no window support (SDL3 was not found when it was built).\n"
                 "Install SDL3 and SDL3_mixer development files and rebuild, or use `stillframe check|pack|list|screenshot`.\n";
    return 3;
}
#endif

} // namespace sf
