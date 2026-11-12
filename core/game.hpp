// A game: everything the engine needs to run one title, loaded from a folder or .stillgame file.
// The engine is installed once; games are data that run on it.
#pragma once
#include "script.hpp"
#include "vfs.hpp"

namespace sf {

constexpr int kEngineMajor = 2;
constexpr int kGameFormat = 1;

struct Issue {
    enum Level { Warning, Error } level;
    std::string where, message;
    std::string str() const { return std::string(level == Error ? "error: " : "warning: ") + (where.empty() ? "" : where + ": ") + message; }
};

struct CharacterDef {
    std::string id, name, color = "#ffffff";
    // The IMMUTABLE memory slot. Loaded once from the game package, never written at runtime.
    std::vector<std::pair<std::string, std::string>> immutable;   // (label, text)
    std::map<std::string, std::string> poses;                     // pose id -> image path in package
    std::string default_pose;
    double scale = 1.0;
    double anchor_x = 0.5, anchor_y = 1.0;                        // sprite point placed at the stage position
    double y_offset = 0.0;                                        // fraction of canvas height
    Json raw;
};

struct MinigameDef {
    std::string id, title, dir;      // dir: folder inside the package that holds the minigame
    Json manifest;
};

class Game {
public:
    static std::shared_ptr<Game> load(std::unique_ptr<Vfs> vfs);   // throws Error with a readable message

    std::unique_ptr<Vfs> vfs;
    Json manifest;
    std::string id, title, author, version = "0.0.0", start_label = "start";
    int canvas_w = 1280, canvas_h = 720;
    int save_format = 1;
    Json initial_vars = Json::object();
    Json llm = Json::object();       // game-level model/memory/interjection settings
    Json theme = Json::object();
    std::map<std::string, CharacterDef> characters;
    std::map<std::string, std::string> backgrounds, audio;          // id -> path
    std::map<std::string, double> positions;                        // stage positions (fraction of canvas width)
    std::map<std::string, MinigameDef> minigames;
    std::shared_ptr<const Program> program;

    const CharacterDef* character(const std::string& id) const { auto it = characters.find(id); return it == characters.end() ? nullptr : &it->second; }
    std::string resolve_background(const std::string& id) const;
    std::string resolve_audio(const std::string& id_or_path) const;
    double position(const std::string& name) const;                 // < 0 if unknown

    // Cross-checks scripts against assets. Errors mean the game cannot run correctly.
    std::vector<Issue> validate() const;
};

// Lists games found in `dir` (sub-folders with a game.json, or *.stillgame files).
struct GameEntry { std::string path, id, title, author, version; };
std::vector<GameEntry> scan_games(const std::string& dir);

} // namespace sf
