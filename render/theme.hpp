// Everything a game can change about how it looks, from the "theme" object in game.json.
// Every field has a default, so a game with no theme still looks finished.
#pragma once
#include "assets.hpp"
#include "../core/game.hpp"

namespace sf {

struct Theme {
    // colours
    Color text{244, 244, 248, 255}, text_dim{176, 180, 196, 255}, narration{214, 218, 232, 255};
    Color box{14, 16, 28, 234}, box_border{255, 255, 255, 46};
    Color accent{255, 196, 128, 255};
    Color button{26, 30, 50, 232}, button_hover{58, 66, 104, 245}, button_border{255, 255, 255, 56};
    Color input_bg{8, 10, 20, 235};
    Color overlay{6, 8, 16, 190};
    Color good{132, 214, 160, 255}, warn{255, 164, 120, 255};
    // layout (fractions of the canvas unless noted)
    float margin = 0.028f, box_height = 0.27f, radius = 14.f;
    float text_size = 0.034f, name_size = 0.030f, small_size = 0.024f;   // of canvas height
    float sprite_height = 0.94f;                                         // of canvas height, before character scale
    // behaviour
    float chars_per_second = 70.f;
    bool idle_motion = false;
    bool box_blur_backdrop = false;
    // fonts
    FontSet fonts;       // body text
    FontSet fonts_bold;  // names and buttons

    static Theme from_game(const Game& g, AssetCache& assets);
    static Theme engine_default();     // for screens that exist outside any game (the library)
};

} // namespace sf
