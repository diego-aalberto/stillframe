// The library screen: every installed game as a card. Lives outside any game, so it uses the engine theme.
#pragma once
#include "player.hpp"

namespace sf {

class Launcher {
public:
    Launcher(std::string games_dir, int w = 1280, int h = 720);
    void refresh();                              // rescan the folder
    void handle(const Input& in);
    void render(Canvas& out);
    void update(double dt) { clock_ += dt; }
    // Set when the player picked a game; the caller opens it and calls clear_choice() when done.
    const std::string& chosen() const { return chosen_; }
    void clear_choice() { chosen_.clear(); }
    bool quit_requested() const { return quit_; }
    const std::vector<GameEntry>& games() const { return games_; }
    const std::string& games_dir() const { return dir_; }
    void set_notice(const std::string& s) { notice_ = s; }   // e.g. why the last game failed to open

private:
    struct Card { Rect r; size_t index; };
    std::vector<Card> cards() const;
    std::shared_ptr<const Image> cover(const GameEntry& g);

    std::string dir_, chosen_, notice_;
    int w_, h_;
    Theme theme_;
    std::vector<GameEntry> games_;
    std::map<std::string, std::shared_ptr<const Image>> covers_;
    float mx_ = -1, my_ = -1;
    double clock_ = 0, scroll_ = 0;
    bool quit_ = false;
};

} // namespace sf
