#include "test.hpp"
#include "../core/minigame.hpp"
#include "../core/platform.hpp"
#include <filesystem>
#include <fstream>

using namespace sf;
namespace fs = std::filesystem;

#ifndef SF_SOURCE_DIR
#define SF_SOURCE_DIR "."
#endif

namespace {
void put(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}
std::string slurp(const std::string& path) { std::ifstream f(path, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }

std::string make_game_dir(const char* name) {
    auto root = (fs::temp_directory_path() / (std::string("sf_mg_") + name)).string();
    fs::remove_all(root);
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"mg.test","title":"MG","game_version":"3","engine":">=2","start":"start"})");
    put(root + "/characters/hana.json", R"({"id":"hana","name":"Hana","immutable":["Hana, a café shift lead."],"sprites":{"poses":{"neutral":"../s.png"}}})");
    put(root + "/s.png", "png");
    put(root + "/minigames/dice/minigame.json", R"({"id":"dice","title":"Dice duel","outcomes":["win","lose","draw"],"fallback_outcome":"lose",
        "launch":{"type":"process","command":["python3","-I","-u","{dir}/main.py"],"startup_timeout_s":1}, "timeout_s": 0})");
    put(root + "/minigames/dice/main.py", slurp(std::string(SF_SOURCE_DIR) + "/tools/sample_minigame.py"));
    put(root + "/scripts/main.sf", "label start:\n    minigame dice(difficulty=2) -> r\n    end\n");
    return root;
}

bool pump(MinigameSession& s, std::vector<MgEvent>& log, const std::string& until, int ms = 8000) {
    for (int t = 0; t < ms; t += 10) {
        s.poll(log);
        for (auto& e : log) if (e.type == until) return true;
        sleep_ms(10);
    }
    return false;
}
const MgEvent* find(const std::vector<MgEvent>& l, const std::string& type) { for (auto& e : l) if (e.type == type) return &e; return nullptr; }

Json args_of(const char* mode, const char* extra = "") {
    return Json::parse(std::string("{\"mode\":\"") + mode + "\",\"seed\":5,\"difficulty\":2" + extra + "}");
}
Json ctx() { return Json::parse(R"({"player":"Diego"})"); }
}

TEST(minigame_full_round_trip_with_character_line) {
    auto g = Game::load(Vfs::open(make_game_dir("ok")));
    for (auto& i : g->validate()) std::cerr << "  issue: " << i.str() << "\n";
    CHECK(g->validate().empty());
    auto cache = (fs::temp_directory_path() / "sf_mg_cache").string();
    auto s = MinigameSession::launch(*g, "dice", args_of("normal", ",\"ask_character\":\"hana\""), ctx(), cache);
    std::vector<MgEvent> log;
    CHECK(pump(*s, log, "say"));
    auto* say = find(log, "say");
    CHECK_EQ(say->data["character"].as_str(), std::string("hana"));
    CHECK_EQ(say->data["id"].as_int(), 7);
    s->answer_say(7, "You've got this!");
    CHECK(pump(*s, log, "result"));
    CHECK(find(log, "ready") != nullptr);
    CHECK(find(log, "progress") != nullptr);
    CHECK_EQ(find(log, "event")->data["data"]["text"].as_str(), std::string("You've got this!"));
    auto* res = find(log, "result");
    std::string oc = res->data["outcome"].as_str();
    CHECK(oc == "win" || oc == "lose" || oc == "draw");
    CHECK_EQ(res->data["data"]["difficulty"].as_int(), 2);
    CHECK_EQ(res->data["data"]["player"].as_str(), std::string("Diego"));
    CHECK(s->finished());
    Json v = minigame_result_value(oc, res->data["data"]);
    CHECK_EQ(v["outcome"].as_str(), oc);
}

TEST(minigame_same_seed_gives_same_result) {
    auto g = Game::load(Vfs::open(make_game_dir("seed")));
    auto cache = (fs::temp_directory_path() / "sf_mg_cache").string();
    std::string first;
    for (int i = 0; i < 2; ++i) {
        auto s = MinigameSession::launch(*g, "dice", args_of("normal"), ctx(), cache);
        std::vector<MgEvent> log;
        CHECK(pump(*s, log, "result"));
        std::string r = find(log, "result")->data.dump();
        if (i == 0) first = r; else CHECK_EQ(r, first);
    }
}

