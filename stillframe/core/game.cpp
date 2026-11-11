#include "game.hpp"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace sf {

static std::string stem(const std::string& path) {
    std::string f = path.substr(path.rfind('/') == std::string::npos ? 0 : path.rfind('/') + 1);
    auto d = f.rfind('.');
    return d == std::string::npos ? f : f.substr(0, d);
}
static bool is_image(const std::string& p) { auto l = lower(p); return ends_with(l, ".png") || ends_with(l, ".jpg") || ends_with(l, ".jpeg"); }
static bool is_audio(const std::string& p) { auto l = lower(p); return ends_with(l, ".ogg") || ends_with(l, ".wav") || ends_with(l, ".mp3") || ends_with(l, ".flac"); }

static void fail(const std::string& file, const std::string& m) { throw Error(file + ": " + m); }

std::shared_ptr<Game> Game::load(std::unique_ptr<Vfs> v) {
    auto g = std::make_shared<Game>();
    g->vfs = std::move(v);
    Vfs& fsx = *g->vfs;
    if (!fsx.exists("game.json")) throw Error(fsx.describe() + " is not a Stillframe game (no game.json at its root)");
    try { g->manifest = Json::parse(fsx.read_text("game.json")); }
    catch (const Error& e) { fail("game.json", e.what()); }
    const Json& m = g->manifest;
    if (m["format"].as_str() != "stillgame") fail("game.json", "\"format\" must be \"stillgame\"");
    if (m["version"].as_int() != kGameFormat) fail("game.json", "unsupported game format version " + std::to_string(m["version"].as_int()) + " (this engine reads version " + std::to_string(kGameFormat) + ")");
    g->id = m["id"].as_str();
    if (g->id.empty()) fail("game.json", "\"id\" is required (for example \"com.yourname.mygame\")");
    for (char c : g->id) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_')) fail("game.json", "\"id\" may only contain letters, digits, '.', '-' and '_'");
    g->title = m["title"].str_or(g->id);
    g->author = m["author"].as_str();
    g->version = m["game_version"].str_or("0.0.0");
    g->start_label = m["start"].str_or("start");
    g->save_format = static_cast<int>(m["save_format"].as_int(1));
    // engine requirement: ">=2" or ">=2.1" or "2"
    std::string req = m["engine"].as_str();
    if (!req.empty()) {
        std::string digits = req;
        while (!digits.empty() && !std::isdigit(static_cast<unsigned char>(digits[0]))) digits.erase(0, 1);
        int major = std::atoi(digits.c_str());
        if (major > kEngineMajor) fail("game.json", "this game needs Stillframe " + req + " but this is engine " + std::to_string(kEngineMajor) + ".x - please update the engine");
    }
    if (m.has("canvas") && !(m["canvas"].is_array() && m["canvas"].size() == 2))
        fail("game.json", "\"canvas\" must be two sizes like [1280, 720]");
    if (m["canvas"].is_array()) {
        g->canvas_w = static_cast<int>(m["canvas"].at(0).as_int(1280));
        g->canvas_h = static_cast<int>(m["canvas"].at(1).as_int(720));
        if (g->canvas_w < 160 || g->canvas_h < 90 || g->canvas_w > 8192 || g->canvas_h > 8192) fail("game.json", "\"canvas\" must be two sizes like [1280, 720]");
    }
    if (m["variables"].is_object()) g->initial_vars = m["variables"];
    if (m["llm"].is_object()) g->llm = m["llm"];
    if (m["theme"].is_string()) {
        try { g->theme = Json::parse(fsx.read_text(join_path("", m["theme"].as_str()))); }
        catch (const Error& e) { fail(m["theme"].as_str(), e.what()); }
    } else if (m["theme"].is_object()) g->theme = m["theme"];

    g->positions = {{"far_left", 0.10}, {"left", 0.22}, {"center", 0.50}, {"right", 0.78}, {"far_right", 0.90}};
    for (auto& kv : m["positions"].members()) g->positions[kv.first] = kv.second.as_num(0.5);

    // assets: explicit maps, plus every image/audio file in the conventional folders (id = file name without extension)
    auto files = fsx.list();
    for (auto& f : files) {
        if (starts_with(f, "backgrounds/") && is_image(f)) g->backgrounds[stem(f)] = f;
        if (starts_with(f, "audio/") && is_audio(f)) g->audio[stem(f)] = f;
    }
    for (auto& kv : m["backgrounds"].members()) g->backgrounds[kv.first] = normalize_path(kv.second.as_str());
    for (auto& kv : m["audio"].members()) g->audio[kv.first] = normalize_path(kv.second.as_str());

    for (auto& f : files) {
        if (starts_with(f, "characters/") && ends_with(f, ".json")) {
            CharacterDef c;
            try { c.raw = Json::parse(fsx.read_text(f)); }
            catch (const Error& e) { fail(f, e.what()); }
            c.id = c.raw["id"].as_str();
            if (c.id.empty()) fail(f, "\"id\" is required");
            if (c.id != stem(f)) fail(f, "\"id\" (" + c.id + ") must match the file name (" + stem(f) + ".json)");
            if (c.id == "narrator") fail(f, "'narrator' is reserved");
            c.name = c.raw["name"].str_or(c.id);
            c.color = c.raw["color"].str_or("#ffffff");
            const Json& im = c.raw["immutable"];
            if (im.is_array()) {
                for (auto& e : im.items()) {
                    if (e.is_string()) c.immutable.emplace_back("", e.as_str());
                    else c.immutable.emplace_back(e["label"].as_str(), e["text"].as_str());
                }
            } else if (im.is_object()) {
                for (auto& kv : im.members()) c.immutable.emplace_back(kv.first, kv.second.as_str());
            }
            const Json& sp = c.raw["sprites"];
            for (auto& kv : sp["poses"].members()) c.poses[kv.first] = join_path(dir_of(f), kv.second.as_str());
            c.default_pose = sp["default_pose"].as_str();
            if (c.default_pose.empty() && !c.poses.empty()) c.default_pose = c.poses.begin()->first;
            c.scale = sp["scale"].as_num(1.0);
            c.anchor_x = sp["anchor"].at(0).as_num(0.5);
            c.anchor_y = sp["anchor"].at(1).as_num(1.0);
            c.y_offset = sp["y_offset"].as_num(0.0);
            g->characters[c.id] = std::move(c);
        }
        if (ends_with(f, "/minigame.json") && starts_with(f, "minigames/")) {
            MinigameDef d;
            try { d.manifest = Json::parse(fsx.read_text(f)); }
            catch (const Error& e) { fail(f, e.what()); }
            d.dir = dir_of(f);
            d.id = d.manifest["id"].str_or(stem(d.dir));
            d.title = d.manifest["title"].str_or(d.id);
            g->minigames[d.id] = std::move(d);
        }
    }

    // scripts
    std::vector<SourceFile> sources;
    std::vector<std::string> roots;
    if (m["scripts"].is_array()) for (auto& s : m["scripts"].items()) roots.push_back(normalize_path(s.as_str()));
    else roots.push_back("scripts");
    for (auto& f : files) {
        if (!ends_with(f, ".sf")) continue;
        bool in = false;
        for (auto& r : roots) if (f == r || starts_with(f, r + "/")) in = true;
        if (in) sources.push_back({f, fsx.read_text(f)});
    }
    if (sources.empty()) throw Error("the game has no scripts (put .sf files in scripts/)");
    g->program = std::make_shared<const Program>(Program::compile(sources));
    if (!g->program->labels.count(g->start_label)) fail("game.json", "start label '" + g->start_label + "' does not exist in the scripts");
    return g;
}

