#include "test.hpp"
#include "../core/vm.hpp"

using namespace sf;

static std::shared_ptr<Program> build(const std::string& src) {
    return std::make_shared<Program>(Program::compile({{"test.sf", src}}));
}
static Json ev(const char* text) {
    Json vars = Json::object();
    uint64_t rng = 7;
    EvalContext c; c.vars = &vars; c.rng = &rng;
    return eval(parse_expr(text), c);
}

TEST(expr_arithmetic_and_logic) {
    CHECK_EQ(ev("1 + 2 * 3").as_int(), 7);
    CHECK_EQ(ev("(1 + 2) * 3").as_int(), 9);
    CHECK_EQ(ev("7 % 3").as_int(), 1);
    CHECK_EQ(ev("-7 % 3").as_int(), 2);
    CHECK_EQ(ev("7 // 2").as_int(), 3);
    CHECK_NEAR(ev("7 / 2").as_num(), 3.5, 1e-9);
    CHECK(ev("1 < 2 and 2 < 3").as_bool());
    CHECK(ev("not (1 > 2) or false").as_bool());
    CHECK(ev("'a' in 'banana'").as_bool());
    CHECK(ev("3 in [1,2,3]").as_bool());
    CHECK(ev("4 not in [1,2,3]").as_bool());
    CHECK_EQ(ev("'a' + 'b' + 1").as_str(), std::string("ab1"));
    CHECK_EQ(ev("max(1, 5, 3)").as_int(), 5);
    CHECK_EQ(ev("clamp(9, 0, 5)").as_int(), 5);
    CHECK_EQ(ev("len([1,2,3])").as_int(), 3);
    CHECK_EQ(ev("[10,20,30][-1]").as_int(), 30);
    CHECK(ev("missing").is_null());
    CHECK(ev("missing.deeper.still").is_null());
    CHECK_EQ(ev("missing + 2").as_int(), 2);
}

TEST(expr_errors_are_readable) {
    CHECK_THROWS(parse_expr("1 +"));
    CHECK_THROWS(parse_expr("foo(1"));
    CHECK_THROWS(parse_expr("1 $ 2"));
    CHECK_THROWS(ev("1 / 0"));
    CHECK_THROWS(ev("nosuch(1)"));
    CHECK_THROWS(ev("'a' < 1"));
}

TEST(expr_assignment_creates_structure) {
    Json vars = Json::object();
    uint64_t rng = 1;
    EvalContext c; c.vars = &vars; c.rng = &rng;
    assign(parse_expr("rel.hana.trust"), c, Json(3));
    CHECK_EQ(vars["rel"]["hana"]["trust"].as_int(), 3);
    assign(parse_expr("items[2]"), c, Json("x"));
    CHECK_EQ(vars["items"].size(), size_t(3));
    CHECK_THROWS(assign(parse_expr("1 + 2"), c, Json(1)));
}

TEST(rng_is_deterministic) {
    uint64_t a = 99, b = 99;
    for (int i = 0; i < 5; ++i) CHECK_EQ(rng_next(a), rng_next(b));
}

TEST(script_basic_flow) {
    auto p = build(R"(
label start:
    scene cafe with fade
    show hana happy at left with dissolve
    hana "Hello, [player]."
    $ player = "Ana"
    $ trust = 2
    if trust > 1:
        narrator "She smiles."
    else:
        narrator "She frowns."
    "A stranger" "Hi."
    end "done"
)");
    Vm vm(p);
    vm.vars().set("player", "Diego");
    vm.start("start");
    auto e = vm.run();
    CHECK_EQ(e.size(), size_t(5));   // scene, with, show, with, say
    CHECK_EQ(e[0].type, std::string("scene"));
    CHECK_EQ(e[0].data["bg"].as_str(), std::string("cafe"));
    CHECK_EQ(e[1].type, std::string("with"));
    CHECK_EQ(e[2].data["at"].as_str(), std::string("left"));
    CHECK_EQ(e[4].type, std::string("say"));
    CHECK_EQ(e[4].data["text"].as_str(), std::string("Hello, Diego."));
    CHECK(vm.run().empty());          // still waiting
    vm.advance();
    e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("She smiles."));
    vm.advance();
    e = vm.run();
    CHECK_EQ(e.back().data["name"].as_str(), std::string("A stranger"));
    vm.advance();
    e = vm.run();
    CHECK_EQ(e.back().type, std::string("end"));
    CHECK(vm.finished());
    CHECK(p->refs.characters.count("hana"));
    CHECK(p->refs.backgrounds.count("cafe"));
    CHECK(p->refs.poses.count({"hana", "happy"}));
}

TEST(script_menu_and_conditions) {
    auto p = build(R"(
label start:
    $ trust = 1
    menu:
        hana "What now?"
        "Greet her":
            $ choice = "greet"
        "Confess" if trust > 2:
            $ choice = "confess"
        "Leave":
            $ choice = "leave"
    narrator "[choice]"
    end
)");
    Vm vm(p);
    vm.start("start");
    auto e = vm.run();
    CHECK_EQ(e.back().type, std::string("menu"));
    CHECK_EQ(e.back().data["options"].size(), size_t(2));      // "Confess" hidden
    CHECK_EQ(e.back().data["caption"].as_str(), std::string("What now?"));
    CHECK_EQ(e.back().data["speaker"].as_str(), std::string("hana"));
    vm.choose(1);                                               // index among visible options => "Leave"
    e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("leave"));
    CHECK_THROWS(vm.choose(0));                                 // not a menu anymore
}

