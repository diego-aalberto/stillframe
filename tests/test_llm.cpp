#include "test.hpp"
#include "../core/llm.hpp"
#include "../core/http.hpp"
#include "../core/process.hpp"
#include "../core/platform.hpp"

using namespace sf;

#ifndef SF_SOURCE_DIR
#define SF_SOURCE_DIR "."
#endif

TEST(partial_reply_decodes_while_streaming) {
    std::string full = "{\"reply\": \"Hi \\\"there\\\"\\nCaf\\u00e9 \\ud83d\\ude00!\", \"pose\": \"shy\"}";
    bool closed = false;
    // every prefix must give a prefix of the final text, never garbage
    std::string final_text = partial_string_field(full, "reply", &closed);
    CHECK(closed);
    CHECK_EQ(final_text, std::string("Hi \"there\"\nCaf\xC3\xA9 \xF0\x9F\x98\x80!"));
    std::string prev;
    for (size_t n = 0; n <= full.size(); ++n) {
        bool c = false;
        std::string cur = partial_string_field(std::string_view(full).substr(0, n), "reply", &c);
        CHECK(final_text.compare(0, cur.size(), cur) == 0);
        CHECK(cur.size() >= prev.size());
        prev = cur;
    }
}

TEST(partial_reply_ignores_nested_and_other_fields) {
    std::string t = "{\"memory_ops\": [{\"reply\": \"nope\"}], \"pose\": \"x\", \"reply\": \"yes\"}";
    CHECK_EQ(partial_string_field(t, "reply"), std::string("yes"));
    CHECK_EQ(partial_string_field("{\"pose\":\"a\",\"re", "reply"), std::string(""));
    CHECK_EQ(partial_string_field("{\"reply\":\"abc", "reply"), std::string("abc"));
}

TEST(extract_json_object_tolerates_chatter) {
    Json j;
    CHECK(extract_json_object("Sure! Here you go:\n```json\n{\"reply\": \"a } b\", \"n\": {\"x\": 1}}\n``` hope that helps", j));
    CHECK_EQ(j["reply"].as_str(), std::string("a } b"));
    CHECK_EQ(j["n"]["x"].as_int(), 1);
    CHECK(!extract_json_object("no json here", j));
    CHECK(!extract_json_object("{\"broken\": ", j));
    CHECK(extract_json_object("{oops} {\"ok\": true}", j));
    CHECK(j["ok"].as_bool());
}

static bool wait_done(LlmScheduler& s, uint64_t id, std::string* text, LlmEvent::Type* type, int ms = 5000) {
    for (int waited = 0; waited < ms; waited += 20) {
        s.wait(20);
        std::vector<LlmEvent> ev;
        s.poll(ev);
        for (auto& e : ev) if (e.id == id && e.type != LlmEvent::Delta) { if (text) *text = e.text; if (type) *type = e.type; return true; }
    }
    return false;
}

TEST(scheduler_streams_and_finishes) {
    auto be = std::make_shared<ScriptedBackend>();
    be->push("{\"reply\":\"hello there friend\"}");
    LlmScheduler s(be);
    LlmRequest r; r.prompt = "p";
    uint64_t id = s.submit(r, LlmPriority::Player);
    std::string text; LlmEvent::Type t;
    CHECK(wait_done(s, id, &text, &t));
    CHECK(t == LlmEvent::Done);
    CHECK_EQ(text, std::string("{\"reply\":\"hello there friend\"}"));
}

TEST(scheduler_runs_player_before_interjection) {
    auto be = std::make_shared<ScriptedBackend>();
    for (int i = 0; i < 4; ++i) be->push("{\"reply\":\"" + std::to_string(i) + "\"}");
    be->set_delay_ms(30);
    LlmScheduler s(be);
    LlmRequest first; first.prompt = "first";       // occupies the worker
    s.submit(first, LlmPriority::Player);
    LlmRequest a; a.prompt = "interject";
    LlmRequest b; b.prompt = "player";
    LlmRequest c; c.prompt = "background";
    s.submit(c, LlmPriority::Background);
    s.submit(a, LlmPriority::Interject);
    s.submit(b, LlmPriority::Player);
    for (int i = 0; i < 400 && !s.idle(); ++i) sleep_ms(10);
    auto calls = be->calls();
    CHECK_EQ(calls.size(), size_t(4));
    CHECK_EQ(calls[0].req.prompt, std::string("first"));
    CHECK_EQ(calls[1].req.prompt, std::string("player"));
    CHECK_EQ(calls[2].req.prompt, std::string("interject"));
    CHECK_EQ(calls[3].req.prompt, std::string("background"));
}

