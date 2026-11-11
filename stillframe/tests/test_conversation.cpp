#include "test.hpp"
#include "../core/conversation.hpp"
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

std::shared_ptr<Game> make_game() {
    auto root = (fs::temp_directory_path() / "sf_conv_game").string();
    fs::remove_all(root);
    put(root + "/game.json", R"({"format":"stillgame","version":1,"id":"conv.test","title":"Conv","game_version":"1","engine":">=2","start":"start"})");
    for (auto id : {"hana", "mira", "leila"}) {
        std::string n = id; n[0] = static_cast<char>(std::toupper(n[0]));
        put(root + "/characters/" + id + ".json", std::string(R"({"id":")") + id + R"(","name":")" + n + R"(",
            "immutable":[{"label":"identity","text":")" + n + R"( is a person with a secret trait: )" + id + R"(-only"}],
            "sprites":{"poses":{"neutral":"../s/n.png","shy":"../s/s.png"},"default_pose":"neutral"}})");
    }
    put(root + "/s/n.png", "png"); put(root + "/s/s.png", "png");
    put(root + "/scripts/main.sf", "label start:\n    end\n");
    return Game::load(Vfs::open(root));
}

Json talk_event(int max_turns = 0, const std::string& commit = "ask", bool interject = true, const std::string& on_end = "leave") {
    Json t = Json::object(), cast = Json::array(), outs = Json::array();
    cast.push("hana"); cast.push("mira"); cast.push("leila");
    t.set("cast", cast); t.set("start", "hana"); t.set("goal", "Talk in the cafe."); t.set("commit", commit);
    t.set("max_turns", max_turns); t.set("interject", interject); t.set("on_end", on_end); t.set("min_turns", 2);
    for (auto id : {"welcome", "leave"}) {
        Json o = Json::object(); o.set("id", id); o.set("label", std::string("Label ") + id); o.set("rule", "r");
        outs.push(o);
    }
    t.set("outcomes", outs);
    return t;
}

struct Rig {
    std::shared_ptr<Game> game = make_game();
    MemoryHub hub{*game, MemoryConfig{}};
    std::shared_ptr<ScriptedBackend> be = std::make_shared<ScriptedBackend>();
    LlmScheduler sched{be};
    std::unique_ptr<Conversation> conv;
    std::vector<ConvEvent> log;
    explicit Rig(Json talk = talk_event(), ConvOptions o = {}) {
        o.player_name = "Diego";
        conv = std::make_unique<Conversation>(*game, hub, sched, talk, o);
    }
    // Pump until `pred` holds or 5 s pass.
    template <class F> bool until(F pred) {
        for (int i = 0; i < 500; ++i) {
            conv->poll(log);
            if (pred()) return true;
            sleep_ms(10);
        }
        return false;
    }
    int count(const std::string& type, const std::string& kind = "") const {
        int n = 0;
        for (auto& e : log) if (e.type == type && (kind.empty() || e.data["kind"].as_str() == kind)) ++n;
        return n;
    }
    const ConvEvent* last(const std::string& type) const {
        for (auto it = log.rbegin(); it != log.rend(); ++it) if (it->type == type) return &*it;
        return nullptr;
    }
    bool settle() { return until([&] { return sched.idle() && !conv->busy(); }) && (conv->poll(log), true); }
};
}

TEST(conv_streams_reply_and_applies_memory_ops) {
    Rig r;
    r.be->push(R"({"reply":"That stung, you know.","pose":"shy","memory_ops":[{"op":"add","kind":"grudge","text":"Diego mocked my drawings","evidence":"your drawings are childish","severity":2}]})");
    CHECK(r.conv->player_say("Honestly, your drawings are childish."));
    CHECK(r.conv->busy());
    CHECK(!r.conv->player_say("hello?"));          // she is still answering
    CHECK(r.settle());
    CHECK(r.count("thinking") == 1);
    CHECK(r.count("delta") >= 1);
    auto* rep = r.last("reply");
    CHECK(rep != nullptr);
    CHECK_EQ(rep->data["text"].as_str(), std::string("That stung, you know."));
    CHECK_EQ(rep->data["pose"].as_str(), std::string("shy"));
    CHECK(rep->data["memory"].at(0)["ok"].as_bool());
    CHECK_EQ(r.hub.of("hana").contextual().size(), size_t(1));
    CHECK_EQ(r.hub.of("mira").contextual().size(), size_t(0));   // only she changes her own memory
    CHECK_EQ(r.hub.of("hana").willingness(), std::string("guarded"));
    // everyone present saw the exchange, labelled by display name
    CHECK(r.hub.of("mira").render_scene().find("Diego: Honestly") != std::string::npos);
    CHECK(r.hub.of("mira").render_scene().find("Hana: That stung") != std::string::npos);
}

