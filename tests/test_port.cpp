// Plays the shipped game "The Light We Leave On" headlessly with a scripted model and checks the story logic
// (progression, relationship gating, repair, endings) that tools/port_legacy_story.py generated.
#include "test.hpp"
#include "../render/player.hpp"
#include "../core/platform.hpp"
#include <filesystem>
#include <functional>

using namespace sf;
namespace fs = std::filesystem;

#ifndef SF_SOURCE_DIR
#define SF_SOURCE_DIR "."
#endif

namespace {

struct Run {
    std::shared_ptr<Game> game;
    std::shared_ptr<ScriptedBackend> be = std::make_shared<ScriptedBackend>();
    std::unique_ptr<Player> p;
    Canvas canvas;
    std::vector<std::string> visited;       // node ids in order
    // pick(node, menu labels or empty, outcome candidates) -> choice
    std::function<std::vector<std::string>(const std::string& node)> outcomes_for;
    std::function<int(const std::vector<std::string>& labels)> menu_pick;

    explicit Run(const char* tag) : game(Game::load(Vfs::open(std::string(SF_SOURCE_DIR) + "/games/the-light-we-leave-on"))), canvas(game->canvas_w / 3, game->canvas_h / 3) {
        PlayerOptions o;
        o.data_dir = (fs::temp_directory_path() / (std::string("sf_port_") + tag)).string();
        fs::remove_all(o.data_dir);
        o.autosave = false; o.seed = 3; o.llm = be;
        p = std::make_unique<Player>(game, o);
    }
    void click() { Input i; i.type = Input::MouseDown; i.x = 5; i.y = 5; p->handle(i); }   // empty corner: advances text
    void key(const std::string& k) { Input i; i.type = Input::Key; i.key = k; p->handle(i); }
    void tick(int n = 1) { for (int i = 0; i < n; ++i) { p->update(1.0 / 60); } }
    std::string node() const { return p->vars()["node"].as_str(); }
    const Json& rel(const char* who) const { return p->vars()["rel"][who]; }