TEST(minigame_failures_are_reported_not_fatal) {
    auto g = Game::load(Vfs::open(make_game_dir("fail")));
    auto cache = (fs::temp_directory_path() / "sf_mg_cache").string();
    for (auto mode : {"crash", "no_result", "bad_outcome"}) {
        auto s = MinigameSession::launch(*g, "dice", args_of(mode), ctx(), cache);
        std::vector<MgEvent> log;
        CHECK(pump(*s, log, "failed"));
        CHECK(find(log, "result") == nullptr);
        std::string m = find(log, "failed")->data["message"].as_str();
        if (std::string(mode) == "crash") CHECK(m.find("exit code 3") != std::string::npos);
        if (std::string(mode) == "bad_outcome") CHECK(m.find("banana") != std::string::npos);
        CHECK(s->finished());
    }
    {   // never says ready: startup timeout (1 s in the manifest)
        auto s = MinigameSession::launch(*g, "dice", args_of("silent"), ctx(), cache);
        std::vector<MgEvent> log;
        CHECK(pump(*s, log, "failed", 5000));
        CHECK(find(log, "failed")->data["message"].as_str().find("did not start") != std::string::npos);
    }
    CHECK_EQ(MinigameSession::launch(*g, "dice", args_of("crash"), ctx(), cache)->fallback_outcome(), std::string("lose"));
}

TEST(minigame_cancel_ends_the_process) {
    auto g = Game::load(Vfs::open(make_game_dir("cancel")));
    auto cache = (fs::temp_directory_path() / "sf_mg_cache").string();
    auto s = MinigameSession::launch(*g, "dice", args_of("hang"), ctx(), cache);
    std::vector<MgEvent> log;
    CHECK(pump(*s, log, "progress"));
    s->cancel();
    CHECK(s->finished());
    s->poll(log);
    CHECK(find(log, "result") == nullptr);
    CHECK(find(log, "failed") == nullptr);
}

TEST(minigame_missing_launch_program_is_a_readable_error) {
    auto dir = make_game_dir("missing");
    put(dir + "/minigames/dice/minigame.json", R"({"id":"dice","launch":{"type":"process","command":["no-such-program-xyz"]}})");
    auto g = Game::load(Vfs::open(dir));
    auto cache = (fs::temp_directory_path() / "sf_mg_cache").string();
    bool threw = false;
    try { MinigameSession::launch(*g, "dice", Json::object(), Json::object(), cache); }
    catch (const Error& e) { threw = std::string(e.what()).find("no-such-program-xyz") != std::string::npos; }
    CHECK(threw);
    CHECK_THROWS(MinigameSession::launch(*g, "nope", Json::object(), Json::object(), cache));
}

TEST(minigame_runs_from_a_stillgame_archive) {
    auto dir = make_game_dir("zip");
    auto out = (fs::temp_directory_path() / "sf_mg_zip.stillgame").string();
    pack_directory(dir, out);
    auto g = Game::load(Vfs::open(out));
    CHECK(g->vfs->is_archive());
    auto cache = (fs::temp_directory_path() / "sf_mg_cache_zip").string();
    fs::remove_all(cache);
    auto s = MinigameSession::launch(*g, "dice", args_of("normal"), ctx(), cache);
    CHECK(s->dir().find("sf_mg_cache_zip") != std::string::npos);
    CHECK(fs::exists(s->dir() + "/main.py"));
    std::vector<MgEvent> log;
    CHECK(pump(*s, log, "result"));
    // the second launch reuses the extracted files
    auto stamp = fs::last_write_time(s->dir() + "/main.py");
    auto s2 = MinigameSession::launch(*g, "dice", args_of("normal"), ctx(), cache);
    CHECK(stamp == fs::last_write_time(s2->dir() + "/main.py"));
}

TEST(minigame_platform_command_override_and_dir_token) {
    MinigameDef d;
    d.manifest = Json::parse(R"({"launch":{"command":["{dir}/game","--x"],"linux":["{dir}/game.x86_64"],"windows":["{dir}\\game.exe"],"macos":["open","{dir}/g.app"]}})");
    auto c = minigame_command(d, "/tmp/m");
#ifdef _WIN32
    CHECK_EQ(c[0], std::string("/tmp/m\\game.exe"));
#elif defined(__APPLE__)
    CHECK_EQ(c[0], std::string("open"));
#else
    CHECK_EQ(c[0], std::string("/tmp/m/game.x86_64"));
    CHECK_EQ(c.size(), size_t(1));
#endif
}
