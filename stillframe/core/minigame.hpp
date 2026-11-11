// Minigames are separate programs. The engine starts one, tells it what the story knows, and waits for
// a result. It never links against them, so a minigame can be written in anything (Python, Godot,
// Love2D, a Unity build, a browser page behind a tiny launcher, ...) and a crash cannot take the
// story down with it.
//
// Protocol: UTF-8 JSON, one object per line, over the child's stdin/stdout. Lines on stdout that do
// not start with '{' are treated as the program's own logging and ignored.
//
//   engine -> game   {"type":"start","protocol":1,"game":"<id>","args":{...},"context":{...}}
//                    {"type":"say_result","id":N,"text":"..."}     answer to a "say" request
//                    {"type":"cancel"}                              please stop; result is no longer needed
//   game -> engine   {"type":"ready"}                               sent once the program is up
//                    {"type":"progress","value":0..1,"label":"..."} optional, for a loading/score hint
//                    {"type":"event","name":"...","data":{...}}     optional, logged and forwarded to the UI
//                    {"type":"say","id":N,"character":"hana","prompt":"..."}
//                                                                   ask a story character for one line
//                    {"type":"result","outcome":"win","data":{...}} the end; `outcome` picks the story path
#pragma once
#include "game.hpp"
#include "process.hpp"
#include <chrono>

namespace sf {

constexpr int kMinigameProtocol = 1;

struct MgEvent { std::string type; Json data; };
//  ready     {}
//  progress  {value, label}
//  event     {name, data}
//  say       {id, character, prompt}     answer with answer_say()
//  result    {outcome, data}             the minigame is over; this is what the script receives
//  failed    {message}                   it crashed, never started, spoke nonsense or timed out

class MinigameSession {
public:
    // Prepares the files (extracting from a .stillgame archive into `cache_dir` when needed), starts the
    // process and sends "start". Throws Error with a readable message if it cannot be started.
    static std::unique_ptr<MinigameSession> launch(const Game& game, const std::string& id, const Json& args,
                                                   const Json& context, const std::string& cache_dir);
    ~MinigameSession();

    void poll(std::vector<MgEvent>& out);                    // never blocks
    void answer_say(int64_t id, const std::string& text);
    void cancel();                                           // asks politely, then ends the process
    bool finished() const { return finished_; }

    const std::string& id() const { return id_; }
    const std::string& dir() const { return dir_; }
    // What the script receives if the player can't or won't retry after a failure.
    std::string fallback_outcome() const { return fallback_; }

private:
    MinigameSession() = default;
    void fail(const std::string& msg, std::vector<MgEvent>& out);

    std::unique_ptr<Process> proc_;
    std::string id_, dir_, fallback_ = "aborted";
    std::vector<std::string> outcomes_;      // declared in the manifest; empty = anything goes
    bool ready_ = false, finished_ = false, cancelled_ = false;
    std::chrono::steady_clock::time_point started_;
    int startup_timeout_ms_ = 20000, timeout_ms_ = 0;
};

// Folder holding the minigame's files, ready to run. Archives are extracted once per game version.
std::string materialize_minigame(const Game& game, const MinigameDef& def, const std::string& cache_dir);

// The command line for this platform, with {dir} replaced.
std::vector<std::string> minigame_command(const MinigameDef& def, const std::string& dir);

// Value for the script variable: {"outcome": ..., "data": {...}}
Json minigame_result_value(const std::string& outcome, const Json& data);

} // namespace sf
