#include "minigame.hpp"
#include "platform.hpp"
#include <filesystem>
#include <fstream>

namespace sf {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static const char* platform_key() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

std::vector<std::string> minigame_command(const MinigameDef& def, const std::string& dir) {
    const Json& l = def.manifest["launch"];
    const Json* cmd = &l["command"];
    if (l[platform_key()].is_array() && l[platform_key()].size() > 0) cmd = &l[platform_key()];
    std::vector<std::string> out;
    for (auto& a : cmd->items()) {
        std::string s = a.as_str();
        size_t p;
        while ((p = s.find("{dir}")) != std::string::npos) s.replace(p, 5, dir);
        out.push_back(s);
    }
    return out;
}

Json minigame_result_value(const std::string& outcome, const Json& data) {
    Json j = Json::object();
    j.set("outcome", outcome);
    j.set("data", data.is_null() ? Json::object() : data);
    return j;
}

std::string materialize_minigame(const Game& game, const MinigameDef& def, const std::string& cache_dir) {
    if (!game.vfs->is_archive()) {
        std::string d = game.vfs->disk_path(def.dir);
        if (!d.empty()) return d;   // a folder game runs in place: edit, relaunch, no copying
    }
    fs::path target = fs::path(cache_dir) / game.id / game.version / def.dir;
    fs::path marker = target / ".sf_ready";
    std::error_code ec;
    if (fs::exists(marker, ec)) return target.string();
    fs::remove_all(target, ec);
    fs::create_directories(target, ec);
    std::string prefix = def.dir + "/";
    std::string exe = def.manifest["launch"]["command"].at(0).as_str();
    for (auto& f : game.vfs->list()) {
        if (f.compare(0, prefix.size(), prefix) != 0) continue;
        std::string rel = normalize_path(f.substr(prefix.size()));   // throws if it tries to escape
        fs::path out = target / rel;
        fs::create_directories(out.parent_path(), ec);
        Bytes b = game.vfs->read(f);
        std::ofstream o(out, std::ios::binary);
        if (!o) throw Error("cannot write " + out.string());
        o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
        o.close();
#ifndef _WIN32
        // Zip files don't carry reliable permission bits: make the launcher and obvious binaries runnable.
        bool is_cmd = !exe.empty() && (exe == rel || exe == "./" + rel);
        bool in_bin = rel.compare(0, 4, "bin/") == 0 || rel.find('.') == std::string::npos || ends_with(rel, ".sh") || ends_with(rel, ".x86_64");
        if (is_cmd || in_bin) fs::permissions(out, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
#endif
    }
    std::ofstream(marker) << game.version;
    return target.string();
}

std::unique_ptr<MinigameSession> MinigameSession::launch(const Game& game, const std::string& id, const Json& args,
                                                         const Json& context, const std::string& cache_dir) {
    auto it = game.minigames.find(id);
    if (it == game.minigames.end()) throw Error("this game has no minigame called '" + id + "'");
    const MinigameDef& def = it->second;
    std::unique_ptr<MinigameSession> s(new MinigameSession);
    s->id_ = id;
    s->dir_ = materialize_minigame(game, def, cache_dir);
    auto cmd = minigame_command(def, s->dir_);
    if (cmd.empty()) throw Error("minigame '" + id + "' has no launch command");
    for (auto& o : def.manifest["outcomes"].items()) s->outcomes_.push_back(o.as_str());
    if (def.manifest["fallback_outcome"].is_string()) s->fallback_ = def.manifest["fallback_outcome"].as_str();
    s->startup_timeout_ms_ = static_cast<int>(def.manifest["launch"]["startup_timeout_s"].as_num(20) * 1000);
    s->timeout_ms_ = static_cast<int>(def.manifest["timeout_s"].as_num(0) * 1000);

    SpawnOptions so;
    so.argv = cmd;
    so.cwd = s->dir_;
    so.env["STILLFRAME_MINIGAME"] = id;
    so.env["STILLFRAME_PROTOCOL"] = std::to_string(kMinigameProtocol);
    so.stderr_path = (fs::path(cache_dir) / (game.id + "." + id + ".log")).string();
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    s->proc_ = Process::spawn(so);
    s->started_ = Clock::now();

    Json start = Json::object();
    start.set("type", "start"); start.set("protocol", kMinigameProtocol); start.set("game", game.id);
    start.set("args", args.is_null() ? Json::object() : args);
    start.set("context", context.is_null() ? Json::object() : context);
    s->proc_->write_line(start.dump());
    return s;
}

MinigameSession::~MinigameSession() { if (proc_) proc_->terminate(); }

void MinigameSession::fail(const std::string& msg, std::vector<MgEvent>& out) {
    if (finished_) return;
    finished_ = true;
    proc_->terminate();
    MgEvent e{"failed", Json::object()};
    e.data.set("message", msg);
    out.push_back(std::move(e));
}

void MinigameSession::answer_say(int64_t id, const std::string& text) {
    if (finished_) return;
    Json j = Json::object();
    j.set("type", "say_result"); j.set("id", id); j.set("text", text);
    proc_->write_line(j.dump());
}

void MinigameSession::cancel() {
    if (finished_) return;
    cancelled_ = true;
    finished_ = true;
    Json j = Json::object(); j.set("type", "cancel");
    proc_->write_line(j.dump());
    proc_->close_stdin();
    if (proc_->wait(1500) == -9999) proc_->terminate();
}

void MinigameSession::poll(std::vector<MgEvent>& out) {
    if (finished_) return;
    std::string line;
    bool eof = false;
    int budget = 200;       // lines per poll, so a chatty program can't freeze a frame
    while (budget-- > 0 && !finished_ && proc_->read_line(line, 0, eof)) {
        line = trim(line);
        if (line.empty() || line[0] != '{' || line.size() > (1u << 20)) continue;
        Json j;
        try { j = Json::parse(line); } catch (const Error&) { continue; }
        std::string t = j["type"].as_str();
        if (t == "ready") {
            ready_ = true;
            out.push_back({"ready", Json::object()});
        } else if (t == "progress") {
            Json d = Json::object();
            d.set("value", j["value"].as_num()); d.set("label", j["label"].as_str());
            out.push_back({"progress", d});
        } else if (t == "event") {
            Json d = Json::object();
            d.set("name", j["name"].as_str()); d.set("data", j["data"]);
            out.push_back({"event", d});
        } else if (t == "say") {
            Json d = Json::object();
            d.set("id", j["id"].as_int()); d.set("character", j["character"].as_str()); d.set("prompt", j["prompt"].as_str());
            out.push_back({"say", d});
        } else if (t == "result") {
            std::string oc = j["outcome"].as_str();
            if (oc.empty()) { fail("the minigame ended without saying how it went (no \"outcome\")", out); return; }
            if (!outcomes_.empty()) {
                bool ok = false;
                for (auto& o : outcomes_) if (o == oc) ok = true;
                if (!ok) { fail("the minigame reported outcome '" + oc + "', which minigame.json does not list", out); return; }
            }
            finished_ = true;
            proc_->close_stdin();
            if (proc_->wait(1500) == -9999) proc_->terminate();
            Json d = Json::object();
            d.set("outcome", oc); d.set("data", j["data"].is_null() ? Json::object() : j["data"]);
            out.push_back({"result", d});
            return;
        }
    }
    if (finished_) return;
    if (eof || !proc_->running()) {
        // It may have written its result just before exiting: look once more before declaring a crash.
        if (!eof) return;
        int code = proc_->wait(1000);
        fail(code == 0 ? "the minigame closed without finishing" : "the minigame crashed (exit code " + std::to_string(code) + ")", out);
        return;
    }
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started_).count());
    if (!ready_ && elapsed > startup_timeout_ms_) fail("the minigame did not start within " + std::to_string(startup_timeout_ms_ / 1000) + " seconds", out);
    else if (timeout_ms_ > 0 && elapsed > timeout_ms_) fail("the minigame ran longer than its time limit", out);
}

} // namespace sf
