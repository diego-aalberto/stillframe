#include "test.hpp"
#include "../core/memory.hpp"

using namespace sf;

static CharacterMemory fresh(MemoryConfig cfg = {}) {
    return CharacterMemory("hana", std::make_shared<const ImmutableSlot>(std::vector<std::pair<std::string, std::string>>{{"identity", "Hana, 24"}, {"quirk", "Names broken appliances"}}), cfg);
}
static MemoryOp add(const std::string& kind, const std::string& text, const std::string& ev, int sev = 0, int refuses = -1, const std::string& line = "") {
    MemoryOp o; o.op = "add"; o.kind = kind; o.text = text; o.evidence = ev; o.severity = sev; o.refuses = refuses; o.refusal_line = line; return o;
}
static MemoryOp op(const std::string& what, const std::string& id, const std::string& ev = "") {
    MemoryOp o; o.op = what; o.id = id; o.evidence = ev; return o;
}
static OpContext ctx(int64_t turn, const std::string& said) { OpContext c; c.turn = turn; c.player_utterance = said; c.scene = "s1"; return c; }

TEST(memory_immutable_slot_is_constant_and_unaddressable) {
    auto m = fresh();
    CHECK_EQ(m.immutable().facts().size(), size_t(2));
    CHECK(m.immutable().render().find("identity: Hana, 24") != std::string::npos);
    // contextual ids start with "m"; nothing in an operation can name an immutable fact
    auto r = m.apply({op("update", "i1"), op("forget", "identity"), op("resolve", "name")}, ctx(1, "x"));
    for (auto& x : r) CHECK(!x.ok);
    CHECK_EQ(m.immutable().facts().size(), size_t(2));
    static_assert(std::is_const_v<std::remove_reference_t<decltype(m.immutable().facts())>>, "immutable slot must expose const data only");
}

TEST(memory_add_requires_real_evidence) {
    auto m = fresh();
    auto r = m.apply({add("fact", "Player is allergic to nuts", "I can't eat nuts"), add("fact", "Player loves jazz", "I hate jazz")}, ctx(1, "I can't eat nuts, sorry."));
    CHECK(r[0].ok);
    CHECK(!r[1].ok);            // invented quote
    CHECK_EQ(m.contextual().size(), size_t(1));
    CHECK_EQ(m.contextual()[0].id, std::string("m1"));
    CHECK(m.contextual()[0].text.find("nuts") != std::string::npos);
}

TEST(memory_evidence_can_come_from_witnessed_scene_lines) {
    auto m = fresh();
    OpContext c = ctx(2, "ok");
    c.witnessed = {"Mira: I broke the projector on purpose"};
    auto r = m.apply({add("fact", "Mira admitted breaking the projector", "broke the projector on purpose")}, c);
    CHECK(r[0].ok);
}

TEST(memory_grudge_can_make_her_refuse) {
    auto m = fresh();
    CHECK_EQ(m.willingness(), std::string("open"));
    auto r = m.apply({add("grudge", "He called my drawings childish", "your drawings are childish", 3, 1, "Not until you take that back.")}, ctx(1, "Honestly, your drawings are childish."));
    CHECK(r[0].ok);
    CHECK_EQ(m.willingness(), std::string("refusing"));
    CHECK_EQ(m.blocking()->refusal_line, std::string("Not until you take that back."));
    // a refusing grudge needs the line she will say, and a real severity
    auto bad = m.apply({add("grudge", "x", "your drawings", 1, 1, "no"), add("grudge", "y", "your drawings", 3, 1, "")}, ctx(1, "your drawings"));
    CHECK(!bad[0].ok); CHECK(!bad[1].ok);
    // only grudges refuse
    CHECK(!m.apply({add("fact", "z", "your drawings", 2, 1, "no")}, ctx(1, "your drawings"))[0].ok);
}

