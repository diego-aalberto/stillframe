#include "anime_demo.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace comic {
namespace {
constexpr float pi = 3.14159265358979323846f;
const char *poseNames[] = {"Standing", "Seated", "Kneeling"};
const char *clipNames[3][3] = {{"head_sway", "small_wave", "breathing_sway"},
                               {"leg_swing", "seated_nod", "ankle_bounce"},
                               {"curious_tilt", "palm_gesture", "gentle_bow"}};
const char *descriptions[3][3] = {
    {"A gentle head sway.", "A small hello with my hand.", "A quiet breath and body sway."},
    {"Swinging my legs from the bench.", "A little nod while I sit.", "Bouncing my feet to a rhythm."},
    {"A curious tilt of my head.", "Let me explain with a gesture.", "A small bow, then back again."}};

void rect(Image &image, int x, int y, int width, int height, Color c) {
    for (int row = std::max(0, y); row < std::min(int(image.height), y + height); ++row)
        for (int col = std::max(0, x); col < std::min(int(image.width), x + width); ++col) {
            size_t p = (size_t(row) * image.width + col) * 4;
            image.rgba[p] = c.r;
            image.rgba[p + 1] = c.g;
            image.rgba[p + 2] = c.b;
            image.rgba[p + 3] = c.a;
        }
}

void rotate(std::vector<Vec2> &points, const Layer &layer, int root, int pivot, float degrees) {
    float a = degrees * pi / 180.f, cs = std::cos(a), sn = std::sin(a);
    Vec2 center = points[size_t(pivot)];
    for (size_t j = 0; j < points.size(); ++j) {
        int ancestor = int(j);
        while (ancestor >= 0 && ancestor != root)
            ancestor = layer.joints[size_t(ancestor)].parent;
        if (ancestor != root)
            continue;
        Vec2 v = points[j] - center;
        points[j] = center + Vec2{v.x * cs - v.y * sn, v.x * sn + v.y * cs};
    }
}

void addClip(Layer &layer, int pose, int index, float imageScale) {
    Clip clip;
    clip.name = clipNames[pose][index];
    clip.duration = 3;
    clip.loop = true;
    std::vector<Vec2> bind;
    for (auto &joint : layer.joints)
        bind.push_back(joint.bind);
    for (int frame = 0; frame <= 24; ++frame) {
        float u = float(frame) / 24.f;
        float cycle = std::sin(2 * pi * u), twice = std::sin(4 * pi * u);
        float envelope = std::sin(pi * u);
        auto points = bind;
        if (pose == 0 && index == 0)
            rotate(points, layer, 3, 2, 4.5f * cycle);
        if (pose == 0 && index == 1) {
            rotate(points, layer, 7, 6, 10.f * twice * envelope);
            rotate(points, layer, 8, 7, 7.f * twice * envelope);
            rotate(points, layer, 3, 2, 1.5f * cycle);
        }
        if (pose == 0 && index == 2) {
            rotate(points, layer, 1, 0, 1.4f * cycle);
            for (int j = 1; j <= 12; ++j)
                points[size_t(j)].y -= 6.f * imageScale * envelope * envelope;
        }
        if (pose == 1 && index == 0) {
            rotate(points, layer, 15, 14, 9.f * cycle);
            rotate(points, layer, 19, 18, -9.f * cycle);
        }
        if (pose == 1 && index == 1) {
            rotate(points, layer, 3, 2, 3.7f * cycle);
            points[3].y += 4.f * imageScale * envelope * envelope;
            points[4].y += 4.f * imageScale * envelope * envelope;
        }
        if (pose == 1 && index == 2) {
            rotate(points, layer, 15, 14, 4.f * twice * envelope);
            rotate(points, layer, 19, 18, 4.f * twice * envelope);
            rotate(points, layer, 16, 15, 10.f * twice * envelope);
            rotate(points, layer, 20, 19, 10.f * twice * envelope);
        }
        if (pose == 2 && index == 0)
            rotate(points, layer, 3, 2, 5.f * cycle);
        if (pose == 2 && index == 1) {
            rotate(points, layer, 7, 6, 7.f * cycle);
            rotate(points, layer, 8, 7, -5.f * cycle);
        }
        if (pose == 2 && index == 2) {
            rotate(points, layer, 1, 0, -3.5f * envelope * envelope);
            rotate(points, layer, 3, 2, -2.f * envelope * envelope);
        }
        // Exact shared endpoints avoid floating point residue at clip changes.
        if (frame == 0 || frame == 24)
            points = bind;
        clip.keys.push_back({3.f * u, std::move(points)});
    }
    layer.clips.push_back(std::move(clip));
}

Layer rig(int pose, const Image &image) {
    static const std::array<Vec2, 21> coordinates[] = {
        {{{509, 548},  {507, 370},  {512, 265},  {511, 135}, {513, 56},   {407, 311},  {349, 460},
          {326, 329},  {304, 266},  {617, 334},  {666, 552}, {751, 721},  {778, 777},  {456, 602},
          {432, 1000}, {462, 1350}, {438, 1468}, {561, 602}, {602, 1034}, {663, 1350}, {688, 1468}}},
        {{{518, 665}, {523, 454},  {528, 311},  {516, 170}, {515, 62},  {391, 350},  {338, 572},
          {257, 771}, {213, 822},  {638, 367},  {692, 583}, {787, 792}, {827, 824},  {473, 711},
          {535, 802}, {455, 1206}, {467, 1383}, {589, 711}, {640, 815}, {744, 1210}, {786, 1386}}},
        {{{532, 815},  {538, 623},  {542, 403},  {538, 242}, {526, 100},  {411, 475},  {357, 706},
          {197, 533},  {111, 519},  {703, 480},  {669, 767}, {510, 1027}, {450, 1101}, {435, 940},
          {310, 1298}, {618, 1318}, {877, 1250}, {631, 948}, {457, 1382}, {839, 1261}, {936, 1320}}}};
    static const int parents[] = {-1, 0, 1, 2, 3, 1, 5, 6, 7, 1, 9, 10, 11, 0, 13, 14, 15, 0, 17, 18, 19};
    static const char *names[] = {"pelvis",
                                  "chest",
                                  "neck",
                                  "head",
                                  "crown",
                                  "screen_left_shoulder",
                                  "screen_left_elbow",
                                  "screen_left_wrist",
                                  "screen_left_hand",
                                  "screen_right_shoulder",
                                  "screen_right_elbow",
                                  "screen_right_wrist",
                                  "screen_right_hand",
                                  "screen_left_hip",
                                  "screen_left_knee",
                                  "screen_left_ankle",
                                  "screen_left_toe",
                                  "screen_right_hip",
                                  "screen_right_knee",
                                  "screen_right_ankle",
                                  "screen_right_toe"};
    Layer layer;
    layer.id = pose == 0 ? "standing" : pose == 1 ? "seated" : "kneeling";
    layer.image = uint32_t(pose + 1);
    layer.visible = pose == 0;
    layer.grid = 40;
    float xScale = float(image.width) / 1024, yScale = float(image.height) / 1536;
    for (int i = 0; i < 21; ++i)
        layer.joints.push_back(
            {names[i], parents[i], {coordinates[pose][i].x * xScale, coordinates[pose][i].y * yScale}});
    const float scales[] = {.44f, .40f, .32f};
    const Vec2 offsets[] = {{675, 80}, {660, 115}, {685, 280}};
    layer.scale = scales[pose] / xScale;
    layer.offset = offsets[pose];
    for (int a = 0; a < 3; ++a)
        addClip(layer, pose, a, xScale);
    return layer;
}
} // namespace

