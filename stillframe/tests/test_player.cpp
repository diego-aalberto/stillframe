#include "test.hpp"
#include "../render/player.hpp"
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
void put_png(const std::string& path, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    Image im; im.w = w; im.h = h; im.px.resize(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < im.px.size(); i += 4) { im.px[i] = r; im.px[i + 1] = g; im.px[i + 2] = b; im.px[i + 3] = 255; }
    fs::create_directories(fs::path(path).parent_path());
    png_save(path, im);
}
std::string slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }

const char* kScript = R"(
label start:
    scene cafe with fade
    show hana neutral at left with dissolve
    narrator "Rain."
    hana "You came back."
    $ trust += 1
    menu:
        "Sit":
            $ choice = "sit"
            jump chat
        "Stand":
            $ choice = "stand"
            jump chat
label chat:
    talk with hana, mira:
        goal: Hana asks why you came back.
        on_end: leave
        min_turns: 1
        outcome stay "Stay":
            rule: player stays
            $ result = "stay"
            jump after
        outcome leave "Leave":
            rule: player leaves
            $ result = "leave"
            jump after
label after:
    scene street with fade
    hana "Bye."
    minigame dice(difficulty=2) -> r
    if r.outcome == "win":
        $ duel = "win"
    elif r.outcome == "lose":
        $ duel = "lose"
    else:
        $ duel = "draw"
    end "done"
)";

std::string make_game(const char* name) {
    auto root = (fs::temp_directory_path() / (std::string("sf_player_") + name)).string();
    fs::remove_all(root);
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"player.test","title":"PT","game_version":"1","engine":">=2","start":"start",
        "canvas":[640,360],"variables":{"player":"Diego","trust":0},"llm":{"system":"A cafe story.","interject_limit":2}})");
    for (auto id : {"hana", "mira"}) {
        std::string n = id; n[0] = static_cast<char>(std::toupper(n[0]));
        put(root + "/characters/" + id + ".json", std::string(R"({"id":")") + id + R"(","name":")" + n + R"(","color":"#88ccaa","immutable":["Trait of )" + id + R"("],
            "sprites":{"poses":{"neutral":"../sprites/n.png","shy":"../sprites/s.png"},"default_pose":"neutral"}})");
    }
    put_png(root + "/sprites/n.png", 60, 160, 200, 120, 120);
    put_png(root + "/sprites/s.png", 60, 160, 120, 200, 120);
    put_png(root + "/backgrounds/cafe.png", 96, 64, 90, 60, 40);
    put_png(root + "/backgrounds/street.png", 96, 64, 40, 50, 90);
    put(root + "/minigames/dice/minigame.json", R"({"id":"dice","title":"Dice","outcomes":["win","lose","draw"],"launch":{"type":"process","command":["python3","-I","-u","{dir}/main.py"]}})");
    put(root + "/minigames/dice/main.py", slurp(std::string(SF_SOURCE_DIR) + "/tools/sample_minigame.py"));
    put(root + "/scripts/main.sf", kScript);
    return root;
}

struct Rig {
    std::shared_ptr<Game> game;
    std::shared_ptr<ScriptedBackend> be = std::make_shared<ScriptedBackend>();
    std::unique_ptr<Player> p;
    Canvas canvas;
    explicit Rig(const char* name, bool with_llm = true) : game(Game::load(Vfs::open(make_game(name)))), canvas(640, 360) {
        PlayerOptions o;
        o.data_dir = (fs::temp_directory_path() / (std::string("sf_player_data_") + name)).string();
        fs::remove_all(o.data_dir);
        o.autosave = false;
        o.seed = 5;
        if (with_llm) o.llm = be;
        else o.llm_url = "http://127.0.0.1:1";
        p = std::make_unique<Player>(game, o);
    }
    bool until(const std::string& mode, double secs = 10) {
        for (double t = 0; t < secs; t += 1.0 / 60) {
            if (p->mode_name() == mode) return true;
            p->update(1.0 / 60);
            p->render(canvas);
            sleep_ms(2);
        }
        return p->mode_name() == mode;
    }
    void click(float x = 320, float y = 300) { Input i; i.type = Input::MouseDown; i.x = x; i.y = y; p->handle(i); }
    void key(const char* k) { Input i; i.type = Input::Key; i.key = k; p->handle(i); }
    void type(const char* s) { Input i; i.type = Input::Text; i.text = s; p->handle(i); }
    // Complete the typewriter, then advance.
    void next() { click(); click(); }
    void settle(double secs) { for (double t = 0; t < secs; t += 1.0 / 60) { p->update(1.0 / 60); sleep_ms(1); } }
};
}