TEST(memory_grudge_resolution_rules) {
    auto m = fresh();
    m.apply({add("grudge", "He mocked my drawings", "mocked your drawings", 3, 1, "Not talking.")}, ctx(5, "I mocked your drawings."));
    std::string id = m.contextual()[0].id;
    // same turn: too soon
    CHECK(!m.apply({op("resolve", id, "I'm sorry")}, ctx(5, "I'm sorry"))[0].ok);
    // later turn but the quote is not from the player's current words
    CHECK(!m.apply({op("resolve", id, "I'm sorry I mocked them")}, ctx(6, "so anyway, nice weather"))[0].ok);
    CHECK_EQ(m.willingness(), std::string("refusing"));
    // a real apology now
    CHECK(m.apply({op("resolve", id, "I'm sorry I mocked them")}, ctx(6, "I'm sorry I mocked them. That was cruel."))[0].ok);
    CHECK_EQ(m.willingness(), std::string("open"));
    CHECK_EQ(m.find(id)->status, std::string("resolved"));
    CHECK(!m.apply({op("resolve", id, "again")}, ctx(7, "again"))[0].ok);   // already resolved
}

TEST(memory_calming_a_grudge_needs_the_player_to_have_earned_it) {
    auto m = fresh();
    m.apply({add("grudge", "Insult", "you are boring", 3, 1, "Leave me be.")}, ctx(1, "you are boring"));
    std::string id = m.contextual()[0].id;
    MemoryOp soften = op("update", id, "");
    soften.severity = 1; soften.refuses = 0;
    CHECK(!m.apply({soften}, ctx(4, "hello again"))[0].ok);
    soften.evidence = "I apologize";
    CHECK(m.apply({soften}, ctx(4, "I apologize, truly"))[0].ok);
    CHECK_EQ(m.willingness(), std::string("guarded"));   // still carries an open grudge, but will talk
    // she can always get angrier without proof
    MemoryOp worse = op("update", id); worse.severity = 5;
    CHECK(m.apply({worse}, ctx(5, "whatever"))[0].ok);
    CHECK_EQ(m.find(id)->severity, 5);
}

TEST(memory_forget_rules) {
    auto m = fresh();
    m.apply({add("fact", "likes tea", "I like tea"), add("grudge", "insult", "ugly", 2, 0)}, ctx(1, "I like tea and you are ugly"));
    CHECK(m.apply({op("forget", "m1")}, ctx(2, ""))[0].ok);
    CHECK(!m.apply({op("forget", "m2")}, ctx(2, ""))[0].ok);   // unresolved grudge cannot be forgotten
    CHECK_EQ(m.contextual().size(), size_t(1));
}

TEST(memory_caps_dedupe_and_room) {
    MemoryConfig cfg; cfg.contextual_max = 4; cfg.max_ops_per_turn = 2;
    auto m = fresh(cfg);
    auto r = m.apply({add("fact", "a", "x"), add("fact", "b", "x"), add("fact", "c", "x")}, ctx(1, "x"));
    CHECK(r[0].ok); CHECK(r[1].ok); CHECK(!r[2].ok);   // per-turn cap
    m.apply({add("fact", "A", "x")}, ctx(2, "x"));       // duplicate text (case-insensitive) merges
    CHECK_EQ(m.contextual().size(), size_t(2));
    for (int i = 0; i < 6; ++i) m.apply({add("fact", "fact" + std::to_string(i), "x")}, ctx(3 + i, "x"));
    CHECK(static_cast<int>(m.contextual().size()) <= 4);
    // open grudges are never evicted to make room
    auto g = fresh(cfg);
    for (int i = 0; i < 4; ++i) g.apply({add("grudge", "g" + std::to_string(i), "x", 2, 0)}, ctx(i, "x"));
    CHECK(!g.apply({add("fact", "new", "x")}, ctx(9, "x"))[0].ok);
    CHECK_EQ(g.contextual().size(), size_t(4));
}

TEST(memory_unknown_input_is_rejected_not_crashed) {
    auto m = fresh();
    auto ops = MemoryOp::parse(Json::parse(R"([{"op":"teleport"},{"op":"add","kind":"weird","text":"t","evidence":"x"},{"op":"add"},"junk",7,{"op":"add","kind":"fact","text":"ok","evidence":"hi"}])"));
    CHECK_EQ(ops.size(), size_t(4));
    auto r = m.apply(ops, ctx(1, "hi there"));
    int okc = 0; for (auto& x : r) okc += x.ok;
    CHECK_EQ(okc, 1);
}

