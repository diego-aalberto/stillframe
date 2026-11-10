#include "render.hpp"
#include <GL/glu.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace comic {
GLuint Renderer::texture(const Image &i) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(i.width), GLsizei(i.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 i.rgba.data());
    if (glGetError() != GL_NO_ERROR) {
        GLint limit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
        uint32_t w = 1, h = 1;
        while (w < i.width)
            w *= 2;
        while (h < i.height)
            h *= 2;
        w = std::min(w, uint32_t(limit));
        h = std::min(h, uint32_t(limit));
        std::vector<uint8_t> resized(size_t(w) * h * 4);
        int result = gluScaleImage(GL_RGBA, GLsizei(i.width), GLsizei(i.height), GL_UNSIGNED_BYTE,
                                   i.rgba.data(), GLsizei(w), GLsizei(h), GL_UNSIGNED_BYTE, resized.data());
        if (!result)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(w), GLsizei(h), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         resized.data());
        if (result || glGetError() != GL_NO_ERROR) {
            glDeleteTextures(1, &t);
            throw std::runtime_error(
                "Cannot upload texture. Try smaller images or update the graphics driver.");
        }
    }
    return t;
}
void Renderer::initialize(HWND w) {
    dc = GetDC(w);
    PIXELFORMATDESCRIPTOR p{};
    p.nSize = sizeof(p);
    p.nVersion = 1;
    p.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    p.iPixelType = PFD_TYPE_RGBA;
    p.cColorBits = 32;
    p.cAlphaBits = 8;
    p.iLayerType = PFD_MAIN_PLANE;
    int format = ChoosePixelFormat(dc, &p);
    if (!format || !SetPixelFormat(dc, format, &p))
        throw std::runtime_error("Cannot initialize the graphics window");
    context = wglCreateContext(dc);
    if (!context || !wglMakeCurrent(dc, context))
        throw std::runtime_error("OpenGL is unavailable");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    ui = rasterFont(defaultFont());
    uiTexture = texture(ui.atlas);
}
void Renderer::upload(const Scene &s) {
    std::vector<GLuint> nextImages, nextFonts;
    try {
        for (auto &i : s.images)
            nextImages.push_back(texture(i));
        for (auto &f : s.fonts)
            nextFonts.push_back(texture(f.atlas));
    } catch (...) {
        glDeleteTextures(GLsizei(nextImages.size()), nextImages.data());
        glDeleteTextures(GLsizei(nextFonts.size()), nextFonts.data());
        throw;
    }
    glDeleteTextures(GLsizei(images.size()), images.data());
    glDeleteTextures(GLsizei(fonts.size()), fonts.data());
    images = std::move(nextImages);
    fonts = std::move(nextFonts);
}
void Renderer::close(HWND w) {
    if (context) {
        wglMakeCurrent(dc, context);
        glDeleteTextures(GLsizei(images.size()), images.data());
        glDeleteTextures(GLsizei(fonts.size()), fonts.data());
        glDeleteTextures(1, &uiTexture);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        context = nullptr;
    }
    if (dc) {
        ReleaseDC(w, dc);
        dc = nullptr;
    }
}
void Renderer::quad(GLuint t, float x, float y, float w, float h, Color c, float u0, float v0, float u1,
                    float v1) {
    if (t) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, t);
    } else
        glDisable(GL_TEXTURE_2D);
    glColor4ub(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0);
    glVertex2f(x, y);
    glTexCoord2f(u1, v0);
    glVertex2f(x + w, y);
    glTexCoord2f(u1, v1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1);
    glVertex2f(x, y + h);
    glEnd();
}
void Renderer::text(const Font &f, GLuint tex, const std::string &str, float x, float y, float s, Color c) {
    auto w = wide(str);
    float start = x;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4ub(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    for (wchar_t ch : w) {
        if (ch == L'\r')
            continue;
        if (ch == L'\n') {
            x = start;
            y += f.lineHeight * s;
            continue;
        }
        int i = (ch >= 32 && ch <= 255) ? ch - 32 : '?' - 32;
        float glyphWidth = std::min(f.cellWidth, f.glyphs[i].advance + 2);
        float u = float(i % 16) * f.cellWidth / f.atlas.width,
              v = float(i / 16) * f.cellHeight / f.atlas.height,
              du = std::max(0.f, glyphWidth - 1) / f.atlas.width,
              dv = std::max(0.f, f.cellHeight - 1) / f.atlas.height;
        u += .5f / f.atlas.width;
        v += .5f / f.atlas.height;
        float ww = glyphWidth * s, hh = f.cellHeight * s;
        glTexCoord2f(u, v);
        glVertex2f(x, y);
        glTexCoord2f(u + du, v);
        glVertex2f(x + ww, y);
        glTexCoord2f(u + du, v + dv);
        glVertex2f(x + ww, y + hh);
        glTexCoord2f(u, v + dv);
        glVertex2f(x, y + hh);
        x += f.glyphs[i].advance * s;
    }
    glEnd();
}
void Renderer::bubble(const Character &c, const Scene &s) {
    if (c.text.empty())
        return;
    auto &f = s.fonts[c.font];
    float size = .8f, maxWidth = c.bubble.width - 32;
    std::wstring wrapped;
    float line = 0;
    int lines = 1;
    auto source = wide(c.text);
    for (size_t at = 0; at < source.size() && lines < 10;) {
        wchar_t ch = source[at];
        if (ch == L'\r') {
            at++;
            continue;
        }
        if (ch == L'\n') {
            wrapped += ch;
            line = 0;
            lines++;
            at++;
            continue;
        }
        size_t end = at;
        float wordWidth = 0;
        while (end < source.size() && source[end] != L' ' && source[end] != L'\n' && source[end] != L'\r') {
            auto z = source[end++];
            wordWidth += f.glyphs[(z >= 32 && z <= 255) ? z - 32 : '?' - 32].advance * size;
        }
        if (end > at && line > 0 && line + wordWidth > maxWidth) {
            wrapped += L'\n';
            line = 0;
            if (++lines >= 10)
                break;
        }
        if (end == at)
            end++;
        while (at < end && lines < 10) {
            ch = source[at++];
            float a = f.glyphs[(ch >= 32 && ch <= 255) ? ch - 32 : '?' - 32].advance * size;
            if (line + a > maxWidth) {
                wrapped += L'\n';
                line = 0;
                if (++lines >= 10)
                    break;
            }
            if (ch == L' ' && line == 0)
                continue;
            wrapped += ch;
            line += a;
        }
    }
    float x = c.position.x + c.bubble.offset.x, y = c.position.y + c.bubble.offset.y,
          h = std::min(lines, 10) * f.lineHeight * size + 30, w = c.bubble.width;
    quad(0, x + 5, y + 6, w, h, {0, 0, 0, 65});
    quad(0, x, y, w, h, c.bubble.edge);
    quad(0, x + 3, y + 3, w - 6, h - 6, c.bubble.fill);
    glDisable(GL_TEXTURE_2D);
    glColor4ub(c.bubble.edge.r, c.bubble.edge.g, c.bubble.edge.b, c.bubble.edge.a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + w - 58, y + h - 1);
    glVertex2f(x + w - 23, y + h - 1);
    glVertex2f(x + w - 15, y + h + 24);
    glEnd();
    glColor4ub(c.bubble.fill.r, c.bubble.fill.g, c.bubble.fill.b, c.bubble.fill.a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + w - 52, y + h - 4);
    glVertex2f(x + w - 26, y + h - 4);
    glVertex2f(x + w - 21, y + h + 15);
    glEnd();
    text(f, fonts[c.font], utf8(wrapped), x + 16, y + 13, size, c.bubble.text);
}
void Renderer::draw(Scene &s, float seconds, bool rig, bool mesh, int ci, int li, int ji, bool bindMode,
                    const std::vector<Vec2> *overridePose, const std::string &status, float sampleRate) {
    RECT r{};
    GetClientRect(WindowFromDC(dc), &r);
    width = std::max(1L, r.right);
    height = std::max(1L, r.bottom);
    glViewport(0, 0, width, height);
    glClearColor(.055f, .067f, .09f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    int footer=showStatus?34:0;
    scale = std::min(float(width) / s.width, float(std::max(1, height - footer)) / s.height);
    if (fillViewport)
        scale = std::max(float(width) / s.width, float(std::max(1, height - footer)) / s.height);
    originX = (width - s.width * scale) / 2;
    originY = (height - footer - s.height * scale) / 2;
    glTranslatef(originX, originY, 0);
    glScalef(scale, scale, 1);
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(originX), int(height - originY - s.height * scale), int(s.width * scale),
              int(s.height * scale));
    quad(0, 0, 0, float(s.width), float(s.height), s.clear);
    for (auto &b : s.backgrounds)
        quad(images[b.image], b.position.x, b.position.y, b.size.x, b.size.y, {});
    for (size_t c = 0; c < s.characters.size(); c++)
        for (size_t l = 0; l < s.characters[c].layers.size(); l++) {
            auto &character = s.characters[c];
            auto &layer = character.layers[l];
            if (!layer.visible)
                continue;
            bool selected = int(c) == ci && int(l) == li;
            std::vector<Vec2> pose;
            if (selected && overridePose)
                pose = *overridePose;
            else if (selected && bindMode) {
                for (auto &j : layer.joints)
                    pose.push_back(j.bind);
            } else
                pose = sample(layer, std::max(0.f, seconds - layer.timeOrigin), sampleRate < 0 ? s.poseRate : sampleRate);
            deform(layer, pose);
            glPushMatrix();
            glTranslatef(character.position.x + layer.offset.x, character.position.y + layer.offset.y, 0);
            glScalef(layer.scale, layer.scale, 1);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, images[layer.image]);
            glColor4ub(255, 255, 255, 255);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glVertexPointer(2, GL_FLOAT, sizeof(Vec2), layer.deformed.data());
            glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &layer.mesh[0].uv);
            glDrawElements(GL_TRIANGLES, GLsizei(layer.indices.size()), GL_UNSIGNED_INT,
                           layer.indices.data());
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            if (mesh && selected) {
                glDisable(GL_TEXTURE_2D);
                glColor4ub(125, 244, 205, 100);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawElements(GL_TRIANGLES, GLsizei(layer.indices.size()), GL_UNSIGNED_INT,
                               layer.indices.data());
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            glDisableClientState(GL_VERTEX_ARRAY);
            if (rig && selected) {
                glDisable(GL_TEXTURE_2D);
                glLineWidth(2);
                glColor4ub(123, 245, 206, 255);
                glBegin(GL_LINES);
                for (size_t j = 1; j < pose.size(); j++) {
                    auto p = pose[size_t(layer.joints[j].parent)];
                    glVertex2f(p.x, p.y);
                    glVertex2f(pose[j].x, pose[j].y);
                }
                glEnd();
                for (size_t j = 0; j < pose.size(); j++) {
                    auto p = pose[j];
                    quad(0, p.x - 5, p.y - 5, 10, 10,
                         int(j) == ji ? Color{255, 188, 89, 255} : Color{226, 255, 247, 255});
                }
            }
            glPopMatrix();
        }
    for (auto &c : s.characters)
        bubble(c, s);
    glDisable(GL_SCISSOR_TEST);
    glLoadIdentity();
    if(showStatus){
      quad(0, 0, float(height - 32), float(width), 32, {20, 25, 34, 255});
      text(ui, uiTexture, status, 12, float(height - 26), .58f, {176, 195, 210, 255});
    }
    if (overlay) overlay();
    if (!capturePath.empty()) {
        std::vector<uint8_t> pixels(size_t(width) * height * 3);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        std::ofstream f(capturePath, std::ios::binary);
        f << "P6\n" << width << ' ' << height << "\n255\n";
        for (int y = height - 1; y >= 0; y--)
            f.write(reinterpret_cast<char *>(pixels.data() + size_t(y) * width * 3), width * 3);
        if (!f)
            throw std::runtime_error("Could not save smoke-test screenshot");
        capturePath.clear();
    }
    SwapBuffers(dc);
}
} // namespace comic
