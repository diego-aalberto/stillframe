// Window shell: SDL3 only does what the OS must do (window, input, putting pixels on screen).
// Everything else - drawing, library, games - lives in sf_render and is tested without a display.
#include "cli.hpp"
#include "../render/app.hpp"
#include "../core/platform.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>

namespace sf {

bool window_support_available() { return true; }

namespace {

std::string key_name(SDL_Keycode k) {
    if (k >= SDLK_1 && k <= SDLK_9) return std::to_string(k - SDLK_1 + 1);
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return std::to_string(k - SDLK_KP_1 + 1);
    switch (k) {
        case SDLK_ESCAPE: return "escape";
        case SDLK_RETURN: case SDLK_KP_ENTER: return "enter";
        case SDLK_SPACE: return "space";
        case SDLK_BACKSPACE: return "backspace";
        case SDLK_DELETE: return "delete";
        case SDLK_TAB: return "tab";
        case SDLK_LEFT: return "left";
        case SDLK_RIGHT: return "right";
        case SDLK_UP: return "up";
        case SDLK_DOWN: return "down";
        case SDLK_HOME: return "home";
        case SDLK_END: return "end";
        case SDLK_PAGEUP: return "pageup";
        case SDLK_PAGEDOWN: return "pagedown";
        case SDLK_F5: return "f5";
        case SDLK_F9: return "f9";
        case SDLK_H: return "h";
        case SDLK_LCTRL: case SDLK_RCTRL: return "ctrl";
        default: return "";
    }
}

struct Window {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;
    int cw = 0, ch = 0;      // size of the texture / logical canvas

    ~Window() {
        if (tex) SDL_DestroyTexture(tex);
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
    }

    // (Re)create the streaming texture whenever the canvas size changes, e.g. library -> game.
    bool fit(int w, int h) {
        if (tex && w == cw && h == ch) return true;
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!tex) return false;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        // Letterboxing + mouse coordinate conversion are done by SDL: events converted with
        // SDL_ConvertEventToRenderCoordinates arrive in canvas pixels.
        SDL_SetRenderLogicalPresentation(ren, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        cw = w; ch = h;
        return true;
    }

    void present(const Canvas& c) {
        SDL_UpdateTexture(tex, nullptr, c.pixels().data(), c.width() * 4);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
};

} // namespace

int cmd_run(const std::vector<std::string>& args) {
    AppOptions o;
    bool fullscreen = false;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--fullscreen") fullscreen = true;
        else if (args[i] == "--games" && i + 1 < args.size()) o.games_dir = args[++i];
        else if (args[i] == "--data" && i + 1 < args.size()) o.data_dir = args[++i];
        else if (args[i] == "--name" && i + 1 < args.size()) o.player_name = args[++i];
        else if (args[i] == "run") continue;
        else if (!args[i].empty() && args[i][0] != '-') o.open = args[i];
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) { std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n"; return 1; }
    int rc = 0;
    {
    Window w;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (!SDL_CreateWindowAndRenderer("Stillframe", o.width, o.height, flags, &w.win, &w.ren)) {
        std::cerr << "Could not open a window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(w.ren, 1);
    SDL_StartTextInput(w.win);

    try {
        App app(o);
        Canvas canvas(o.width, o.height);
        Uint64 last = SDL_GetPerformanceCounter();
        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        bool dirty = true;
        std::string title;
        bool quit = false;

        while (!quit && !app.quit_requested()) {
            // Idle screens sleep until something happens; animated ones run at the display rate.
            SDL_Event e;
            bool have = app.needs_frame() || dirty ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 100);
            while (have) {
                if (e.type == SDL_EVENT_QUIT) { quit = true; break; }
                Input in;
                bool send = false;
                switch (e.type) {
                    case SDL_EVENT_MOUSE_MOTION:
                        SDL_ConvertEventToRenderCoordinates(w.ren, &e);
                        in.type = Input::MouseMove; in.x = e.motion.x; in.y = e.motion.y; send = true; break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        SDL_ConvertEventToRenderCoordinates(w.ren, &e);
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            in.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? Input::MouseDown : Input::MouseUp;
                            in.x = e.button.x; in.y = e.button.y; send = true;
                        }
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        in.type = Input::Wheel; in.wheel = e.wheel.y; send = true; break;
                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_KEY_UP: {
                        std::string n = key_name(e.key.key);
                        if (n == "ctrl") { in.type = Input::Key; in.key = e.type == SDL_EVENT_KEY_DOWN ? "ctrl_down" : "ctrl_up"; send = true; }
                        else if (e.type == SDL_EVENT_KEY_DOWN && !n.empty()) {
                            if (n == "enter" && (e.key.mod & SDL_KMOD_ALT)) {      // Alt+Enter toggles fullscreen
                                fullscreen = !fullscreen;
                                SDL_SetWindowFullscreen(w.win, fullscreen);
                            } else {
                                in.type = Input::Key; in.key = n; in.ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0; in.shift = (e.key.mod & SDL_KMOD_SHIFT) != 0; send = true;
                            }
                        } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11) {
                            fullscreen = !fullscreen;
                            SDL_SetWindowFullscreen(w.win, fullscreen);
                        }
                        break;
                    }
                    case SDL_EVENT_TEXT_INPUT:
                        in.type = Input::Text; in.text = e.text.text; send = true; break;
                    default: break;
                }
                if (send) app.handle(in);
                dirty = true;                       // any event (also expose/resize) is worth one redraw
                have = SDL_PollEvent(&e);
            }

            Uint64 now = SDL_GetPerformanceCounter();
            double dt = std::min(0.1, static_cast<double>(now - last) / freq);      // a long stall must not skip a whole scene
            last = now;
            app.update(dt);

            auto [cw, ch] = app.canvas_size();
            if (canvas.width() != cw || canvas.height() != ch) canvas = Canvas(cw, ch);
            if (app.needs_frame() || dirty) {
                app.render(canvas);
                if (!w.fit(cw, ch)) { std::cerr << "Could not create a texture: " << SDL_GetError() << "\n"; rc = 1; break; }
                w.present(canvas);
                dirty = false;
            }
            std::string want = app.in_game() ? "Stillframe" : "Stillframe - library";
            if (want != title) { title = want; SDL_SetWindowTitle(w.win, title.c_str()); }
        }
    } catch (const Error& e) {
        std::cerr << "error: " << e.what() << "\n";
        rc = 1;
    }
    SDL_StopTextInput(w.win);
    }   // Window destroyed here, before SDL shuts down
    SDL_Quit();
    return rc;
}

} // namespace sf
