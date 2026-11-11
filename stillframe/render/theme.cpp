#include "theme.hpp"

namespace sf {

Theme Theme::from_game(const Game& g, AssetCache& assets) {
    Theme t;
    const Json& j = g.theme;
    auto col = [&](const char* key, Color& c) { if (j["colors"][key].is_string()) c = Color::parse(j["colors"][key].as_str(), c); };
    col("text", t.text); col("text_dim", t.text_dim); col("narration", t.narration); col("box", t.box); col("box_border", t.box_border);
    col("accent", t.accent); col("button", t.button); col("button_hover", t.button_hover); col("button_border", t.button_border);
    col("input", t.input_bg); col("overlay", t.overlay); col("good", t.good); col("warn", t.warn);
    auto num = [&](const Json& v, float& f) { if (v.is_number()) f = static_cast<float>(v.as_num()); };
    num(j["layout"]["margin"], t.margin); num(j["layout"]["box_height"], t.box_height); num(j["layout"]["radius"], t.radius);
    num(j["layout"]["text_size"], t.text_size); num(j["layout"]["name_size"], t.name_size); num(j["layout"]["sprite_height"], t.sprite_height);
    num(j["chars_per_second"], t.chars_per_second);
    if (j["idle_motion"].is_bool()) t.idle_motion = j["idle_motion"].as_bool();

    auto add_font = [&](FontSet& set, const Json& spec, const char* engine_default) {
        std::string path = spec.str_or("");
        if (!path.empty()) {
            try { set.add(assets.font_from_package(path)); }
            catch (const Error& e) { assets.font_from_engine("Poppins-Regular.ttf"); (void)e; }
        }
        set.add(AssetCache::font_from_engine(engine_default));
        set.add(AssetCache::font_from_engine("DejaVuSans.ttf"));      // symbols and wider Unicode coverage
    };
    add_font(t.fonts, j["font"], "Poppins-Regular.ttf");
    add_font(t.fonts_bold, j["font_bold"].is_string() ? j["font_bold"] : j["font"], "Poppins-Medium.ttf");
    return t;
}

} // namespace sf
