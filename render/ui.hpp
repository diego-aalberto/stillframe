// Reusable pieces of the visual novel interface: panels, buttons, name tags, a text input.
#pragma once
#include "theme.hpp"

namespace sf {

struct TextInput {
    std::string text;
    size_t cursor = 0;                  // byte offset, always on a code point boundary
    size_t max_bytes = 600;
    void insert(const std::string& s);
    void backspace();
    void del();
    void left();
    void right();
    void home() { cursor = 0; }
    void end() { cursor = text.size(); }
    void clear() { text.clear(); cursor = 0; }
};

void draw_panel(Canvas& c, const Theme& t, const Rect& r, float radius, bool shadow = true);
void draw_button(Canvas& c, const Theme& t, const Rect& r, const std::string& label, float px, bool hover, bool enabled = true, bool primary = false);
// Pill with the speaker's name; returns its rectangle.
Rect draw_name_tag(Canvas& c, const Theme& t, float x, float y, const std::string& name, Color color, float px);
void draw_thinking_dots(Canvas& c, const Rect& r, double time, Color col);
void draw_caret(Canvas& c, float x, float y0, float y1, double time, Color col);
// Text clipped with an ellipsis if it doesn't fit.
std::string ellipsize(const FontSet& f, const std::string& s, float px, float max_w);

} // namespace sf
