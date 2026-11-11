// Runs one game: story script -> stage + interface, talk blocks through the model, minigames, saves.
// The player is driven entirely by handle()/update()/render(), so the same code runs in a window
// (SDL), in a headless screenshot tool, and in tests.
#pragma once
#include "stage.hpp"
#include "ui.hpp"
#include "../core/conversation.hpp"
#include "../core/minigame.hpp"
#include "../core/save.hpp"
#include <deque>
#include <set>

namespace sf {

struct Input {
    enum Type { MouseMove, MouseDown, MouseUp, Wheel, Key, Text } type = MouseMove;
    float x = 0, y = 0;                 // canvas coordinates
    int wheel = 0;                      // positive = up
    std::string key;                    // "enter" "space" "escape" "backspace" "delete" "left" "right" "up" "down" "home" "end" "tab" "f5" "f9" "h"
    bool ctrl = false, shift = false;
    std::string text;                   // UTF-8, for Text
};

// Sound output is the platform's job; the player only says what should play.
class AudioSink {
public:
    virtual ~AudioSink() = default;
    virtual void music(const std::string& file_in_package, double fade, bool loop) = 0;
    virtual void stop_music(double fade) = 0;
    virtual void sound(const std::string& file_in_package) = 0;
    virtual void set_volumes(double music, double sfx) { (void)music; (void)sfx; }
};

struct PlayerOptions {
    std::string data_dir;                       // saves/settings root; empty = user_data_dir()
    std::string cache_dir;                      // extracted minigames etc.; empty = <data_dir>/cache
    std::string llm_url;                        // overrides game.json "llm.server"
    std::shared_ptr<LlmBackend> llm;            // tests inject a ScriptedBackend here
    AudioSink* audio = nullptr;
    std::string player_name;                    // overrides the "player" variable in prompts
    bool autosave = true;
    int64_t seed = -1;
};

class Player {
public:
    Player(std::shared_ptr<Game> game, PlayerOptions opt = {});
    ~Player();

    void handle(const Input& in);
    void update(double dt);
    void render(Canvas& out);

    bool needs_frame() const;                    // keep calling update/render at full rate while true
    bool quit_requested() const { return quit_; }
    std::string mode_name() const;               // "say" "menu" "talk" "minigame" "pause" "ended" "error" "menu_overlay" ...
    const Json& vars() const { return vm_.vars(); }
    bool save_slot(const std::string& slot);
    bool load_slot(const std::string& slot);
    std::vector<std::string> take_messages();    // problems worth showing in a log (missing art, model errors)
    const std::vector<std::string>& history_text() const { return hist_text_; }

    Vm& vm() { return vm_; }
    Conversation* conversation() { return conv_.get(); }
    MemoryHub& memory() { return hub_; }

private:
    enum class Mode { Running, Say, Menu, Talk, Minigame, Pause, Ended, Error, MenuOverlay, SaveLoad, History };
    struct Line {
        std::string speaker, name, text, kind;      // kind: say | narration | reply | interjection | refusal
        Color color{255, 255, 255, 255};
        double shown = 0;                           // code points revealed
        size_t total = 0;
        bool full() const { return shown >= static_cast<double>(total); }
    };
    struct Layout;

    // flow
    void run_script();
    void process_events();
    void present_wait();
    void show_line(Line l);
    void start_talk(const Json& ev);
    void start_minigame(const Json& ev);
    void pump_talk(double dt);
    void pump_minigame();
    void finish_talk(const std::string& outcome);
    void show_error(const std::string& title, const std::string& detail, bool can_retry);
    void toast(const std::string& text);
    void autosave();
    void log_history(const Line& l);
    LlmBackend* backend();
    void answer_minigame_say(int64_t id, const std::string& character, const std::string& prompt);

    // input / drawing
    Layout layout() const;
    void click(float x, float y);
    void key(const Input& in);
    void advance();
    std::string tr(const std::string& key) const;
    float px_text() const; float px_name() const; float px_small() const;
    void draw_line_box(Canvas& c, const Layout& L);
    void draw_menu(Canvas& c, const Layout& L);
    void draw_talk_extras(Canvas& c, const Layout& L);
    void draw_overlays(Canvas& c, const Layout& L);
    void draw_toasts(Canvas& c);
    Color color_of(const std::string& speaker) const;
    std::string name_of(const std::string& speaker, const Json& ev) const;

    std::shared_ptr<Game> game_;
    PlayerOptions opt_;
    AssetCache assets_;
    Theme theme_;
    Stage stage_;
    Vm vm_;
    MemoryHub hub_;
    SaveStore saves_;
    std::map<std::string, std::string> strings_;
    int w_, h_;
    double clock_ = 0;
    bool quit_ = false, skipping_ = false;

    Mode mode_ = Mode::Running, return_mode_ = Mode::Running;
    std::deque<Event> pending_;
    Line line_;
    bool has_line_ = false;
    Json menu_;                                      // current menu event
    double pause_left_ = 0;
    std::string ending_;
    std::string error_title_, error_detail_, error_kind_;
    bool error_retry_ = false;
    std::string music_;

    // talk
    std::shared_ptr<LlmBackend> backend_;
    std::unique_ptr<LlmScheduler> sched_;
    std::unique_ptr<Conversation> conv_;
    Json talk_event_;
    std::deque<Line> talk_queue_;
    bool talk_thinking_ = false, talk_done_ = false, streaming_ = false;
    std::string talk_done_outcome_, player_echo_;
    std::string talk_thinking_who_, offered_outcome_, offered_label_;
    TextInput input_;
    std::string backend_error_, pending_chip_;   // pending_chip_: addressee chosen with a click/Tab, used for the next message

    // minigame
    std::unique_ptr<MinigameSession> mg_;
    Json mg_event_;
    double mg_progress_ = 0;
    std::string mg_label_;
    std::map<int64_t, uint64_t> mg_say_jobs_;       // minigame say id -> scheduler job
    std::map<uint64_t, int64_t> mg_say_by_job_;

    // overlays
    float mx_ = -1, my_ = -1;
    double history_scroll_ = 0;
    bool save_mode_ = true;
    double text_speed_ = 1.0;
    struct Toast { std::string text; double age = 0; };
    std::vector<Toast> toasts_;
    std::vector<std::string> hist_text_;
    struct Hist { std::string name, text; Color color; };
    std::deque<Hist> hist_;
    std::vector<std::string> messages_;
    bool dirty_ = true;
};

} // namespace sf
