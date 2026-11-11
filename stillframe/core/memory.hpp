// Character memory: three slots with different rules about who may change them.
//
//   IMMUTABLE   Who the character is (name, personality, quirks). Loaded from the game package.
//               There is no write path: the type only exposes const accessors, and nothing in an
//               operation can address it.
//   CONTEXTUAL  What she has come to know and feel about the player: facts, promises, grudges.
//               Changed ONLY by the character herself, through validated operations she returns
//               with her reply (add / update / resolve / forget). A grudge can make her refuse to
//               talk until the player repairs it.
//   SCENE       What she witnessed in the current scene. One buffer per character, flushed when the
//               scene changes, so memories of different characters never blend.
#pragma once
#include "game.hpp"
#include <deque>
#include <set>

namespace sf {

struct MemoryConfig {
    int contextual_max = 40;          // entries kept per character
    int scene_max_lines = 24;         // witnessed lines kept per character per scene
    int max_ops_per_turn = 3;
    int prompt_budget_chars = 2400;   // contextual memory included in a prompt
    int scene_budget_chars = 1800;    // scene memory included in a prompt
    int min_turns_to_resolve = 1;     // a grudge cannot be resolved on the turn it was made
    bool witness_present = true;      // everyone present witnesses a line (false: only speaker + addressee)
    static MemoryConfig from_json(const Json& j);
};

class ImmutableSlot {
public:
    explicit ImmutableSlot(std::vector<std::pair<std::string, std::string>> facts) : facts_(std::move(facts)) {}
    const std::vector<std::pair<std::string, std::string>>& facts() const { return facts_; }
    std::string render() const;
private:
    const std::vector<std::pair<std::string, std::string>> facts_;
};

struct MemoryEntry {
    std::string id, kind, text, status = "open";
    int severity = 1;
    bool refuses = false;
    std::string refusal_line, repair_hint, quote, scene;
    int64_t turn = 0, updated = 0;
    Json to_json() const;
    static MemoryEntry from_json(const Json& j);
};

struct SceneLine { std::string speaker, text; };

// One operation, exactly as the character returned it.
struct MemoryOp {
    std::string op, id, kind, text, evidence, refusal_line, repair_hint, note;
    int severity = 0;                 // 0 = not given
    int refuses = -1;                 // -1 = not given
    static std::vector<MemoryOp> parse(const Json& arr);
};

struct OpContext {
    int64_t turn = 0;
    std::string player_utterance;           // what the player just said
    std::vector<std::string> witnessed;     // lines she saw this scene (evidence for what others said)
    std::string scene;                      // scene label for bookkeeping
};

struct OpResult { bool ok; std::string op, id, reason; };

class CharacterMemory {
public:
    CharacterMemory(std::string id, std::shared_ptr<const ImmutableSlot> immutable, MemoryConfig cfg)
        : id_(std::move(id)), immutable_(std::move(immutable)), cfg_(cfg) {}

    const std::string& id() const { return id_; }
    const ImmutableSlot& immutable() const { return *immutable_; }
    const std::vector<MemoryEntry>& contextual() const { return contextual_; }
    const std::deque<SceneLine>& scene() const { return scene_; }

    // The only way contextual memory changes.
    std::vector<OpResult> apply(const std::vector<MemoryOp>& ops, const OpContext& ctx);

    // Scene slot.
    void witness(const std::string& speaker, const std::string& text);
    void begin_scene() { scene_.clear(); }

    // Willingness: "open", "guarded" (carries an unresolved grudge) or "refusing".
    std::string willingness() const;
    const MemoryEntry* blocking() const;              // the open grudge that makes her refuse, if any
    const MemoryEntry* find(const std::string& id) const;
    size_t open_count() const;

    // Prompt text.
    std::string render_contextual() const;
    std::string render_scene() const;

    Json save() const;
    void load(const Json& j);

private:
    std::string id_;
    std::shared_ptr<const ImmutableSlot> immutable_;
    MemoryConfig cfg_;
    std::vector<MemoryEntry> contextual_;
    std::deque<SceneLine> scene_;
    int64_t next_id_ = 1;
    MemoryEntry* mut(const std::string& id);
    bool make_room();
};

class MemoryHub {
public:
    MemoryHub(const Game& game, MemoryConfig cfg);
    CharacterMemory& of(const std::string& id);
    const CharacterMemory& of(const std::string& id) const;
    bool has(const std::string& id) const { return mem_.count(id) > 0; }
    // A new scene starts: every character forgets what she witnessed (contextual memory stays).
    void begin_scene();
    // A line was spoken in front of `present`. Each of them records it in her own scene slot.
    void witness(const std::string& speaker, const std::string& text, const std::vector<std::string>& present, const std::string& addressee = "", const std::string& label = "");
    const MemoryConfig& config() const { return cfg_; }
    int64_t turn = 0;   // global conversation turn counter (grudge timing); saved with the memories
    Json save() const;
    void load(const Json& j);
private:
    MemoryConfig cfg_;
    std::map<std::string, CharacterMemory> mem_;
};

bool contains_ci(const std::string& haystack, const std::string& needle);

} // namespace sf
