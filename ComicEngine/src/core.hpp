#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace comic {
struct Vec2 {
    float x = 0, y = 0;
    Vec2 operator+(Vec2 b) const {
        return {x + b.x, y + b.y};
    }
    Vec2 operator-(Vec2 b) const {
        return {x - b.x, y - b.y};
    }
    Vec2 operator*(float k) const {
        return {x * k, y * k};
    }
};
struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
};
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
struct Glyph {
    float advance = 0;
};
struct Font {
    std::string name = "Comic";
    Image atlas;
    float cellWidth = 32, cellHeight = 40, lineHeight = 30;
    std::array<Glyph, 224> glyphs{};
};
struct Joint {
    std::string name;
    int parent = -1;
    Vec2 bind;
};
struct Key {
    float time = 0;
    std::vector<Vec2> points;
};
struct Clip {
    std::string name = "idle";
    float duration = 2;
    bool loop = true;
    std::vector<Key> keys;
};
struct Vertex {
    Vec2 bind, uv;
    uint16_t a = 0, b = 0;
    float weight = 1;
};
struct Layer {
    std::string id = "image";
    uint32_t image = 0;
    bool visible = true;
    Vec2 offset;
    float scale = 1;
    uint32_t grid = 16, active = 0;
    std::vector<Joint> joints;
    std::vector<Clip> clips;
    std::vector<Vertex> mesh;
    std::vector<Vec2> deformed;
    std::vector<uint32_t> indices;
    // Runtime-only clock; not serialized in .cscene files.
    float timeOrigin = 0;
};
struct Bubble {
    Color fill{255, 250, 231, 245}, edge{38, 34, 51, 255}, text{38, 34, 51, 255};
    float width = 290;
    Vec2 offset{0, -100};
};
struct Character {
    std::string id = "character", text = "Hello!";
    Vec2 position{400, 400};
    uint32_t font = 0;
    Bubble bubble;
    std::vector<Layer> layers;
};
struct Background {
    uint32_t image = 0;
    Vec2 position;
    Vec2 size{960, 640};
};
struct Scene {
    std::string title = "Untitled comic";
    uint32_t width = 960, height = 640;
    float poseRate = 12;
    Color clear{29, 32, 48, 255};
    std::vector<Image> images;
    std::vector<Font> fonts;
    std::vector<Background> backgrounds;
    std::vector<Character> characters;
};

void validate(const Scene &scene);
void buildMesh(Layer &layer, const Image &image);
std::vector<Vec2> sample(const Layer &layer, float seconds, float poseRate);
void deform(Layer &layer, const std::vector<Vec2> &points);
void prepare(Scene &scene);
void compactAssets(Scene &scene);
Scene loadScene(const std::filesystem::path &path);
void saveScene(const Scene &scene, const std::filesystem::path &path);
Image loadImage(const std::filesystem::path &path);
void saveImage(const Image &image, const std::filesystem::path &path);
Font loadFont(const std::filesystem::path &path);
void saveFont(const Font &font, const std::filesystem::path &path);
bool play(Scene &scene, const std::string &character, const std::string &layer, const std::string &clip);
bool say(Scene &scene, const std::string &character, const std::string &text);
std::string manifest(const Scene &scene);
Scene demo(Font font);
} // namespace comic
