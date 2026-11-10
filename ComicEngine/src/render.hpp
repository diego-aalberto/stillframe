#pragma once
#include "platform.hpp"
#include <GL/gl.h>
#include <functional>

namespace comic {
class Renderer {
    HDC dc = nullptr;
    HGLRC context = nullptr;
    std::vector<GLuint> images, fonts;
    GLuint uiTexture = 0;
    Font ui;
    GLuint texture(const Image &i);
    void quad(GLuint texture, float x, float y, float w, float h, Color c, float u0 = 0, float v0 = 0,
              float u1 = 1, float v1 = 1);
    void text(const Font &f, GLuint texture, const std::string &text, float x, float y, float scale,
              Color color);
    void bubble(const Character &c, const Scene &scene);

  public:
    std::filesystem::path capturePath;
    bool fillViewport = false; // Opt-in visual-novel framing; editor/player retain fit-to-window.
    bool showStatus = true;
    std::function<void()> overlay; // Composited into the same back buffer as the scene.
    float scale = 1, originX = 0, originY = 0;
    int width = 1, height = 1;
    void initialize(HWND window);
    void upload(const Scene &scene);
    void close(HWND window);
    void draw(Scene &scene, float seconds, bool showRig, bool showMesh, int character, int layer, int joint,
              bool bindMode, const std::vector<Vec2> *overridePose, const std::string &status,
              float sampleRate = -1);
    Vec2 scenePoint(int x, int y) const {
        return {(x - originX) / scale, (y - originY) / scale};
    }
};
} // namespace comic