AnimeStep animeStep(double seconds) {
    if (!std::isfinite(seconds))
        throw std::runtime_error("Demo time must be finite");
    double time = std::fmod(std::max(0.0, seconds), 27.0);
    int segment = int(time / 3.0);
    return {segment / 3, segment % 3, float(time - segment * 3), float(time)};
}

Scene animeDemo(Font font, std::array<Image, 3> artwork) {
    Scene scene;
    scene.title = "HANA / nine living-comic animation tests";
    scene.width = 1280;
    scene.height = 800;
    scene.poseRate = 12;
    scene.fonts.push_back(std::move(font));
    Image stage{1280, 800, std::vector<uint8_t>(1280 * 800 * 4)};
    rect(stage, 0, 0, 1280, 800, {230, 234, 232, 255});
    rect(stage, 32, 32, 1216, 736, {244, 243, 235, 255});
    rect(stage, 32, 32, 8, 736, {48, 114, 111, 255});
    rect(stage, 580, 56, 2, 688, {216, 220, 213, 255});
    for (int x = 626; x < 1240; x += 48)
        rect(stage, x, 58, 1, 672, {233, 233, 221, 255});
    for (int y = 58; y < 730; y += 48)
        rect(stage, 620, y, 600, 1, {233, 233, 221, 255});
    rect(stage, 612, 742, 610, 2, {122, 153, 146, 255});
    scene.images.push_back(std::move(stage));
    for (auto &image : artwork)
        scene.images.push_back(std::move(image));
    // Bench is static geometry packed as its own background image, never skinned with the character.
    Image bench{1280, 800, std::vector<uint8_t>(1280 * 800 * 4)};
    rect(bench, 744, 451, 310, 19, {68, 90, 88, 255});
    rect(bench, 746, 451, 306, 6, {114, 150, 138, 255});
    rect(bench, 770, 470, 12, 272, {68, 90, 88, 255});
    rect(bench, 1021, 470, 12, 272, {68, 90, 88, 255});
    rect(bench, 780, 689, 245, 8, {86, 113, 103, 255});
    scene.images.push_back(std::move(bench));
    // Always retain the bench reference in the file; the sequence hides it off-canvas when not seated.
    scene.backgrounds = {{0, {0, 0}, {1280, 800}}, {4, {-2000, 0}, {1280, 800}}};
    Character character;
    character.id = "hana";
    character.position = {0, 0};
    character.bubble.offset = {80, 220};
    character.bubble.width = 430;
    character.bubble.fill = {255, 254, 246, 255};
    character.bubble.edge = {43, 91, 89, 255};
    character.bubble.text = {39, 55, 59, 255};
    for (int pose = 0; pose < 3; ++pose)
        character.layers.push_back(rig(pose, scene.images[pose + 1]));
    scene.characters.push_back(std::move(character));
    applyAnimeDemo(scene, 0);
    prepare(scene);
    return scene;
}

