// The picture behind the text: background, character sprites, motions and scene transitions.
// It consumes the VM's presentation events and knows nothing about the user interface.
#pragma once
#include "theme.hpp"
#include "../core/vm.hpp"

namespace sf {

constexpr const char* kMotionNames[] = {"breathe", "nod", "shake_head", "tilt", "emphasize", "chuckle", "bashful", "reassure", "sigh", "glance", "hop", "bob", "shake"};
bool is_known_motion(const std::string& name);
bool is_known_transition(const std::string& name);

class Stage {
public:
    Stage(const Game& game, AssetCache& assets, const Theme& theme);

    // Applies one presentation event (scene, show, hide, pose, anim, with). Others are ignored.
    void apply(const Event& e);
    // Call after a batch of events: changes that were not followed by a `with` appear instantly.
    void end_batch();
    void update(double dt);
    void render(Canvas& out);

    bool busy() const;                  // a transition, motion or pose fade is running (needs frames)
    bool transitioning() const { return trans_active_; }
    void finish_transition();           // skip to the end (player pressed skip)
    void set_speaker(const std::string& id);
    std::vector<std::string> present() const;
    const std::string& background() const { return bg_; }

    Json save() const;
    void load(const Json& j);

private:
    struct Sprite {
        std::string id, pose, prev_pose;
        double pose_t = 1.0;            // 0..1 crossfade from prev_pose
        double x = 0.5;                 // stage position, fraction of canvas width
        double bright = 1.0, bright_target = 1.0;
        std::string motion; double motion_t = 0, motion_len = 0;
        int order = 0;
    };
    void render_scene(Canvas& out, double t_now);
    void begin_change();
    void start_transition(const std::string& kind, double seconds);
    Sprite* find(const std::string& id);
    void draw_sprite(Canvas& out, Sprite& s);

    const Game& game_;
    AssetCache& assets_;
    const Theme& theme_;
    int w_, h_;
    std::string bg_;
    std::vector<Sprite> sprites_;
    int next_order_ = 1;
    double clock_ = 0;
    std::string speaker_;

    // transitions
    std::unique_ptr<Canvas> snap_, live_, from_, scratch_, black_;
    bool snap_taken_ = false, trans_active_ = false;
    std::string trans_kind_;
    double trans_t_ = 0, trans_len_ = 0;
};

} // namespace sf
