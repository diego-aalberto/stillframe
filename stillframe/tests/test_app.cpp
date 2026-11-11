#include "test.hpp"
#include "../render/app.hpp"
#include "../core/platform.hpp"
#include <filesystem>
#include <fstream>

using namespace sf;
namespace fs = std::filesystem;

namespace {
void put(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}
void make_tiny(const std::string& root, const char* id, const char* title, const char* canvas = "[640,360]") {
    put(root + "/game.json", std::string(R"({"format":"stillgame","version":1,"id":")") + id + R"(","title":")" + title +
        R"(","game_version":"1","engine":">=2","start":"start","canvas":)" + canvas + "}");
    put(root + "/scripts/main.sf", "label start:\n    narrator \"Hello.\"\n    end \"fin\"\n");
}
Input click(float x, float y) { Input i; i.type = Input::MouseDown; i.x = x; i.y = y; return i; }
Input key(const char* k) { Input i; i.type = Input::Key; i.key = k; return i; }
}

TEST(app_library_lists_and_opens_a_game) {
    auto lib = (fs::temp_directory_path() / "sf_app_lib").string();
    fs::remove_all(lib);
    make_tiny(lib + "/alpha", "alpha", "Alpha");
    AppOptions o; o.games_dir = lib; o.data_dir = lib + "_data";
    App app(o);
    CHECK_EQ(app.launcher().games().size(), size_t(1));
    CHECK(!app.in_game());
    CHECK_EQ(app.canvas_size().first, 1280);
    app.handle(click(300, 330));             // first card
    CHECK(app.in_game());
    CHECK_EQ(app.canvas_size().first, 640);  // the game's own canvas size
    CHECK_EQ(app.canvas_size().second, 360);
    Canvas c(640, 360);
    for (int i = 0; i < 30; ++i) { app.update(1.0 / 60); app.render(c); }
}

TEST(app_returns_to_library_when_game_quits) {
    auto lib = (fs::temp_directory_path() / "sf_app_lib2").string();
    fs::remove_all(lib);
    make_tiny(lib + "/alpha", "alpha", "Alpha");
    AppOptions o; o.games_dir = lib; o.data_dir = lib + "_data";
    App app(o);
    app.handle(click(300, 330));
    CHECK(app.in_game());
    Canvas c(1280, 720);
    for (int i = 0; i < 60; ++i) { app.update(1.0 / 60); app.render(c); }
    CHECK(app.player()->mode_name() == "say");
    app.handle(key("escape"));               // opens the in-game menu
    CHECK(app.player()->mode_name() == "menu_overlay");
    app.handle(key("escape"));               // and closes it again
    CHECK(app.player()->mode_name() == "say");
    CHECK(app.in_game());
    CHECK(!app.quit_requested());
    app.close_game();                        // what "Quit to library" does
    CHECK(!app.in_game());
    CHECK_EQ(app.canvas_size().first, 1280);
    app.render(c);
}

TEST(app_open_flag_skips_library_and_quits_with_game) {
    auto lib = (fs::temp_directory_path() / "sf_app_lib3").string();
    fs::remove_all(lib);
    make_tiny(lib + "/alpha", "alpha", "Alpha");
    AppOptions o; o.games_dir = lib; o.data_dir = lib + "_data"; o.open = lib + "/alpha";
    App app(o);
    CHECK(app.in_game());
}

TEST(app_broken_game_shows_notice_instead_of_crashing) {
    auto lib = (fs::temp_directory_path() / "sf_app_lib4").string();
    fs::remove_all(lib);
    make_tiny(lib + "/good", "good", "Good");
    make_tiny(lib + "/bad", "bad", "Bad", R"({"w":1})");
    AppOptions o; o.games_dir = lib; o.data_dir = lib + "_data";
    App app(o);
    CHECK(!app.open_game(lib + "/bad"));
    CHECK(!app.in_game());
    CHECK(app.open_game(lib + "/good"));
    CHECK(app.in_game());
}

TEST(app_library_empty_folder_renders) {
    auto lib = (fs::temp_directory_path() / "sf_app_lib5").string();
    fs::remove_all(lib);
    fs::create_directories(lib);
    AppOptions o; o.games_dir = lib; o.data_dir = lib + "_data";
    App app(o);
    CHECK_EQ(app.launcher().games().size(), size_t(0));
    Canvas c(1280, 720);
    app.render(c);
    app.handle(click(500, 400));
    CHECK(!app.in_game());
    app.handle(key("escape"));
    CHECK(app.quit_requested());
}