std::string Game::resolve_background(const std::string& id) const {
    auto it = backgrounds.find(id);
    if (it != backgrounds.end()) return it->second;
    return vfs->exists(id) ? normalize_path(id) : "";
}
std::string Game::resolve_audio(const std::string& id_or_path) const {
    auto it = audio.find(id_or_path);
    if (it != audio.end()) return it->second;
    try { return vfs->exists(id_or_path) ? normalize_path(id_or_path) : ""; } catch (const Error&) { return ""; }
}
double Game::position(const std::string& name) const {
    auto it = positions.find(name);
    if (it != positions.end()) return it->second;
    char* end = nullptr;
    double v = std::strtod(name.c_str(), &end);
    return (end && *end == 0 && !name.empty() && v >= 0 && v <= 1) ? v : -1;
}

std::vector<Issue> Game::validate() const {
    std::vector<Issue> out;
    auto err = [&](const std::string& w, const std::string& m) { out.push_back({Issue::Error, w, m}); };
    auto warn = [&](const std::string& w, const std::string& m) { out.push_back({Issue::Warning, w, m}); };
    auto exact = [&](const std::string& path) -> bool {
        // exact-case check: games developed on Windows break on Linux/macOS/zip if case differs
        if (!vfs->exists(path)) return false;
        auto l = vfs->list();
        return std::find(l.begin(), l.end(), normalize_path(path)) != l.end();
    };
    auto& R = program->refs;
    std::set<std::string> known_trans = {"none", "fade", "dissolve", "slide_left", "slide_right", "wipe_left", "wipe_right", "flash", "cut"};

    for (auto& c : R.characters) if (!characters.count(c)) err("scripts", "character '" + c + "' is used but characters/" + c + ".json does not exist");
    for (auto& c : R.speakers) if (!characters.count(c)) err("scripts", "speaker '" + c + "' has no characters/" + c + ".json (write narrator \"...\" for narration, or \"Name\" \"...\" for a one-off speaker)");
    for (auto& cp : R.poses) {
        auto c = character(cp.first);
        if (c && !c->poses.count(cp.second)) err("scripts", "character '" + cp.first + "' has no pose '" + cp.second + "'");
    }
    for (auto& b : R.backgrounds) {
        std::string p = resolve_background(b);
        if (p.empty()) err("scripts", "background '" + b + "' not found (put backgrounds/" + b + ".png in the game)");
        else if (!exact(p)) warn(p, "file name differs in letter case from what exists on disk; this breaks on case-sensitive systems");
    }
    for (auto& a : R.music) if (resolve_audio(a).empty()) err("scripts", "music '" + a + "' not found");
    for (auto& a : R.sounds) if (resolve_audio(a).empty()) err("scripts", "sound '" + a + "' not found");
    for (auto& p : R.positions) if (position(p) < 0) err("scripts", "unknown stage position '" + p + "' (use left, center, right, far_left, far_right, a number from 0 to 1, or define it under \"positions\" in game.json)");
    for (auto& t : R.transitions) if (!known_trans.count(t)) err("scripts", "unknown transition '" + t + "'");
    for (auto& mg : R.minigames) if (!minigames.count(mg)) err("scripts", "minigame '" + mg + "' is used but minigames/" + mg + "/minigame.json does not exist");
    for (auto& kv : characters) {
        const CharacterDef& c = kv.second;
        std::string where = "characters/" + c.id + ".json";
        if (c.immutable.empty()) warn(where, "no \"immutable\" memory: the character has no fixed identity for the model to rely on");
        if (c.poses.empty()) warn(where, "no sprite poses; the character will be invisible on stage");
        for (auto& p : c.poses) {
            if (!vfs->exists(p.second)) err(where, "pose '" + p.first + "' image missing: " + p.second);
            else if (!exact(p.second)) warn(where, "image path differs in letter case from the file on disk: " + p.second);
        }
        for (auto& kv : c.raw["pose_requires"].members()) {
            if (!c.poses.count(kv.first)) err(where, "pose_requires names unknown pose '" + kv.first + "'");
            try { parse_expr(kv.second.as_str()); } catch (const Error& e) { err(where, "pose_requires." + kv.first + ": " + e.what()); }
        }
        if (!c.poses.empty() && !c.poses.count(c.default_pose)) err(where, "default_pose '" + c.default_pose + "' is not one of the poses");
    }
    // talk blocks
    for (auto& in : program->code) {
        if (in.op != Op::Talk) continue;
        std::string where = in.file + ":" + std::to_string(in.line);
        for (auto& c : in.talk->cast) if (!characters.count(c)) err(where, "talk with unknown character '" + c + "'");
        if (llm.is_object() && llm["enabled"].is_bool() && !llm["enabled"].as_bool()) warn(where, "talk block present but \"llm.enabled\" is false");
    }
    // minigame launch descriptions
    for (auto& kv : minigames) {
        const Json& l = kv.second.manifest["launch"];
        std::string where = kv.second.dir + "/minigame.json";
        if (!l.is_object() || l["command"].size() == 0) err(where, "\"launch.command\" must list the program to run, for example [\"python\", \"main.py\"]");
        if (l["type"].str_or("process") != "process") err(where, "unsupported launch type '" + l["type"].as_str() + "' (only \"process\" is supported)");
    }
    return out;
}

std::vector<GameEntry> scan_games(const std::string& dir) {
    std::vector<GameEntry> out;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return out;
    std::vector<fs::path> paths;
    for (auto& e : fs::directory_iterator(dir, ec)) paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    for (auto& p : paths) {
        bool is_dir = fs::is_directory(p, ec);
        if (!is_dir && p.extension() != ".stillgame") continue;
        try {
            auto vfs = Vfs::open(p.string());
            if (!vfs->exists("game.json")) continue;
            Json m = Json::parse(vfs->read_text("game.json"));
            out.push_back({p.string(), m["id"].as_str(), m["title"].str_or(m["id"].as_str()), m["author"].as_str(), m["game_version"].str_or("0.0.0")});
        } catch (const std::exception&) { /* not a valid game; ignore here, `stillframe check` reports why */ }
    }
    return out;
}

} // namespace sf
