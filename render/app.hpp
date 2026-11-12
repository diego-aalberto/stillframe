// The whole program minus the operating system: library screen <-> running game.
// A window backend (SDL) only has to feed Input, call update/render, and show the Canvas.
#pragma once
#include "launcher.hpp"
#include "player.hpp"

namespace sf {

struct AppOptions {
    std::string games_dir;          // empty = default_games_dir()
    std::string data_dir;           // empty = user_data_dir()
    std::string open;               // a game path to start in directly (skips the library)
    std::string player_name;
    AudioSink* audio = nullptr;
    int width = 1280, height = 720; // library size; a game uses its own canvas size
};

class App {
public:
    explicit App(AppOptions o);
    void handle(const Input& in);               // coordinates are in the current canvas space (see canvas_size)
    void update(double dt);
    void render(Canvas& out);
    bool needs_frame() const;
    bool quit_requested() const { return quit_; }
    bool in_game() const { return player_ != nullptr; }
    std::pair<int, int> canvas_size() const;    // what the window should present right now
    Launcher& launcher() { return launcher_; }
    Player* player() { return player_.get(); }
    bool open_game(const std::string& path);    // false (and a notice on the library) when it can't be opened
    void close_game();

private:
    AppOptions o_;
    Launcher launcher_;
    std::unique_ptr<Player> player_;
    bool quit_ = false;
};

} // namespace sf
