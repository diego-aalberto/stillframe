#include "test.hpp"
#include "../core/game.hpp"
#include "../core/save.hpp"
#include "../core/platform.hpp"
#include <filesystem>
#include <fstream>

using namespace sf;
namespace fs = std::filesystem;

static void put(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}

static std::string make_game(const char* name, const std::string& script = "") {
    auto root = (fs::temp_directory_path() / (std::string("sf_game_") + name)).string();
    fs::remove_all(root);
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"test.game","title":"Test","game_version":"1.2.0","engine":">=2","start":"start","variables":{"player":"You"}})");
    put(root + "/characters/hana.json", R"({"id":"hana","name":"Hana","color":"#67b3aa",
        "immutable":[{"label":"identity","text":"Hana, 24, café shift lead."},"Names broken appliances."],
        "sprites":{"poses":{"neutral":"../sprites/hana.png","shy":"../sprites/hana_shy.png"},"default_pose":"neutral","scale":0.9}})");
    put(root + "/sprites/hana.png", "png"); put(root + "/sprites/hana_shy.png", "png");
    put(root + "/backgrounds/cafe.png", "png");
    put(root + "/audio/theme.ogg", "ogg");
    put(root + "/scripts/main.sf", script.empty() ? R"(
label start:
    scene cafe with fade
    show hana shy at left
    hana "Hi."
    play music theme
    end
)" : script);
    return root;
}

TEST(game_loads_and_validates) {
    auto g = Game::load(Vfs::open(make_game("ok")));
    CHECK_EQ(g->id, std::string("test.game"));
    CHECK_EQ(g->version, std::string("1.2.0"));
    CHECK_EQ(g->characters.size(), size_t(1));
    auto& h = *g->character("hana");
    CHECK_EQ(h.immutable.size(), size_t(2));
    CHECK_EQ(h.immutable[0].first, std::string("identity"));
    CHECK_EQ(h.poses.at("shy"), std::string("sprites/hana_shy.png"));
    CHECK_NEAR(h.scale, 0.9, 1e-9);
    CHECK(g->backgrounds.count("cafe"));
    CHECK(g->audio.count("theme"));
    CHECK_NEAR(g->position("left"), 0.22, 1e-9);
    CHECK_NEAR(g->position("0.4"), 0.4, 1e-9);
    CHECK(g->position("nowhere") < 0);
    for (auto& i : g->validate()) std::cerr << "  unexpected: " << i.str() << "\n";
    CHECK(g->validate().empty());
}

TEST(game_validation_catches_missing_things) {
    auto g = Game::load(Vfs::open(make_game("bad", R"(
label start:
    scene nowhere with fade
    show ghost happy at middle
    hana angry "Grr."
    mira "Hello."
    play music missing
    minigame cooking() -> r
    talk with hana, sam:
        goal: x
        outcome a "A":
            rule: r
            end
    end
)")));
    std::string all;
    for (auto& i : g->validate()) all += i.str() + "\n";
    for (const char* needle : {"background 'nowhere'", "character 'ghost'", "no pose 'angry'", "speaker 'mira'", "music 'missing'", "unknown stage position 'middle'", "minigame 'cooking'", "unknown character 'sam'"})
        if (all.find(needle) == std::string::npos) { ++sft::failures(); std::cerr << "  FAIL missing diagnostic: " << needle << "\n" << all; }
}

TEST(game_manifest_errors) {
    auto root = make_game("badmanifest");
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"x","engine":">=9"})");
    CHECK_THROWS(Game::load(Vfs::open(root)));
    put(root + "/game.json", R"({"format":"other","version":1,"id":"x"})");
    CHECK_THROWS(Game::load(Vfs::open(root)));
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"bad id!"})");
    CHECK_THROWS(Game::load(Vfs::open(root)));
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"x","start":"nope"})");
    CHECK_THROWS(Game::load(Vfs::open(root)));
    auto r2 = make_game("badchar");
    put(r2 + "/characters/hana.json", R"({"id":"someone_else"})");
    CHECK_THROWS(Game::load(Vfs::open(r2)));
}

TEST(game_loads_from_stillgame_file_and_is_listed) {
    auto root = make_game("pack");
    auto lib = (fs::temp_directory_path() / "sf_game_library").string();
    fs::remove_all(lib);
    fs::create_directories(lib);
    pack_directory(root, lib + "/test.stillgame");
    fs::create_directories(lib + "/not-a-game");
    put(lib + "/junk.stillgame", "garbage");
    auto games = scan_games(lib);
    CHECK_EQ(games.size(), size_t(1));
    CHECK_EQ(games[0].id, std::string("test.game"));
    auto g = Game::load(Vfs::open(games[0].path));
    CHECK(g->vfs->is_archive());
    CHECK(g->validate().empty());
}

TEST(saves_are_outside_the_game_and_survive_game_updates) {
    auto root = (fs::temp_directory_path() / "sf_saves").string();
    fs::remove_all(root);
    Json state = Json::object(); state.set("hp", 3);
    {
        SaveStore s(root, "test.game", 1, "1.0.0");
        s.write("1", state, "Chapter one");
        s.write("auto", state);
        CHECK(s.exists("1"));
        CHECK_EQ(s.read("1")["hp"].as_int(), 3);
        CHECK_EQ(s.list().size(), size_t(2));
    }
    SaveStore updated(root, "test.game", 1, "1.1.0");     // new game version, same save format
    CHECK_EQ(updated.read("1")["hp"].as_int(), 3);
    SaveStore breaking(root, "test.game", 2, "2.0.0");     // author bumped save_format
    CHECK_THROWS(breaking.read("1"));
    SaveStore other(root, "other.game", 1, "1.0.0");        // different game never sees it
    CHECK(!other.exists("1"));
    CHECK_THROWS(updated.write("../escape", state));
    CHECK_THROWS(updated.write("0", state));
}

TEST(saves_recover_from_corruption_using_backup) {
    auto root = (fs::temp_directory_path() / "sf_saves2").string();
    fs::remove_all(root);
    SaveStore s(root, "g", 1, "1");
    Json a = Json::object(); a.set("v", 1);
    Json b = Json::object(); b.set("v", 2);
    s.write("1", a);
    s.write("1", b);     // previous copy kept as .bak
    CHECK_EQ(s.read("1")["v"].as_int(), 2);
    { std::ofstream f(s.dir() + "/slot-1.json", std::ios::trunc); f << "{ broken"; }
    CHECK_EQ(s.read("1")["v"].as_int(), 1);
    s.remove("1");
    CHECK_THROWS(s.read("1"));
}