void validateAnimeDemo(const Scene &scene) {
    validate(scene);
    if (scene.characters.size() != 1 || scene.characters[0].id != "hana" ||
        scene.characters[0].layers.size() != 3 || scene.backgrounds.size() != 2)
        throw std::runtime_error("This is not the three-pose Hana test scene. Rebuild anime-test.cscene.");
    for (int p = 0; p < 3; ++p) {
        const auto &layer = scene.characters[0].layers[p];
        if (layer.clips.size() != 3)
            throw std::runtime_error("Anime test requires three clips per pose");
        for (int a = 0; a < 3; ++a)
            if (layer.clips[a].name != clipNames[p][a] || layer.clips[a].duration != 3.f)
                throw std::runtime_error(
                    "Anime test clips must retain their names and three-second duration");
    }
}

AnimeStep applyAnimeDemo(Scene &scene, double seconds) {
    AnimeStep step = animeStep(seconds);
    auto &character = scene.characters.at(0);
    for (int p = 0; p < 3; ++p) {
        auto &layer = character.layers.at(size_t(p));
        layer.visible = p == step.pose;
        if (layer.visible)
            layer.active = uint32_t(step.animation);
    }
    scene.backgrounds.at(1).position.x = step.pose == 1 ? 0.f : -2000.f;
    std::ostringstream text;
    text << "HANA / ANIMATION STUDY\n\n"
         << poseNames[step.pose] << "  /  " << step.pose + 1 << " of 3\n"
         << "Motion " << step.animation + 1 << " of 3\n\n"
         << descriptions[step.pose][step.animation]
         << "\n\n3 seconds per motion\n9 seconds per pose / 27-second loop";
    character.text = text.str();
    return step;
}

std::string animeStepLabel(const AnimeStep &step) {
    std::ostringstream out;
    out << poseNames[step.pose] << " / " << clipNames[step.pose][step.animation] << "   |   " << std::fixed
        << std::setprecision(1) << step.localTime << "/3 s   |   " << step.loopTime
        << "/27 s   |   SPACE pause   HOME restart   1-9 jump";
    return out.str();
}
} // namespace comic