TEST(scheduler_cancel_running_and_queued) {
    auto be = std::make_shared<ScriptedBackend>();
    be->push(std::string(300, 'x'));
    be->push("{}");
    be->set_delay_ms(10);
    LlmScheduler s(be);
    LlmRequest r; r.prompt = "long";
    uint64_t a = s.submit(r, LlmPriority::Player);
    uint64_t b = s.submit(r, LlmPriority::Interject);
    sleep_ms(60);
    s.cancel(a);
    s.cancel(b);
    std::string txt; LlmEvent::Type t;
    CHECK(wait_done(s, a, &txt, &t));
    CHECK(t == LlmEvent::Cancelled);
    for (int i = 0; i < 200 && !s.idle(); ++i) sleep_ms(10);
    CHECK(s.idle());
    CHECK(be->calls()[0].cancelled);
    CHECK_EQ(be->calls().size(), size_t(1));   // the queued one never ran
}

TEST(scheduler_reports_backend_failure) {
    auto be = std::make_shared<ScriptedBackend>();
    be->push("__ERROR__");
    LlmScheduler s(be);
    LlmRequest r; r.prompt = "x";
    uint64_t id = s.submit(r, LlmPriority::Player);
    std::string txt; LlmEvent::Type t;
    CHECK(wait_done(s, id, &txt, &t));
    CHECK(t == LlmEvent::Failed);
    CHECK(txt.find("scripted failure") != std::string::npos);
}

TEST(llama_server_backend_against_fake_server) {
    SpawnOptions o;
    o.argv = {"python3", "-I", std::string(SF_SOURCE_DIR) + "/tools/fake_llama_server.py"};
    auto proc = Process::spawn(o);
    std::string line; bool eof = false;
    CHECK(proc->read_line(line, 5000, eof));
    std::string base = "http://127.0.0.1:" + line.substr(5);
    Json sc = Json::object(), arr = Json::array();
    arr.push(Json("{\"reply\":\"I hear you.\",\"pose\":\"shy\"}"));
    sc.set("responses", arr);
    http_request("POST", base + "/_script", {}, sc.dump());

    LlamaServerBackend be(base);
    int n_ctx = 0, slots = 0;
    CHECK_EQ(be.probe(&n_ctx, &slots), std::string(""));
    CHECK_EQ(n_ctx, 4096);
    LlmRequest r; r.prompt = "hello"; r.seed = 7; r.slot = 0;
    r.schema = Json::parse("{\"type\":\"object\"}");
    std::atomic<bool> cancel{false};
    std::string streamed;
    std::string out = be.complete(r, [&](const std::string& d) { streamed += d; }, cancel);
    CHECK_EQ(out, std::string("{\"reply\":\"I hear you.\",\"pose\":\"shy\"}"));
    CHECK_EQ(streamed, out);
    Json log = Json::parse(http_request("GET", base + "/_log", {}, "").body);
    CHECK_EQ(log.at(0)["prompt"].as_str(), std::string("hello"));
    CHECK(log.at(0)["cache_prompt"].as_bool());
    CHECK_EQ(log.at(0)["seed"].as_int(), 7);
    CHECK_EQ(log.at(0)["id_slot"].as_int(), 0);
    CHECK(log.at(0)["json_schema"].is_object());

    arr = Json::array(); arr.push(Json("__ERROR__")); sc.set("responses", arr);
    http_request("POST", base + "/_script", {}, sc.dump());
    bool threw = false;
    try { be.complete(r, [](const std::string&) {}, cancel); } catch (const Error& e) { threw = std::string(e.what()).find("boom") != std::string::npos; }
    CHECK(threw);

    LlamaServerBackend dead("http://127.0.0.1:1");
    CHECK(!dead.probe().empty());
}
