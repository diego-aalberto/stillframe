#include "save.hpp"
#include "platform.hpp"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace sf {

static std::string safe_dir_name(const std::string& id) {
    std::string r;
    for (char c : id) r += (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') ? c : '_';
    return r;
}

SaveStore::SaveStore(std::string root, std::string game_id, int fmt, std::string ver)
    : dir_((fs::path(root) / "saves" / safe_dir_name(game_id)).string()), game_id_(std::move(game_id)), game_version_(std::move(ver)), format_(fmt) {}

bool SaveStore::valid_slot(const std::string& s) {
    if (s == "auto" || s == "quick") return true;
    if (s.empty() || s.size() > 2) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); }) && s != "0" && s != "00";
}

std::string SaveStore::path(const std::string& slot) const {
    if (!valid_slot(slot)) throw Error("invalid save slot '" + slot + "'");
    return (fs::path(dir_) / ("slot-" + slot + ".json")).string();
}

void SaveStore::write(const std::string& slot, const Json& state, const std::string& summary) {
    Json doc = Json::object();
    doc.set("format", "stillframe.save");
    doc.set("version", 1);
    doc.set("game", game_id_);
    doc.set("game_version", game_version_);
    doc.set("save_format", format_);
    doc.set("time", unix_time());
    doc.set("summary", summary);
    doc.set("state", state);
    write_file_atomic(path(slot), doc.dump(1));
}

Json SaveStore::read(const std::string& slot) const {
    std::string p = path(slot);
    auto load = [&](const std::string& file) {
        Json doc = Json::parse(read_file(file));
        if (doc["format"].as_str() != "stillframe.save") throw Error("not a Stillframe save");
        if (doc["game"].as_str() != game_id_) throw Error("this save belongs to a different game");
        if (doc["save_format"].as_int(1) != format_)
            throw Error("this save was made by an incompatible version of the game (save format " + std::to_string(doc["save_format"].as_int(1)) + ", game uses " + std::to_string(format_) + ")");
        return doc["state"];
    };
    if (!file_exists(p) && !file_exists(p + ".bak")) throw Error("no save in slot " + slot);
    try { return load(p); }
    catch (const Error& first) {
        // a damaged primary file falls back to the previous good write
        if (file_exists(p + ".bak")) {
            try { return load(p + ".bak"); } catch (const Error&) {}
        }
        throw Error("save slot " + slot + " cannot be loaded: " + first.what());
    }
}

bool SaveStore::exists(const std::string& slot) const { return file_exists(path(slot)); }

void SaveStore::remove(const std::string& slot) {
    std::error_code ec;
    fs::remove(path(slot), ec);
    fs::remove(path(slot) + ".bak", ec);
}

std::vector<SaveInfo> SaveStore::list() const {
    std::vector<SaveInfo> out;
    std::error_code ec;
    if (!fs::is_directory(dir_, ec)) return out;
    for (auto& e : fs::directory_iterator(dir_, ec)) {
        std::string n = e.path().filename().string();
        if (!starts_with(n, "slot-") || !ends_with(n, ".json")) continue;
        SaveInfo si;
        si.slot = n.substr(5, n.size() - 5 - 5);
        if (!valid_slot(si.slot)) continue;
        try {
            Json doc = Json::parse(read_file(e.path().string()));
            si.time = doc["time"].as_int();
            si.summary = doc["summary"].as_str();
            si.ok = doc["game"].as_str() == game_id_ && doc["save_format"].as_int(1) == format_;
        } catch (const Error&) { si.ok = false; }
        out.push_back(si);
    }
    std::sort(out.begin(), out.end(), [](const SaveInfo& a, const SaveInfo& b) { return a.time > b.time; });
    return out;
}

Json SaveStore::read_settings() const {
    std::string p = (fs::path(dir_) / "settings.json").string();
    try { return file_exists(p) ? Json::parse(read_file(p)) : Json::object(); }
    catch (const Error&) { return Json::object(); }
}
void SaveStore::write_settings(const Json& s) { write_file_atomic((fs::path(dir_) / "settings.json").string(), s.dump(1)); }

} // namespace sf