    // One conversational turn: the scripted model answers and offers `outcome`.
    bool turn(const std::string& outcome) {
        size_t before = be->calls().size();
        be->push(std::string(R"({"reply":"Mm.","pose":"neutral","outcome":")") + outcome + R"("})");
        for (int i = 0; i < 1500 && be->calls().size() == before; ++i) {    // type until the box accepts it (lines may still be typing out)
            tick();
            if (i % 8 == 0) click();
            Input t; t.type = Input::Text; t.text = "I would like that."; p->handle(t);
            key("enter");
            sleep_ms(1);
        }
        if (be->calls().size() == before) return false;
        for (int i = 0; i < 600 && p->conversation() && p->conversation()->busy(); ++i) { tick(); sleep_ms(1); }
        for (int i = 0; i < 40; ++i) { tick(); if (i % 5 == 0) click(); sleep_ms(1); }
        return true;
    }

    bool do_talk() {
        std::string n = node();
        visited.push_back(n);
        for (auto& id : outcomes_for(n)) {
            if (!turn(id)) return false;
            bool ok = p->conversation() && p->conversation()->commit(id);
            if (!ok && getenv("SF_DEBUG")) std::cerr << "  commit " << id << " refused in " << n << " rel=" << p->vars()["rel"].dump() << "\n";
            if (ok) {
                tick(2);
                for (int i = 0; i < 6 && p->mode_name() == "talk"; ++i) { click(); click(); tick(2); }
                return true;
            }
        }
        return false;
    }

    // Returns "" on success, or a description of why the run stopped early.
    std::string play(int max_steps = 60000) {
        for (int step = 0; step < max_steps; ++step) {
            std::string m = p->mode_name();
            if (m == "ended") return "";
            if (m == "say") { click(); click(); tick(2); }
            else if (m == "menu") {
                int idx = menu_pick(p->menu_labels());
                if (idx < 0) return "menu pick failed";
                key(std::to_string(idx + 1)); tick(2);
            }
            else if (m == "talk") { if (!do_talk()) return "no outcome accepted in " + node(); }
            else if (m == "error") return "error panel";
            else tick();
        }
        return "step limit";
    }
};

int find(const std::vector<std::string>& v, const std::string& part) {
    for (size_t i = 0; i < v.size(); ++i) if (v[i].find(part) != std::string::npos) return static_cast<int>(i);
    return -1;
}
bool has_text(Run& r, const std::string& s) {
    for (auto& line : r.p->history_text()) if (line.find(s) != std::string::npos) return true;
    return false;
}
}

TEST(port_game_validates_cleanly) {
    auto g = Game::load(Vfs::open(std::string(SF_SOURCE_DIR) + "/games/the-light-we-leave-on"));
    auto issues = g->validate();
    for (auto& i : issues) std::cerr << "  issue: " << i.where << ": " << i.message << "\n";
    CHECK(issues.empty());
    CHECK_EQ(g->characters.size(), size_t(4));
    CHECK(g->characters.at("hana").immutable.size() >= 8);          // identity slot is complete
}

TEST(port_hana_exclusive_route_reaches_the_ending) {
    Run r("hana");
    r.outcomes_for = [](const std::string& n) -> std::vector<std::string> {
        if (n[0] == 'H') return {"romance"};
        if (n == "S01") return {"welcome"};
        if (n == "S03") return {"help"};
        if (n == "S04") return {"consent"};
        if (n == "S05") return {"clear"};
        if (n == "S06") return {"cafe"};
        if (n == "S07") return {"share"};
        if (n == "S08") return {"adapt"};
        if (n == "S09") return {"continue"};
        if (n == "S10") return {"finish"};
        return {"settle"};
    };
    r.menu_pick = [](const std::vector<std::string>& labels) {
        int i = find(labels, "Hana");
        return i >= 0 ? i : static_cast<int>(labels.size()) - 1;          // visit Hana, else let the story move on
    };
    std::string why = r.play();
    CHECK_EQ(why, std::string(""));
    CHECK(r.p->mode_name() == "ended");
    CHECK_EQ(r.rel("hana")["progress"].as_int(), 6);
    CHECK(r.rel("hana")["status"].as_str() == "exclusive");
    CHECK_EQ(r.rel("mira")["progress"].as_int(), 0);                 // never visited
    CHECK(r.p->vars()["flags"]["plan_cafe"].as_bool());
    CHECK(has_text(r, "window bench"));                              // Hana's exclusive ending
    CHECK(has_text(r, "The light is on."));
    // order sanity: shared scenes in sequence, Hana's scenes in her own order
    int last_h = 0, last_s = 0;
    for (auto& n : r.visited) {
        if (n[0] == 'H') { int k = std::stoi(n.substr(1)); CHECK(k == last_h + 1); last_h = k; }
        if (n[0] == 'S') { int k = std::stoi(n.substr(1)); CHECK(k > last_s); last_s = k; }
    }
    CHECK_EQ(last_h, 6);
    CHECK_EQ(last_s, 10);
}

TEST(port_a_hurt_outcome_blocks_romance_until_repaired) {
    Run r("repair");
    bool hurt_done = false, repaired = false;
    r.outcomes_for = [&](const std::string& n) -> std::vector<std::string> {
        if (n == "H01" && !hurt_done) { hurt_done = true; return {"hurt"}; }
        if (n == "REPAIR") { repaired = true; return {"repair"}; }
        if (n[0] == 'H') return {"romance", "connect"};
        if (n == "S01") return {"welcome"};
        if (n == "S10") return {"finish"};
        return {"settle", "visit", "quiet", "uncertain", "leave"};
    };
    int visits = 0;
    r.menu_pick = [&](const std::vector<std::string>& labels) {
        int i = find(labels, "Hana");
        if (i >= 0 && visits < 3) { ++visits; return i; }
        return static_cast<int>(labels.size()) - 1;
    };
    // Stop as soon as the repair visit has happened; no need to finish the story.
    // While she is hurt, `rel.hana.repair` stays true and the hub offers her visit as the repair conversation.
    for (int step = 0; step < 40000 && !(repaired && r.rel("hana")["repair"].as_bool() == false && r.rel("hana")["progress"].as_int() >= 2); ++step) {
        std::string m = r.p->mode_name();
        if (m == "say") { r.click(); r.click(); r.tick(2); }
        else if (m == "menu") { int idx = r.menu_pick(r.p->menu_labels()); r.key(std::to_string(idx + 1)); r.tick(2); }
        else if (m == "talk") { if (!r.do_talk()) { CHECK(false); break; } }
        else if (m == "ended" || m == "error") break;
        else r.tick();
    }
    CHECK(hurt_done);
    CHECK(repaired);
    CHECK(!r.rel("hana")["repair"].as_bool());
    CHECK(std::find(r.visited.begin(), r.visited.end(), "REPAIR") != r.visited.end());
    // the failed first visit still counted: she is on her second scene afterwards
    CHECK(r.rel("hana")["progress"].as_int() >= 2);
}

TEST(port_everyone_can_agree_to_the_shared_relationship) {
    Run r("shared");
    r.outcomes_for = [](const std::string& n) -> std::vector<std::string> {
        if (n == "G01") return {"together"};
        if (n[0] == 'H' || n[0] == 'M' || n[0] == 'L' || n[0] == 'T') {
            if (n[2] == '6') return {"together", "romance"};
            return {"romance"};
        }
        if (n == "S01") return {"flirt"};
        if (n == "S03") return {"help"};
        if (n == "S04") return {"consent"};
        if (n == "S05") return {"clear"};
        if (n == "S06") return {"club"};
        if (n == "S07") return {"share"};
        if (n == "S08") return {"adapt"};
        if (n == "S09") return {"continue"};
        if (n == "S10") return {"finish"};
        return {"settle"};
    };
    // Advance all four together: always visit whoever is furthest behind, so nobody commits exclusively first.
    r.menu_pick = [&r](const std::vector<std::string>& labels) {
        int best = -1; int64_t best_progress = 99;
        for (auto who : {"hana", "mira", "leila", "tessa"}) {
            std::string name = who; name[0] = static_cast<char>(std::toupper(name[0]));
            int i = find(labels, name);
            int64_t pr = r.rel(who)["progress"].as_int();
            if (i >= 0 && pr < best_progress) { best = i; best_progress = pr; }
        }
        return best >= 0 ? best : static_cast<int>(labels.size()) - 1;
    };
    std::string why = r.play();
    if (!why.empty()) std::cerr << "  state at stop: " << r.p->vars()["rel"].dump() << "\n";
    CHECK_EQ(why, std::string(""));
    for (auto who : {"hana", "mira", "leila", "tessa"}) {
        CHECK(r.rel(who)["status"].as_str() == "shared");
        CHECK_EQ(r.rel(who)["progress"].as_int(), 6);
    }
    CHECK(std::find(r.visited.begin(), r.visited.end(), "G01") != r.visited.end());
    CHECK(r.p->vars()["flags"]["everyone_agreed"].as_bool());
    CHECK(has_text(r, "Each person is here because she chose to be"));
}
