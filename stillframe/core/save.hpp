// Per-game saves live in the user's data folder, never inside the game. Updating or replacing a game
// (or the engine) keeps player progress.
#pragma once
#include "json.hpp"

namespace sf {

struct SaveInfo { std::string slot; int64_t time = 0; std::string label, summary; bool ok = false; };

class SaveStore {
public:
    SaveStore(std::string root_dir, std::string game_id, int save_format, std::string game_version);
    std::string dir() const { return dir_; }

    void write(const std::string& slot, const Json& state, const std::string& summary = "");
    // Throws Error with a human message if the slot is missing, corrupt, or from an incompatible game version.
    Json read(const std::string& slot) const;
    bool exists(const std::string& slot) const;
    void remove(const std::string& slot);
    std::vector<SaveInfo> list() const;

    Json read_settings() const;
    void write_settings(const Json& s);

    static bool valid_slot(const std::string& slot);   // "auto", "quick", "1".."99"
private:
    std::string dir_, game_id_, game_version_;
    int format_;
    std::string path(const std::string& slot) const;
};

} // namespace sf
