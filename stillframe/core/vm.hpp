// Virtual machine for compiled scripts. It never touches the screen or the model:
// it produces JSON "events" for a presentation layer and waits for the answer to blocking ones.
#pragma once
#include "script.hpp"

namespace sf {

struct Event { std::string type; Json data; };

/* Event types (see docs/PROTOCOL.md)
   non-blocking: scene, show, hide, pose, anim, with, music, stop_music, sound, notify, checkpoint
   blocking:     say, pause, menu, talk, minigame, end */
class Vm {
public:
    explicit Vm(std::shared_ptr<const Program> program, uint64_t seed = 0x5EED);

    void start(const std::string& label);
    // Runs until a blocking event (inclusive) is produced. Returns nothing while still waiting.
    std::vector<Event> run();

    bool waiting() const { return !wait_kind_.empty(); }
    const std::string& wait_kind() const { return wait_kind_; }
    const Json& wait_event() const { return wait_event_; }
    bool finished() const { return finished_; }

    void advance();                                   // say / pause
    void choose(int option_index);                    // menu (index from the event)
    void finish_talk(const std::string& outcome_id);  // talk ("" continues after the block)
    void finish_minigame(Json result);                // minigame

    Json& vars() { return vars_; }
    const Json& vars() const { return vars_; }
    EvalContext context();
    const Program& program() const { return *prog_; }
    std::string where() const;                        // "file:line" of the current instruction (for messages)

    Json save() const;
    // Returns false if the script changed since the save was made (positions are re-resolved by label).
    bool load(const Json& j);

private:
    std::shared_ptr<const Program> prog_;
    Json vars_ = Json::object();
    uint64_t rng_;
    int pc_ = 0;
    std::vector<int> stack_;
    std::set<std::string> seen_;
    std::map<int, std::string> label_at_;
    std::string wait_kind_;
    Json wait_event_;
    Json wait_data_;        // menu: enabled option map; talk: enabled outcome ids
    int wait_pc_ = -1;
    bool finished_ = false;

    void block_on(const std::string& kind, Json event, int pc, Json data = Json());
    void jump_to(int pc);
    void need(const char* kind) const;
    Json build_pos(int pc) const;
    int read_pos(const Json& j) const;
};

} // namespace sf
