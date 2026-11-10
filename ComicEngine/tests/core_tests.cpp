#include "../src/anime_demo.hpp"
#include "../src/core.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace comic;
void expect(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void rejects(const std::function<void()> &fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::exception &) {
        caught = true;
    }
    expect(caught, "Expected invalid input rejection");
}
bool near(Vec2 a, Vec2 b) {
    return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f;
}
Font testFont() {
    Font f;
    f.atlas = {16, 14, std::vector<uint8_t>(16 * 14 * 4, 255)};
    f.cellWidth = f.cellHeight = f.lineHeight = 1;
    for (auto &g : f.glyphs)
        g.advance = 1;
    return f;
}
int main() {
    try {
        auto dir =
            std::filesystem::temp_directory_path() /
            ("comic-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto s = demo(testFont());
        {
            Image art{512, 768, std::vector<uint8_t>(512 * 768 * 4, 255)};
            auto anime = animeDemo(testFont(), {art, art, art});
            validateAnimeDemo(anime);
            const double boundaries[] = {0, 2.999999, 3, 8.999999, 9, 17.999999, 18, 26.999999, 27, 54};
            const int expected[] = {0, 0, 1, 2, 3, 5, 6, 8, 0, 0};
            for (size_t i = 0; i < 10; ++i) {
                auto step = animeStep(boundaries[i]);
                expect(step.pose * 3 + step.animation == expected[i], "Incorrect 3/9/27-second boundary");
            }
            for (int segment = 0; segment < 9; ++segment) {
                auto step = applyAnimeDemo(anime, segment * 3 + 1.125);
                expect(step.pose == segment / 3 && step.animation == segment % 3,
                       "Anime clips must play in pose-major order");
                int visible = 0;
                for (auto &layer : anime.characters[0].layers)
                    visible += layer.visible ? 1 : 0;
                expect(visible == 1, "Exactly one pose must be visible");
                auto &layer = anime.characters[0].layers[step.pose];
                const auto &clip = layer.clips[step.animation];
                expect(clip.duration == 3 && clip.keys.size() == 25,
                       "Every anime clip needs three seconds of keys");
                for (size_t joint = 0; joint < layer.joints.size(); ++joint) {
                    expect(near(clip.keys.front().points[joint], layer.joints[joint].bind),
                           "Clip must start in bind pose");
                    expect(near(clip.keys.back().points[joint], layer.joints[joint].bind),
                           "Clip must end in bind pose");
                }
                auto points = sample(layer, step.localTime, anime.poseRate);
                deform(layer, points);
                float displacement = 0;
                for (size_t v = 0; v < layer.mesh.size(); ++v) {
                    auto delta = layer.deformed[v] - layer.mesh[v].bind;
                    displacement = std::max(displacement, std::abs(delta.x) + std::abs(delta.y));
                    expect(std::isfinite(layer.deformed[v].x) && std::isfinite(layer.deformed[v].y),
                           "Anime deformation must remain finite");
                }
                expect(displacement > 1, "Every animation must actually deform its image");
            }
            saveScene(anime, dir / "anime.cscene");
            auto restored = loadScene(dir / "anime.cscene");
            validateAnimeDemo(restored);
            auto loop = applyAnimeDemo(restored, 27);
            expect(loop.pose == 0 && loop.animation == 0 && loop.localTime == 0,
                   "Anime loop must restart standing clip one at 27 seconds");
            std::cout << "PASS: anime 3/9/27-second boundaries, all nine skeletal clips, visibility, resting "
                         "endpoints and save/reload.\n";
        }
        Layer bone;
        bone.grid = 2;
        bone.joints = {{"root", -1, {0, 0}}, {"tip", 0, {0, 10}}};
        Image square{10, 10, std::vector<uint8_t>(400, 255)};
        buildMesh(bone, square);
        deform(bone, {{0, 0}, {20, 0}});
        expect(near(bone.deformed[6], {20, 0}), "Bone endpoint must follow rotation and longitudinal scale");
        expect(near(bone.deformed[8], {20, -10}), "Bone rotation must preserve perpendicular thickness");
        deform(bone, {{0, 0}, {0, 0}});
        expect(near(bone.deformed[6], {0, 0}), "Collapsed bone should remain finite at root");
        auto &l = s.characters[0].layers[0];
        auto base = sample(l, 0, 0);
        deform(l, base);
        for (size_t i = 0; i < l.mesh.size(); i++)
            expect(near(l.mesh[i].bind, l.deformed[i]), "Bind pose must preserve artwork");
        auto moved = base;
        for (auto &p : moved)
            p = p + Vec2{25, -13};
        deform(l, moved);
        for (size_t i = 0; i < l.mesh.size(); i++)
            expect(near(l.mesh[i].bind + Vec2{25, -13}, l.deformed[i]),
                   "Root translation must translate entire mesh");
        auto half = sample(l, .75f, 0);
        expect(near(half[2], base[2] + Vec2{0, -2}), "Interpolated pose mismatch");
        expect(near(sample(l, 3, 0)[0], base[0]), "Loop boundary mismatch");
        expect(near(sample(l, .01f, 12)[2], base[2]), "Comic hold should quantize pose time");
        l.clips[0].loop = false;
        expect(near(sample(l, 99, 0)[2], base[2]), "Non-loop clip should hold final key");
        l.clips[0].loop = true;
        saveScene(s, dir / "sample.cscene");
        auto roundtrip = loadScene(dir / "sample.cscene");
        expect(manifest(roundtrip) == manifest(s), "Scene metadata roundtrip mismatch");
        expect(roundtrip.images[1].rgba == s.images[1].rgba, "Image roundtrip mismatch");
        expect(roundtrip.fonts[0].atlas.rgba == s.fonts[0].atlas.rgba, "Font roundtrip mismatch");
        saveScene(roundtrip, dir / "sample.cscene");
        auto redundant = s;
        redundant.images.insert(redundant.images.begin(), s.images[0]);
        for (auto &b : redundant.backgrounds)
            ++b.image;
        for (auto &c : redundant.characters)
            for (auto &layer : c.layers)
                ++layer.image;
        redundant.fonts.push_back(testFont());
        compactAssets(redundant);
        expect(redundant.images.size() == s.images.size() && redundant.fonts.size() == s.fonts.size(),
               "Unused assets should be removed");
        expect(redundant.images[redundant.characters[0].layers[0].image].rgba == s.images[1].rgba,
               "Compaction must preserve image references");
        saveImage(s.images[1], dir / "image.cimg");
        expect(loadImage(dir / "image.cimg").rgba == s.images[1].rgba, "CIMG roundtrip mismatch");
        saveFont(s.fonts[0], dir / "font.cfont");
        expect(loadFont(dir / "font.cfont").name == s.fonts[0].name, "CFNT roundtrip mismatch");
        expect(play(s, "atlas", "body", "wave"), "Named animation not found");
        expect(!play(s, "atlas", "body", "missing"), "Unknown animation accepted");
        expect(say(s, "atlas", "New text"), "Dialogue API failed");
        expect(!say(s, "missing", "text"), "Unknown speaker accepted");
        auto bad = s;
        bad.characters[0].layers[0].joints[1].parent = 1;
        rejects([&] { validate(bad); });
        bad = s;
        bad.characters[0].layers[0].clips[0].keys[1].time = 0;
        rejects([&] { validate(bad); });
        bad = s;
        bad.characters[0].layers[0].clips[0].keys[0].points.pop_back();
        rejects([&] { validate(bad); });
        bad = s;
        bad.characters[0].layers[0].image = 999;
        rejects([&] { validate(bad); });
        bad = s;
        bad.characters.push_back(bad.characters[0]);
        rejects([&] { validate(bad); });
        bad = s;
        bad.fonts[0].lineHeight = 0;
        rejects([&] { validate(bad); });
        {
            std::ofstream f(dir / "broken.cscene", std::ios::binary);
            f << "CSCN";
        }
        rejects([&] { loadScene(dir / "broken.cscene"); });
        {
            std::ifstream f(dir / "sample.cscene", std::ios::binary);
            std::vector<char> data((std::istreambuf_iterator<char>(f)), {});
            data.resize(data.size() - 9);
            std::ofstream out(dir / "truncated.cscene", std::ios::binary);
            out.write(data.data(), std::streamsize(data.size()));
        }
        rejects([&] { loadScene(dir / "truncated.cscene"); });
        {
            std::ofstream f(dir / "sample.cscene", std::ios::app | std::ios::binary);
            f.put('x');
        }
        rejects([&] { loadScene(dir / "sample.cscene"); });
        auto start = std::chrono::steady_clock::now();
        double checksum = 0;
        for (int n = 0; n < 10000; n++) {
            auto pose = sample(l, n / 60.f, 12);
            deform(l, pose);
            checksum += l.deformed[112].x;
        }
        auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "PASS: validation, embedded scene/assets, bind skinning, translation, interpolation, "
                     "loop/hold, malformed files and command API.\n"
                  << "CPU benchmark: 10,000 samples + skinning (625 vertices, 13 joints): " << ms << " ms; "
                  << ms / 10000 << " ms/update. Checksum " << checksum << "\n";
        for (auto &p : std::filesystem::directory_iterator(dir))
            std::filesystem::remove(p.path());
        std::filesystem::remove(dir);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
