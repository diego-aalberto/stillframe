#include "test.hpp"
#include "../core/http.hpp"
#include "../core/process.hpp"
#include "../core/json.hpp"
#include "../core/platform.hpp"
#include <atomic>
#include <thread>

using namespace sf;

#ifndef SF_SOURCE_DIR
#define SF_SOURCE_DIR "."
#endif

namespace {
struct FakeServer {
    std::unique_ptr<Process> proc;
    std::string base;
    FakeServer() {
        SpawnOptions o;
        o.argv = {"python3", "-I", std::string(SF_SOURCE_DIR) + "/tools/fake_llama_server.py"};
        proc = Process::spawn(o);
        std::string line; bool eof = false;
        if (!proc->read_line(line, 5000, eof) || line.rfind("PORT ", 0) != 0) throw Error("fake server did not start");
        base = "http://127.0.0.1:" + line.substr(5);
    }
    void script(const std::vector<std::string>& r, double delay = 0) {
        Json b = Json::object();
        Json arr = Json::array();
        for (auto& s : r) arr.push(Json(s));
        b.set("responses", arr);
        b.set("delay", Json(delay));
        http_request("POST", base + "/_script", {}, b.dump());
    }
};
}

TEST(url_parsing) {
    auto u = parse_url("http://127.0.0.1:8080/completion");
    CHECK_EQ(u.host, std::string("127.0.0.1"));
    CHECK_EQ(u.port, 8080);
    CHECK_EQ(u.path, std::string("/completion"));
    CHECK_THROWS(parse_url("https://example.com"));
    CHECK_EQ(parse_url("http://localhost").port, 80);
}

TEST(http_get_and_post) {
    FakeServer s;
    auto r = http_request("GET", s.base + "/props", {}, "");
    CHECK_EQ(r.status, 200);
    CHECK_EQ(Json::parse(r.body)["total_slots"].as_int(), 1);
    auto nf = http_request("GET", s.base + "/nope", {}, "");
    CHECK_EQ(nf.status, 404);
}

TEST(http_streams_chunked_sse) {
    FakeServer s;
    s.script({"hello streaming world"});
    std::string all; int pieces = 0;
    Json b = Json::object(); b.set("prompt", Json("x")); b.set("stream", Json(true));
    auto r = http_request("POST", s.base + "/completion", {}, b.dump(), [&](std::string_view d) { all.append(d); ++pieces; return true; });
    CHECK_EQ(r.status, 200);
    CHECK(pieces >= 1);
    CHECK(all.find("data: ") != std::string::npos);
    CHECK(all.find("\"stop\": true") != std::string::npos || all.find("\"stop\":true") != std::string::npos);
}

TEST(http_error_and_unreachable) {
    FakeServer s;
    s.script({"__ERROR__"});
    Json b = Json::object(); b.set("prompt", Json("x"));
    auto r = http_request("POST", s.base + "/completion", {}, b.dump(), [](std::string_view) { return true; });
    CHECK_EQ(r.status, 500);
    CHECK(r.body.find("boom") != std::string::npos);
    CHECK_THROWS(http_request("GET", "http://127.0.0.1:1/x", {}, ""));
}

TEST(http_cancel_aborts_stream) {
    FakeServer s;
    s.script({std::string(400, 'a')}, 0.05);
    std::atomic<bool> cancel{false};
    Json b = Json::object(); b.set("prompt", Json("x"));
    int got = 0; bool threw = false;
    try {
        http_request("POST", s.base + "/completion", {}, b.dump(), [&](std::string_view) { if (++got == 3) cancel = true; return true; }, &cancel);
    } catch (const Cancelled&) { threw = true; }
    CHECK(threw);
    sleep_ms(300);
    auto log = Json::parse(http_request("GET", s.base + "/_log", {}, "").body);
    CHECK(log.at(0)["aborted"].as_bool());
}

TEST(process_line_protocol) {
    SpawnOptions o;
    o.argv = {"python3", "-I", "-u", "-c", "import sys\nfor l in sys.stdin:\n    print('echo:'+l.strip())\n"};
    auto p = Process::spawn(o);
    CHECK(p->write_line("one"));
    std::string line; bool eof = false;
    CHECK(p->read_line(line, 3000, eof));
    CHECK_EQ(line, std::string("echo:one"));
    p->close_stdin();
    CHECK(!p->read_line(line, 3000, eof));
    CHECK(eof);
    CHECK_EQ(p->wait(3000), 0);
}

TEST(process_missing_program_and_kill) {
    SpawnOptions o; o.argv = {"definitely-not-a-real-program-xyz"};
    CHECK_THROWS(Process::spawn(o));
    SpawnOptions s; s.argv = {"python3", "-I", "-c", "import time; time.sleep(60)"};
    auto p = Process::spawn(s);
    CHECK(p->running());
    p->terminate();
    CHECK(!p->running());
}