TEST(script_call_return_jump_seen) {
    auto p = build(R"(
label start:
    call helper
    call helper
    if seen("helper") and n == 2:
        jump finale
    narrator "wrong"
label helper:
    $ n += 1
    return
label finale:
    narrator "ok"
    end
)");
    Vm vm(p);
    vm.start("start");
    auto e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("ok"));
}

TEST(script_talk_block) {
    auto p = build(R"(
label start:
    talk with hana, mira:
        goal: Will you accept a little company?
        commit: ask
        interject_limit: 2
        outcome welcome "A seat inside":
            rule: Player accepts company.
            $ ease += 1
            jump next
        outcome gated "Secret":
            rule: Only when trusted.
            when: trust >= 3
            jump next
        outcome leave:
            rule: Player leaves.
            narrator "bye"
            end
    narrator "fallthrough"
    end
label next:
    narrator "next scene"
    end
)");
    Vm vm(p);
    vm.start("start");
    auto e = vm.run();
    CHECK_EQ(e.back().type, std::string("talk"));
    auto& t = e.back().data;
    CHECK_EQ(t["cast"].size(), size_t(2));
    CHECK_EQ(t["start"].as_str(), std::string("hana"));
    CHECK_EQ(t["outcomes"].size(), size_t(2));                  // gated one hidden
    CHECK_EQ(t["interject_limit"].as_int(), 2);
    CHECK_THROWS(vm.finish_talk("gated"));
    vm.finish_talk("welcome");
    e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("next scene"));
    CHECK_EQ(vm.vars()["ease"].as_int(), 1);
}

TEST(script_minigame_result) {
    auto p = build(R"(
label start:
    minigame cooking(order="soup", time=30 + 30) -> res
    if res.outcome == "win":
        narrator "tasty [res.score]"
    else:
        narrator "burnt"
    end
)");
    Vm vm(p);
    vm.start("start");
    auto e = vm.run();
    CHECK_EQ(e.back().type, std::string("minigame"));
    CHECK_EQ(e.back().data["args"]["time"].as_int(), 60);
    CHECK_EQ(e.back().data["args"]["order"].as_str(), std::string("soup"));
    Json r = Json::object(); r.set("outcome", "win"); r.set("score", 90);
    vm.finish_minigame(r);
    e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("tasty 90"));
}

TEST(script_save_load_roundtrip_mid_menu) {
    auto p = build(R"(
label start:
    $ gold = 5
    call shop
    narrator "done [gold]"
    end
label shop:
    menu:
        "Buy":
            $ gold -= 2
        "Skip":
            $ gold += 0
    return
)");
    Vm a(p);
    a.start("start");
    a.run();
    CHECK_EQ(a.wait_kind(), std::string("menu"));
    Json saved = Json::parse(a.save().dump());
    Vm b(p);
    CHECK(b.load(saved));
    CHECK_EQ(b.wait_kind(), std::string("menu"));
    CHECK_EQ(b.wait_event()["options"].size(), size_t(2));
    b.choose(0);
    auto e = b.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("done 3"));
}

TEST(script_load_after_edit_degrades_gracefully) {
    auto v1 = build("label start:\n    narrator \"one\"\n    narrator \"two\"\n    end\n");
    Vm a(v1);
    a.start("start"); a.run(); a.advance(); a.run();
    Json saved = a.save();
    auto v2 = build("label start:\n    narrator \"intro\"\n    narrator \"one\"\n    narrator \"two\"\n    end\n");
    Vm b(v2);
    CHECK(!b.load(saved));   // reports the mismatch
    auto v3 = build("label other:\n    end\n");
    Vm c(v3);
    CHECK_THROWS(c.load(saved));
}

TEST(script_errors_have_file_and_line) {
    auto expect_msg = [](const std::string& src, const std::string& needle) {
        try { Program::compile({{"x.sf", src}}); }
        catch (const Error& e) { CHECK(std::string(e.what()).find(needle) != std::string::npos); if (std::string(e.what()).find(needle) == std::string::npos) std::cerr << "    got: " << e.what() << "\n"; return; }
        ++sft::failures(); std::cerr << "  FAIL expected error containing: " << needle << "\n";
    };
    expect_msg("label a:\n    jump nowhere\n", "x.sf:2: unknown label 'nowhere'");
    expect_msg("label a:\n    hana Hello\n", "x.sf:2");
    expect_msg("label a:\nlabel a:\n    end\n", "x.sf:2");
    expect_msg("label a:\n    $ x +\n", "x.sf:2");
    expect_msg("label a:\n    scene cafe with spin\n", "unknown transition");
    expect_msg("label a:\n    menu:\n", "x.sf");
    expect_msg("  narrator \"hi\"\n", "x.sf:1");
    expect_msg("label a:\n    talk with hana:\n        goal: x\n", "needs at least one outcome");
    expect_msg("label a:\n    narrator \"[oops\"\n", "unterminated");
    expect_msg("label a:\n    if x\n        end\n", "trailing ':'");
}

TEST(script_runaway_loop_is_caught) {
    auto p = build("label a:\n    jump a\n");
    Vm vm(p);
    vm.start("a");
    CHECK_THROWS(vm.run());
}

TEST(script_comments_and_apostrophes) {
    auto p = build("label a:\n    # a comment\n    narrator \"Don't # panic\"  # trailing\n    end\n");
    Vm vm(p);
    vm.start("a");
    auto e = vm.run();
    CHECK_EQ(e.back().data["text"].as_str(), std::string("Don't # panic"));
}