TEST(memory_scene_slot_is_per_character_and_flushes) {
    Json manifest = Json::parse(R"({"format":"stillgame","version":1,"id":"t"})");
    auto g = std::make_shared<Game>();
    for (const char* id : {"hana", "mira", "leila"}) { CharacterDef c; c.id = id; c.name = id; c.immutable = {{"", std::string("I am ") + id}}; g->characters[id] = c; }
    MemoryHub hub(*g, {});
    hub.witness("player", "Hello Hana", {"hana", "mira"}, "hana");
    hub.witness("hana", "Hi!", {"hana", "mira"}, "player");
    CHECK_EQ(hub.of("hana").scene().size(), size_t(2));
    CHECK_EQ(hub.of("mira").scene().size(), size_t(2));
    CHECK_EQ(hub.of("leila").scene().size(), size_t(0));        // was not there: knows nothing
    // contextual memory is separate per character too
    hub.of("hana").apply({add("fact", "Player says hello politely", "Hello Hana")}, ctx(1, "Hello Hana"));
    CHECK_EQ(hub.of("hana").contextual().size(), size_t(1));
    CHECK_EQ(hub.of("mira").contextual().size(), size_t(0));
    hub.begin_scene();
    CHECK_EQ(hub.of("hana").scene().size(), size_t(0));
    CHECK_EQ(hub.of("hana").contextual().size(), size_t(1));    // scene change does not erase what she learned
    // "addressed" witness mode: bystanders do not record the line
    MemoryConfig cfg; cfg.witness_present = false;
    MemoryHub quiet(*g, cfg);
    quiet.witness("player", "psst", {"hana", "mira", "leila"}, "hana");
    CHECK_EQ(quiet.of("hana").scene().size(), size_t(1));
    CHECK_EQ(quiet.of("mira").scene().size(), size_t(0));
}

TEST(memory_scene_slot_is_capped) {
    MemoryConfig cfg; cfg.scene_max_lines = 3;
    auto m = fresh(cfg);
    for (int i = 0; i < 10; ++i) m.witness("player", "line " + std::to_string(i));
    CHECK_EQ(m.scene().size(), size_t(3));
    CHECK_EQ(m.scene().back().text, std::string("line 9"));
}

TEST(memory_save_load_roundtrip_and_ids_never_reused) {
    auto m = fresh();
    m.apply({add("fact", "one", "x"), add("fact", "two", "x")}, ctx(1, "x"));
    m.apply({op("forget", "m2")}, ctx(2, ""));
    m.witness("player", "seen this");
    Json saved = Json::parse(m.save().dump());
    auto n = fresh();
    n.load(saved);
    CHECK_EQ(n.contextual().size(), size_t(1));
    CHECK_EQ(n.scene().size(), size_t(1));
    n.apply({add("fact", "three", "x")}, ctx(3, "x"));
    CHECK_EQ(n.contextual().back().id, std::string("m3"));   // m2 is not reused
}

TEST(memory_prompt_rendering_prioritises_refusals_and_respects_budget) {
    MemoryConfig cfg; cfg.prompt_budget_chars = 300;
    auto m = fresh(cfg);
    m.apply({add("fact", "likes tea", "x"), add("grudge", "Called my art childish", "x", 3, 1, "No.")}, ctx(1, "x"));
    for (int i = 0; i < 8; ++i) m.apply({add("fact", "filler fact number " + std::to_string(i) + " with some padding text", "x")}, ctx(2 + i, "x"));
    std::string out = m.render_contextual();
    CHECK(out.find("childish") != std::string::npos);
    CHECK(out.find("you refuse to talk") != std::string::npos);
    CHECK(out.find("childish") < out.find("likes tea"));       // refusal first
    CHECK(out.size() < 420);
}

TEST(memory_clip_never_splits_utf8) {
    auto m = fresh();
    std::string long_text;
    for (int i = 0; i < 400; ++i) long_text += "é";
    m.apply({add("fact", long_text, "x")}, ctx(1, "x"));
    const std::string& t = m.contextual()[0].text;
    CHECK(t.size() <= 300);
    CHECK(t.size() % 2 == 0);   // every é is two bytes
}