TEST(conv_rejects_invented_evidence_and_unknown_pose) {
    Rig r;
    r.be->push(R"({"reply":"Okay.","pose":"flying","memory_ops":[{"op":"add","kind":"fact","text":"likes jazz","evidence":"I love jazz"}]})");
    r.conv->player_say("I like tea.");
    CHECK(r.settle());
    auto* rep = r.last("reply");
    CHECK_EQ(rep->data["pose"].as_str(), std::string(""));
    CHECK(!rep->data["memory"].at(0)["ok"].as_bool());
    CHECK_EQ(r.hub.of("hana").contextual().size(), size_t(0));
}

TEST(conv_refusal_skips_model_until_repair_attempt) {
    Rig r;
    r.be->push(R"({"reply":"Don't.","memory_ops":[{"op":"add","kind":"grudge","text":"He called me a liar","evidence":"you're a liar","severity":3,"refuses":true,"refusal_line":"I'm done talking.","repair_hint":"take it back"}]})");
    r.conv->player_say("You're a liar.");
    CHECK(r.settle());
    CHECK_EQ(r.hub.of("hana").willingness(), std::string("refusing"));
    size_t calls = r.be->calls().size();

    CHECK(r.conv->player_say("Come on, talk to me."));
    CHECK(r.settle());
    CHECK_EQ(r.be->calls().size(), calls);                      // no model call
    CHECK_EQ(r.last("reply")->data["text"].as_str(), std::string("I'm done talking."));
    CHECK_EQ(r.last("reply")->data["kind"].as_str(), std::string("refusal"));

    r.be->push(R"({"reply":"...Fine. Thank you.","memory_ops":[{"op":"resolve","id":"m1","evidence":"I'm sorry"}]})");
    CHECK(r.conv->player_say("I'm sorry, I shouldn't have said that."));
    CHECK(r.settle());
    CHECK_EQ(r.be->calls().size(), calls + 1);
    CHECK(r.be->calls().back().req.prompt.find("currently refusing") != std::string::npos);
    CHECK(r.be->calls().back().req.prompt.find("take it back") != std::string::npos);
    CHECK_EQ(r.hub.of("hana").willingness(), std::string("open"));
}

TEST(conv_refusing_character_does_not_interject) {
    Rig r;
    r.hub.of("mira").apply({[] { MemoryOp o; o.op = "add"; o.kind = "grudge"; o.text = "x"; o.evidence = "rude"; o.severity = 3; o.refuses = 1; o.refusal_line = "no"; return o; }()},
                           [] { OpContext c; c.turn = 1; c.player_utterance = "rude"; return c; }());
    CHECK_EQ(r.hub.of("mira").willingness(), std::string("refusing"));
    r.be->push(R"({"reply":"Mira would know.","interject":{"who":"mira","about":"the café"}})");
    r.conv->player_say("Tell me about the café.");
    CHECK(r.settle());
    CHECK_EQ(r.be->calls().size(), size_t(1));
    CHECK_EQ(r.conv->interjections_used(), 0);
}

TEST(conv_interjection_is_chosen_by_the_model_and_rate_limited) {
    ConvOptions o; o.interject_limit = 2; o.interject_cooldown = 1;
    Rig r(talk_event(), o);
    // turn 1: Hana names Mira -> Mira cuts in
    r.be->push(R"({"reply":"Ask Mira about the roof.","interject":{"who":"mira","about":"the roof"}})");
    r.be->push(R"({"speak":true,"reply":"Hey, don't drag me into this!","pose":"shy"})");
    r.conv->player_say("What about the roof?");
    CHECK(r.until([&] { return r.count("reply", "interjection") == 1; }));
    CHECK_EQ(r.last("reply")->data["speaker"].as_str(), std::string("mira"));
    CHECK(r.be->calls()[1].req.prompt.find("the topic involves you") != std::string::npos);
    CHECK(r.be->calls()[1].req.prompt.find("Ask Mira about the roof.") != std::string::npos);   // she heard Hana
    CHECK(r.hub.of("leila").render_scene().find("Mira: Hey") != std::string::npos);
    CHECK_EQ(r.be->calls()[1].req.schema["properties"].find("interject"), nullptr);            // no chains

    // turn 2: cooldown blocks another interjection straight away, even though Hana asks
    r.be->push(R"({"reply":"Leila knows too.","interject":{"who":"leila","about":"x"}})");
    r.conv->player_say("And?");
    CHECK(r.settle());
    CHECK_EQ(r.conv->interjections_used(), 1);
    CHECK_EQ(r.be->calls().size(), size_t(3));

    // turn 3: allowed again; she declines
    r.be->push(R"({"reply":"Leila knows.","interject":{"who":"leila","about":"x"}})");
    r.be->push(R"({"speak":false,"reply":""})");
    r.conv->player_say("Tell me more.");
    CHECK(r.until([&] { return r.count("declined") == 1; }));
    CHECK_EQ(r.conv->interjections_used(), 2);

    // turn 5: budget spent
    r.conv->player_say("ok"); r.be->push(R"({"reply":"x"})"); r.settle();
    r.be->push(R"({"reply":"Mira?","interject":{"who":"mira","about":"y"}})");
    r.conv->player_say("again");
    CHECK(r.settle());
    CHECK_EQ(r.conv->interjections_used(), 2);
}

TEST(conv_ignores_bad_interjection_targets) {
    Rig r;
    r.be->push(R"({"reply":"Myself?","interject":{"who":"hana","about":"me"}})");
    r.conv->player_say("hi");
    CHECK(r.settle());
    r.be->push(R"({"reply":"Nobody.","interject":{"who":"stranger","about":"me"}})");
    r.conv->player_say("hi again");
    CHECK(r.settle());
    CHECK_EQ(r.conv->interjections_used(), 0);
    CHECK_EQ(r.be->calls().size(), size_t(2));
}

TEST(conv_interjection_with_interject_off) {
    Rig r(talk_event(0, "ask", false));
    r.be->push(R"({"reply":"Mira?","interject":{"who":"mira","about":"y"}})");
    r.conv->player_say("hi");
    CHECK(r.settle());
    CHECK_EQ(r.conv->interjections_used(), 0);
    CHECK_EQ(r.be->calls()[0].req.schema["properties"].find("interject"), nullptr);
}

TEST(conv_player_speaking_cancels_pending_interjection) {
    Rig r;
    r.be->set_delay_ms(15);
    r.be->push(R"({"reply":"Ask Mira.","interject":{"who":"mira","about":"z"}})");
    r.be->push(std::string("{\"speak\":true,\"reply\":\"") + std::string(300, 'a') + "\"}");
    r.be->push(R"({"reply":"Okay, moving on."})");
    r.conv->player_say("hi");
    CHECK(r.until([&] { return r.conv->interjections_used() == 1 && r.be->calls().size() == 2; }));   // interjection is generating
    CHECK(r.conv->player_say("never mind, different question"));   // interjection still in flight
    CHECK(r.settle());
    CHECK_EQ(r.count("reply", "interjection"), 0);
    CHECK_EQ(r.last("reply")->data["text"].as_str(), std::string("Okay, moving on."));
}

TEST(conv_outcome_offer_then_commit) {
    Rig r;
    r.be->push(R"({"reply":"Sure.","outcome":"welcome"})");
    r.conv->player_say("Can I sit here?");
    CHECK(r.settle());
    CHECK_EQ(r.count("offer"), 0);                                // min_turns is 2: too early
    r.be->push(R"({"reply":"Come in.","outcome":"welcome"})");
    r.conv->player_say("Thanks!");
    CHECK(r.settle());
    CHECK_EQ(r.count("offer"), 1);
    CHECK_EQ(r.last("offer")->data["label"].as_str(), std::string("Label welcome"));
    CHECK(!r.conv->commit("leave"));                              // not the offered one
    CHECK(r.conv->commit("welcome"));
    r.conv->poll(r.log);
    CHECK_EQ(r.last("done")->data["outcome"].as_str(), std::string("welcome"));
    CHECK_EQ(r.last("done")->data["reason"].as_str(), std::string("committed"));
    CHECK(r.conv->finished());
    CHECK(!r.conv->player_say("more"));
}

TEST(conv_auto_commit_and_unknown_outcome) {
    Rig r(talk_event(0, "auto"));
    r.be->push(R"({"reply":"a"})"); r.conv->player_say("one"); r.settle();
    r.be->push(R"({"reply":"b","outcome":"does_not_exist"})"); r.conv->player_say("two"); r.settle();
    CHECK(!r.conv->finished());
    r.be->push(R"({"reply":"c","outcome":"welcome"})"); r.conv->player_say("three");
    CHECK(r.settle());
    CHECK(r.conv->finished());
    CHECK_EQ(r.last("done")->data["outcome"].as_str(), std::string("welcome"));
}

TEST(conv_she_walks_away_turn_limit_and_leave) {
    {
        Rig r;
        r.be->push(R"({"reply":"I'm leaving.","end":true})");
        r.conv->player_say("hey");
        CHECK(r.settle());
        CHECK_EQ(r.last("done")->data["reason"].as_str(), std::string("ended_by_her"));
        CHECK_EQ(r.last("done")->data["outcome"].as_str(), std::string("leave"));   // on_end
    }
    {
        Rig r(talk_event(2));
        r.be->push(R"({"reply":"a"})"); r.conv->player_say("1"); r.settle();
        CHECK(!r.conv->finished());
        r.be->push(R"({"reply":"b"})"); r.conv->player_say("2"); r.settle();
        CHECK_EQ(r.last("done")->data["reason"].as_str(), std::string("turn_limit"));
    }
    {
        Rig r;
        r.conv->leave();
        r.conv->poll(r.log);
        CHECK_EQ(r.last("done")->data["reason"].as_str(), std::string("left"));
    }
}

TEST(conv_error_then_retry_does_not_duplicate_the_line) {
    Rig r;
    r.be->push("__ERROR__");
    r.conv->player_say("Are you there?");
    CHECK(r.until([&] { return r.count("error") == 1; }));
    CHECK(r.settle());
    CHECK(!r.conv->busy());
    r.be->push(R"({"reply":"Yes, I'm here."})");
    CHECK(r.conv->retry());
    CHECK(r.settle());
    CHECK_EQ(r.last("reply")->data["text"].as_str(), std::string("Yes, I'm here."));
    std::string scene = r.hub.of("hana").render_scene();
    size_t first = scene.find("Are you there?");
    CHECK(first != std::string::npos);
    CHECK(scene.find("Are you there?", first + 1) == std::string::npos);
}

TEST(conv_salvages_malformed_model_output) {
    Rig r;
    r.be->push("{\"reply\": \"Half a thought, then the model");   // cut off mid-string
    r.conv->player_say("hi");
    CHECK(r.settle());
    CHECK_EQ(r.last("reply")->data["text"].as_str(), std::string("Half a thought, then the model"));
    r.be->push("Just plain words, no JSON at all.");
    r.conv->player_say("hi again");
    CHECK(r.settle());
    CHECK_EQ(r.last("reply")->data["text"].as_str(), std::string("Just plain words, no JSON at all."));
}

TEST(conv_prompts_keep_characters_apart_and_stay_prefix_stable) {
    Rig r;
    r.be->push(R"({"reply":"Hi.","memory_ops":[{"op":"add","kind":"fact","text":"Diego likes tea","evidence":"I like tea"}]})");
    r.conv->player_say("I like tea.");
    r.settle();
    r.be->push(R"({"reply":"Noted."})");
    r.conv->player_say("Do you remember?");
    r.settle();
    r.be->push(R"({"reply":"Hello, I'm Mira."})");
    r.conv->player_say("And you?", "mira");
    r.settle();
    auto calls = r.be->calls();
    std::string p1 = calls[0].req.prompt, p2 = calls[1].req.prompt, pm = calls[2].req.prompt;

    // 1. immutable slot is in the stable system prefix, and the prefix is byte-identical between turns
    auto split = [](const std::string& p) { return p.substr(0, p.find("## What you remember")); };
    CHECK_EQ(split(p1), split(p2));
    CHECK(split(p1).find("hana-only") != std::string::npos);
    // 2. the scene log only grows, so the cache can be reused up to the new lines
    size_t s1 = p1.find("## This scene");
    CHECK(p2.find("Diego: I like tea.", s1) != std::string::npos);
    // 3. the new fact shows up for Hana only
    CHECK(p2.find("Diego likes tea") != std::string::npos);
    CHECK(pm.find("Diego likes tea") == std::string::npos);
    CHECK(pm.find("hana-only") == std::string::npos);
    CHECK(pm.find("mira-only") != std::string::npos);
    // 4. Mira nevertheless witnessed the scene
    CHECK(pm.find("Hana: Hi.") != std::string::npos);
}

TEST(conv_scene_change_flushes_scene_memory_but_not_contextual) {
    Rig r;
    r.be->push(R"({"reply":"Hi.","memory_ops":[{"op":"add","kind":"fact","text":"Diego likes tea","evidence":"I like tea"}]})");
    r.conv->player_say("I like tea.");
    r.settle();
    r.hub.begin_scene();
    r.be->push(R"({"reply":"Back again."})");
    r.conv->player_say("Next day.");
    r.settle();
    std::string p = r.be->calls()[1].req.prompt;
    CHECK(p.find("Diego likes tea") != std::string::npos);
    CHECK(p.find("Diego: I like tea.") == std::string::npos);
    CHECK(p.find("Diego: Next day.") != std::string::npos);
}

TEST(conv_wrap_prompt_and_repair_words) {
    CHECK(wrap_prompt("chatml", "S", "U").find("<|im_start|>assistant\n") != std::string::npos);
    CHECK(wrap_prompt("plain", "S", "U").find("### Reply") != std::string::npos);
    CHECK(offers_repair("Lo siento mucho", {}));
    CHECK(offers_repair("I APOLOGIZE", {}));
    CHECK(!offers_repair("whatever", {}));
    CHECK(offers_repair("my bad", {"my bad"}));
}
