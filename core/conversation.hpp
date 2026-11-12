// A `talk` block, run live: the player speaks, the character answers through the model, her memory
// changes only through the operations she returns, and she may pull another character into the
// conversation. Nothing here blocks: the game calls player_say() and then poll() every frame.
#pragma once
#include <set>
#include <map>
#include "game.hpp"
#include "llm.hpp"
#include "memory.hpp"
#include <map>

namespace sf {

struct ConvOptions {
    std::string premise;                 // one paragraph about the world, from the game manifest
    std::string player_name = "the player";
    std::string tmpl = "chatml";         // "chatml" | "plain"
    std::string scene;                   // current script label, stored with memories
    int max_tokens = 360;
    double temperature = 0.8;
    int64_t seed = -1;                   // >= 0 makes a run reproducible (the turn number is added)
    int slots = 1;                       // llama-server slots available; characters are spread over them
    int interject_limit = 3;             // per talk block; a nomination spends one even if she declines
    int interject_cooldown = 1;          // player messages that must pass between two interjections
    std::vector<std::string> repair_words;   // empty = built-in English/Spanish list
    std::string language;                // e.g. "Spanish"; empty = same as the player
    std::map<std::string, std::set<std::string>> blocked_poses;   // speaker -> poses she may not use right now (character "pose_requires")
    static ConvOptions from_game(const Game& g, const Json& talk);
};

struct ConvEvent { std::string type; Json data; };
//  thinking  {speaker}                         the model was asked
//  delta     {speaker, text}                   her reply so far (full text each time), for typing it out
//  reply     {speaker, text, pose, gesture, kind: reply|interjection|refusal, memory:[{ok,op,id,reason}]}
//  offer     {outcome, label}                  she thinks the conversation reached an ending (commit: ask)
//  done      {outcome, reason}                 reason: committed | ended_by_her | left | turn_limit
//  declined  {speaker}                         a named character chose not to cut in (diagnostic)
//  error     {message}                         model unreachable etc.; the conversation stays open, retry() works

class Conversation {
public:
    Conversation(const Game& game, MemoryHub& hub, LlmScheduler& sched, const Json& talk_event, ConvOptions opt);
    Json schema_for(const std::string& speaker, bool interjection) const;   // the JSON schema the model must follow
    bool pose_blocked(const std::string& who, const std::string& pose) const;

    bool player_say(const std::string& text, const std::string& to = "");   // false while she is still answering
    bool retry();                          // after an "error": ask again, without recording the line twice
    void leave();                          // the player walks away
    bool commit(const std::string& outcome);
    void poll(std::vector<ConvEvent>& out);

    bool busy() const;                     // a reply is being generated (interjections don't count)
    bool finished() const { return finished_; }
    int turns() const { return turns_; }
    int interjections_used() const { return interj_used_; }
    const std::string& addressee() const { return addressee_; }
    const std::vector<std::string>& cast() const { return cast_; }

private:
    enum class Kind { Reply, Interject };
    struct Job {
        Kind kind; std::string speaker, player_text, about; size_t emitted = 0; uint64_t turn = 0;
        std::string note;      // extra instruction for this one turn (a grudge to judge, why she was named)
    };
    struct Outcome { std::string id, label, rule; };

    void submit_reply(const std::string& speaker, const std::string& text, const std::string& note);
    void submit_interjection(const std::string& who, const std::string& about, const std::string& by);
    void on_done(uint64_t id, const std::string& text);
    void emit_reply(const std::string& speaker, const Json& j, const std::string& kind, const Job& job, std::vector<ConvEvent>& out);
    void finish(const std::string& outcome, const std::string& reason, std::vector<ConvEvent>& out);
    bool looks_like_repair(const std::string& text) const;
    std::string display_name(const std::string& id) const;
    LlmRequest request_for(const std::string& speaker, const Job& job, bool interjection) const;

    const Game& game_;
    MemoryHub& hub_;
    LlmScheduler& sched_;
    ConvOptions opt_;
    std::vector<std::string> cast_;
    std::string addressee_, goal_, commit_, on_end_;
    int max_turns_ = 0, min_turns_ = 1;
    bool interject_allowed_ = true;
    int limit_ = 3;
    std::vector<Outcome> outcomes_;

    int turns_ = 0, interj_used_ = 0, last_interject_turn_ = -100;
    std::string last_interjector_, offered_, last_text_, last_to_;
    bool finished_ = false;
    std::map<uint64_t, Job> jobs_;
    std::vector<ConvEvent> pending_;   // produced outside poll() (refusals, leave)
};

// Exposed for tests and tools.
std::string wrap_prompt(const std::string& tmpl, const std::string& system, const std::string& user);
bool offers_repair(const std::string& text, const std::vector<std::string>& words);

} // namespace sf
