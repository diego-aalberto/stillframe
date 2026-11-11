#include "app.hpp"
#include "../core/platform.hpp"
#include "../core/vfs.hpp"

namespace sf {

App::App(AppOptions o)
    : o_((o.games_dir.empty() ? o.games_dir = default_games_dir() : o.games_dir, o)), launcher_(o_.games_dir, o_.width, o_.height) {
    if (!o_.open.empty()) open_game(o_.open);
}

bool App::open_game(const std::string& path) {
    try {
        auto g = Game::load(Vfs::open(path));
        PlayerOptions po;
        po.data_dir = o_.data_dir;
        po.audio = o_.audio;
        po.player_name = o_.player_name;
        player_ = std::make_unique<Player>(g, po);
        launcher_.set_notice("");
        return true;
    } catch (const Error& e) {
        std::string m = e.what();
        if (m.size() > 110) m = m.substr(0, 107) + "...";
        launcher_.set_notice("Couldn't open that game: " + m);
        player_.reset();
        return false;
    }
}

void App::close_game() {
    player_.reset();
    launcher_.refresh();
}

std::pair<int, int> App::canvas_size() const {
    if (player_) return {player_->canvas_w(), player_->canvas_h()};
    return {o_.width, o_.height};
}

void App::handle(const Input& in) {
    if (player_) {
        player_->handle(in);
        if (player_->quit_requested()) {
            // With a game given on the command line there is no library to return to.
            if (o_.open.empty()) close_game(); else quit_ = true;
        }
        return;
    }
    launcher_.handle(in);
    if (launcher_.quit_requested()) quit_ = true;
    if (!launcher_.chosen().empty()) {
        std::string p = launcher_.chosen();
        launcher_.clear_choice();
        open_game(p);
    }
}

void App::update(double dt) {
    if (player_) player_->update(dt); else launcher_.update(dt);
}

void App::render(Canvas& out) {
    if (player_) player_->render(out); else launcher_.render(out);
}

bool App::needs_frame() const { return player_ ? player_->needs_frame() : true; }

} // namespace sf