TEST(player_plays_a_story_through_menu_talk_and_minigame) {
    Rig r("flow");
    CHECK(r.until("say"));                       // narration "Rain." once the fade finished
    CHECK(r.game->validate().empty());
    r.next(); CHECK(r.until("say"));             // hana: You came back.
    r.next(); CHECK(r.until("menu"));
    CHECK_EQ(r.p->vars()["trust"].as_int(), 1);
    r.key("2");                                  // Stand
    CHECK(r.until("talk"));
    CHECK_EQ(r.p->vars()["choice"].as_str(), std::string("stand"));

    r.be->push(R"({"reply":"Why did you stand there?","pose":"shy","memory_ops":[{"op":"add","kind":"fact","text":"Diego stood by the door","evidence":"I stood"}],"outcome":"stay"})");
    r.type("I stood there a while");
    r.key("enter");
    for (int i = 0; i < 400 && !(r.p->conversation() && !r.p->conversation()->busy()); ++i) r.settle(0.02);
    r.settle(0.5);
    // min_turns is 1, so she may already offer the ending; the player confirms with the offer button
    r.settle(0.3);
    // find the offer button by committing through the conversation (the button calls the same method)
    CHECK(r.p->conversation()->commit("stay"));
    r.p->update(1.0 / 60); r.p->update(1.0 / 60);
    // the final line must still be read before the talk closes
    for (int i = 0; i < 5 && r.p->mode_name() == "talk"; ++i) r.next();
    CHECK(r.until("say"));                       // hana: Bye. (after the scene change)
    CHECK_EQ(r.p->vars()["result"].as_str(), std::string("stay"));
    // memory: she kept the fact, and the scene change flushed her scene slot
    CHECK_EQ(r.p->memory().of("hana").contextual().size(), size_t(1));
    CHECK(r.p->memory().of("hana").scene().size() <= 1);
    r.next();
    CHECK(r.until("minigame", 5) || r.until("ended", 20));
    CHECK(r.until("ended", 20));                 // the sample minigame finishes by itself
    std::string duel = r.p->vars()["duel"].as_str();
    CHECK(duel == "win" || duel == "lose" || duel == "draw");
    CHECK_EQ(r.p->vars()["r"]["data"]["difficulty"].as_int(), 2);
}

TEST(player_witness_feeds_scene_memory_of_everyone_on_stage) {
    Rig r("witness");
    CHECK(r.until("say"));
    r.next(); CHECK(r.until("say"));
    // Hana is on stage, Mira is not: Hana heard the narration and her own line, Mira nothing
    bool hana_heard = false;
    for (auto& l : r.p->memory().of("hana").scene()) if (l.text.find("You came back") != std::string::npos) hana_heard = true;
    CHECK(hana_heard);
    CHECK_EQ(r.p->memory().of("mira").scene().size(), size_t(0));
    CHECK(!r.p->history_text().empty());
}

TEST(player_save_and_load_restore_position_and_state) {
    Rig r("save");
    CHECK(r.until("say"));
    r.next(); CHECK(r.until("say"));
    r.next(); CHECK(r.until("menu"));
    r.click(0, 0);                               // harmless click in a corner while a menu is up
    CHECK(r.p->save_slot("1"));
    r.key("1");
    CHECK(r.until("talk"));
    CHECK_EQ(r.p->vars()["choice"].as_str(), std::string("sit"));
    CHECK(r.p->load_slot("1"));
    CHECK(r.until("menu"));
    CHECK(r.p->vars()["choice"].is_null());      // variables went back too
    CHECK(!r.p->load_slot("nope"));              // a missing slot is reported, not fatal
    CHECK_EQ(r.p->mode_name(), std::string("menu"));
}

TEST(player_talk_without_a_model_offers_retry_or_skip) {
    Rig r("nomodel", false);
    CHECK(r.until("say"));
    r.next(); CHECK(r.until("say"));
    r.next(); CHECK(r.until("menu"));
    r.key("1");
    CHECK(r.until("error", 10));
    auto msgs = r.p->take_messages();
    CHECK(!msgs.empty());
    r.p->render(r.canvas);                       // the error panel draws
    // Skip: on_end outcome ("leave") is used and the story goes on
    for (int i = 0; i < 60 && r.p->mode_name() == "error"; ++i) { r.click(425, 217); r.settle(0.05); }   // the Skip button
    CHECK(r.until("say", 10));
    CHECK_EQ(r.p->vars()["result"].as_str(), std::string("leave"));
}

TEST(player_overlays_and_text_input_do_not_crash_and_edit_correctly) {
    Rig r("overlay");
    CHECK(r.until("say"));
    r.key("escape"); r.p->render(r.canvas);
    CHECK_EQ(r.p->mode_name(), std::string("menu_overlay"));
    r.key("escape");
    CHECK_EQ(r.p->mode_name(), std::string("say"));
    r.key("h"); r.p->render(r.canvas);
    CHECK_EQ(r.p->mode_name(), std::string("history"));
    r.key("escape");
    TextInput t;
    t.insert("h\xC3\xA9llo");                    // héllo
    t.left(); t.left();
    t.backspace();                               // removes the first l, not half a character
    CHECK_EQ(t.text, std::string("h\xC3\xA9lo"));
    t.home(); t.del(); CHECK_EQ(t.text, std::string("\xC3\xA9lo"));
    t.end(); t.insert("\n!"); CHECK_EQ(t.text, std::string("\xC3\xA9lo !"));
}

TEST(player_frames_are_not_blank) {
    Rig r("frames");
    CHECK(r.until("say"));
    r.settle(1.0);                                // let the opening fade finish (the box is hidden during transitions)
    r.p->render(r.canvas);
    // the background colour (90,60,40) is visible at the top-left; the dialogue box darkens the bottom
    const auto& px = r.canvas.pixels();
    CHECK(std::abs(int(px[(5 * 640 + 5) * 4]) - 90) < 6);
    CHECK(px[(340 * 640 + 320) * 4] < 60);
}
