#include "anime_demo.hpp"
#include "render.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <iomanip>
#include <objbase.h>
#include <shellapi.h>
#include <sstream>
#include <stdexcept>
#include <windowsx.h>

using namespace comic;
namespace {
constexpr bool editor =
#ifdef COMIC_EDITOR
    true;
#else
    false;
#endif
constexpr int panel = 328;
enum {
    Open = 100,
    Save,
    SaveAs,
    New,
    Demo,
    Manifest,
    ImportBackground,
    ImportCharacter,
    ImportLayer,
    ExportImage,
    ImportFont,
    ExportFont,
    PickFont,
    FillColor,
    EdgeColor,
    TextColor,
    Characters = 200,
    Layers,
    Clips,
    Joints,
    Play,
    Restart,
    Rig,
    Mesh,
    BindMode,
    Visible,
    ComicRate,
    Timeline,
    AddJoint,
    SetKey,
    DeleteKey,
    AddClip,
    Duration,
    ApplyDuration,
    Loop,
    Dialogue,
    ApplyDialogue,
    PositionX,
    PositionY,
    ImageScale,
    BubbleWidth,
    BubbleX,
    BubbleY,
    ApplyProperties,
    Status,
    ClipName,
    CharacterName,
    LayerName,
    ApplyNames,
    DeleteLayer,
    DeleteCharacter
};
const wchar_t *sceneFilter = L"Comic scene (*.cscene)\0*.cscene\0\0";
const wchar_t *imageFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.cimg)\0*.png;*.jpg;*.jpeg;*.bmp;*.cimg\0\0";
struct App {
    HWND window = nullptr, canvas = nullptr;
    HFONT font = nullptr;
    Renderer renderer;
    Scene scene;
    std::filesystem::path path;
    bool dirty = false, playing = true, dragging = false, rig = true, mesh = false, bind = false,
         poseDirty = false, refreshing = false;
    int character = 0, layer = 0, joint = 0, inspectorScroll = 0;
    float time = 0;
    std::vector<Vec2> pose;
    std::chrono::steady_clock::time_point last;
    std::string note = "Ready";
    bool initialized = false;
    bool animeTest = false;
    std::filesystem::path screenshot;
    int smokeFrames = 0, exitCode = 0;
    Character *ch() {
        return character >= 0 && character < int(scene.characters.size()) ? &scene.characters[character]
                                                                          : nullptr;
    }
    Layer *lay() {
        auto *c = ch();
        return c && layer >= 0 && layer < int(c->layers.size()) ? &c->layers[layer] : nullptr;
    }
    Clip *clip() {
        auto *l = lay();
        return l ? &l->clips[l->active] : nullptr;
    }
    HWND control(int id) {
        return GetDlgItem(window, id);
    }
    void set(int id, const std::string &text) {
        SetWindowTextW(control(id), wide(text).c_str());
    }
    std::string get(int id) {
        HWND w = control(id);
        int n = GetWindowTextLengthW(w);
        std::wstring s(size_t(n) + 1, L'\0');
        GetWindowTextW(w, s.data(), n + 1);
        s.resize(n);
        return utf8(s);
    }
    float number(int id) {
        auto s = get(id);
        size_t n;
        float f = std::stof(s, &n);
        if (n != s.size() || !std::isfinite(f))
            throw std::runtime_error("Enter a valid number in every property field.");
        return f;
    }
    bool checked(int id) {
        return SendMessageW(control(id), BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    void check(int id, bool yes) {
        SendMessageW(control(id), BM_SETCHECK, yes ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    void combo(int id, const std::vector<std::string> &labels, int selected) {
        HWND c = control(id);
        SendMessageW(c, CB_RESETCONTENT, 0, 0);
        for (auto &s : labels)
            SendMessageW(c, CB_ADDSTRING, 0, LPARAM(wide(s).c_str()));
        SendMessageW(c, CB_SETCURSEL, selected, 0);
    }
    int selected(int id) {
        return int(SendMessageW(control(id), CB_GETCURSEL, 0, 0));
    }
    void message(const std::string &s) {
        note = s;
        set(Status, s);
    }
    void mark() {
        dirty = true;
        std::wstring title =
            (editor ? L"Comic Studio — " : L"Comic Player — ") + wide(scene.title) + (dirty ? L" *" : L"");
        SetWindowTextW(window, title.c_str());
    }
    void resetPose() {
        poseDirty = false;
        pose.clear();
    }
    void refresh() {
        if (!editor)
            return;
        refreshing = true;
        std::vector<std::string> a;
        for (auto &c : scene.characters)
            a.push_back(c.id);
        combo(Characters, a, character);
        a.clear();
        if (ch())
            for (auto &l : ch()->layers)
                a.push_back(l.id);
        combo(Layers, a, layer);
        a.clear();
        if (lay())
            for (auto &c : lay()->clips)
                a.push_back(c.name);
        combo(Clips, a, lay() ? int(lay()->active) : -1);
        a.clear();
        if (lay())
            for (auto &j : lay()->joints)
                a.push_back(j.name);
        combo(Joints, a, joint);
        if (ch()) {
            set(Dialogue, ch()->text);
            set(PositionX, std::to_string(int(ch()->position.x)));
            set(PositionY, std::to_string(int(ch()->position.y)));
            set(BubbleWidth, std::to_string(int(ch()->bubble.width)));
            set(BubbleX, std::to_string(int(ch()->bubble.offset.x)));
            set(BubbleY, std::to_string(int(ch()->bubble.offset.y)));
            set(CharacterName, ch()->id);
        } else
            set(Dialogue, "");
        if (lay()) {
            set(ImageScale, std::to_string(lay()->scale));
            check(Visible, lay()->visible);
            set(LayerName, lay()->id);
        }
        if (clip()) {
            set(Duration, std::to_string(clip()->duration));
            set(ClipName, clip()->name);
            check(Loop, clip()->loop);
        }
        check(Rig, rig);
        check(Mesh, mesh);
        check(BindMode, bind);
        check(ComicRate, scene.poseRate > 0);
        set(Play, playing ? "Pause" : "Play");
        set(Status, note);
        const int needsCharacter[] = {Layers,        Clips,       Joints,        AddJoint,   AddClip,
                                      ImportLayer,   SetKey,      DeleteKey,     Duration,   ApplyDuration,
                                      Loop,          Dialogue,    ApplyDialogue, PositionX,  PositionY,
                                      ImageScale,    BubbleWidth, BubbleX,       BubbleY,    ApplyProperties,
                                      CharacterName, LayerName,   ClipName,      ApplyNames, PickFont,
                                      FillColor,     EdgeColor,   TextColor,     Visible};
        for (int id : needsCharacter)
            EnableWindow(control(id), ch() != nullptr);
        refreshing = false;
    }
    bool save(bool as = false) {
        if (poseDirty)
            storeKey();
        if (as || path.empty()) {
            auto p = fileDialog(window, true, sceneFilter, L"cscene");
            if (p.empty())
                return false;
            path = p;
        }
        Scene cleaned = scene;
        compactAssets(cleaned);
        if (cleaned.images.size() != scene.images.size() || cleaned.fonts.size() != scene.fonts.size())
            renderer.upload(cleaned);
        scene = std::move(cleaned);
        saveScene(scene, path);
        dirty = false;
        SetWindowTextW(window,
                       ((editor ? L"Comic Studio — " : L"Comic Player — ") + wide(scene.title)).c_str());
        message("Saved: " + utf8(path.filename().wstring()));
        return true;
    }
    bool leave() {
        if (!dirty && !poseDirty)
            return true;
        int r = MessageBoxW(window, L"Save your scene before continuing?", L"Unsaved scene",
                            MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL)
            return false;
        if (r == IDYES)
            return save();
        return true;
    }
    bool settlePose() {
        if (!poseDirty)
            return true;
        int r =
            MessageBoxW(window, L"Keep the edited pose as a keyframe before changing selection or playback?",
                        L"Unsaved pose", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL)
            return false;
        if (r == IDYES)
            storeKey();
        else
            resetPose();
        return true;
    }
    void install(Scene next, std::filesystem::path p = {}) {
        if (next.fonts.empty())
            next.fonts.push_back(rasterFont(defaultFont()));
        compactAssets(next);
        renderer.upload(next);
        scene = std::move(next);
        path = std::move(p);
        character = layer = joint = 0;
        time = 0;
        resetPose();
        dirty = false;
        bind = false;
        SetWindowTextW(window,
                       ((editor ? L"Comic Studio — " : L"Comic Player — ") + wide(scene.title)).c_str());
        refresh();
    }
    void open() {
        auto p = fileDialog(window, false, sceneFilter, L"cscene");
        if (!p.empty() && leave())
            install(loadScene(p), p);
    }
    void storeKey() {
        auto *l = lay();
        if (!l)
            return;
        if (bind)
            throw std::runtime_error("Turn off Edit bind rig before adding a pose key.");
        auto *c = clip();
        float t = std::clamp(time, 0.f, c->duration);
        if (!poseDirty)
            pose = sample(*l, t, 0);
        Key key{t, pose};
        auto i = std::lower_bound(c->keys.begin(), c->keys.end(), t,
                                  [](const Key &k, float x) { return k.time < x; });
        if (i != c->keys.end() && std::abs(i->time - t) < .0005f)
            *i = key;
        else if (i != c->keys.begin() && std::abs((i - 1)->time - t) < .0005f)
            *(i - 1) = key;
        else {
            if (c->keys.size() >= 4096)
                throw std::runtime_error("Clip keyframe limit reached");
            c->keys.insert(i, key);
        }
        resetPose();
        mark();
        message("Pose key saved at " + std::to_string(t) + " s");
    }
    void addImage(bool background, bool newCharacter) {
        auto p = fileDialog(window, false, imageFilter, L"png");
        if (p.empty())
            return;
        if (!settlePose())
            return;
        Image image = importImage(p);
        if (scene.images.size() >= 128)
            throw std::runtime_error("Scene image limit reached");
        if (background && scene.backgrounds.size() >= 32)
            throw std::runtime_error("Background limit reached");
        if (newCharacter && scene.characters.size() >= 32)
            throw std::runtime_error("Character limit reached");
        if (!background && !newCharacter && !ch())
            throw std::runtime_error("Add a character first");
        if (!background && !newCharacter && ch()->layers.size() >= 64)
            throw std::runtime_error("Layer limit reached");
        Scene previous = scene;
        int previousCharacter = character, previousLayer = layer, previousJoint = joint;
        try {
            uint32_t id = uint32_t(scene.images.size());
            if (background) {
                scene.backgrounds.push_back({id, {0, 0}, {float(scene.width), float(scene.height)}});
            } else {
                if (newCharacter) {
                    Character c;
                    c.id = "character_" + std::to_string(scene.characters.size() + 1);
                    while (std::any_of(scene.characters.begin(), scene.characters.end(),
                                       [&](const Character &v) { return v.id == c.id; }))
                        c.id += "_";
                    c.position = {float(scene.width) * .5f, float(scene.height) * .2f};
                    c.bubble.offset = {-250, 0};
                    scene.characters.push_back(c);
                    character = int(scene.characters.size()) - 1;
                }
                Layer l;
                l.id = "image_" + std::to_string(ch()->layers.size() + 1);
                while (std::any_of(ch()->layers.begin(), ch()->layers.end(),
                                   [&](const Layer &v) { return v.id == l.id; }))
                    l.id += "_";
                l.image = id;
                l.scale = std::min(1.f, 400.f / image.height);
                float w = float(image.width), h = float(image.height);
                l.joints = {{"root", -1, {w * .5f, h * .8f}},
                            {"spine", 0, {w * .5f, h * .45f}},
                            {"head", 1, {w * .5f, h * .2f}}};
                Clip c;
                std::vector<Vec2> points;
                for (auto &j : l.joints)
                    points.push_back(j.bind);
                c.keys = {{0, points}, {2, points}};
                l.clips.push_back(c);
                buildMesh(l, image);
                ch()->layers.push_back(l);
                layer = int(ch()->layers.size()) - 1;
                joint = 0;
            }
            scene.images.push_back(std::move(image));
            validate(scene);
            renderer.upload(scene);
        } catch (...) {
            scene = std::move(previous);
            character = previousCharacter;
            layer = previousLayer;
            joint = previousJoint;
            throw;
        }
        mark();
        refresh();
        message("Imported and embedded " + utf8(p.filename().wstring()));
    }
    void color(int id) {
        if (!ch())
            return;
        Color *c = id == FillColor   ? &ch()->bubble.fill
                   : id == EdgeColor ? &ch()->bubble.edge
                                     : &ch()->bubble.text;
        static COLORREF custom[16]{};
        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = window;
        cc.rgbResult = RGB(c->r, c->g, c->b);
        cc.lpCustColors = custom;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (ChooseColorW(&cc)) {
            c->r = GetRValue(cc.rgbResult);
            c->g = GetGValue(cc.rgbResult);
            c->b = GetBValue(cc.rgbResult);
            mark();
        }
    }
    void assignFont(Font fontData) {
        Scene next = scene;
        compactAssets(next);
        uint32_t selectedFont = next.characters[character].font;
        bool shared = false;
        for (size_t i = 0; i < next.characters.size(); ++i)
            if (int(i) != character && next.characters[i].font == selectedFont)
                shared = true;
        if (!shared)
            next.fonts[selectedFont] = std::move(fontData);
        else {
            if (next.fonts.size() >= 32)
                throw std::runtime_error("Font limit reached");
            next.characters[character].font = uint32_t(next.fonts.size());
            next.fonts.push_back(std::move(fontData));
        }
        validate(next);
        renderer.upload(next);
        scene = std::move(next);
        mark();
    }
    void command(int id, int notification = 0) {
        if (refreshing)
            return;
        switch (id) {
        case Open:
            open();
            break;
        case Save:
            save();
            break;
        case SaveAs:
            save(true);
            break;
        case New:
            if (leave()) {
                Scene s;
                s.fonts.push_back(rasterFont(defaultFont()));
                install(std::move(s));
                mark();
            }
            break;
        case Demo:
            if (leave()) {
                install(demo(rasterFont(defaultFont())));
                mark();
            }
            break;
        case Manifest: {
            auto p = fileDialog(window, true, L"JSON manifest (*.json)\0*.json\0\0", L"json");
            if (!p.empty()) {
                std::ofstream f(p);
                f << manifest(scene);
                if (!f)
                    throw std::runtime_error("Cannot write manifest");
                message("Manifest exported");
            }
            break;
        }
        case ImportBackground:
            addImage(true, false);
            break;
        case ImportCharacter:
            addImage(false, true);
            break;
        case ImportLayer:
            addImage(false, false);
            break;
        case ExportImage:
            if (lay()) {
                auto p = fileDialog(window, true, L"Comic image (*.cimg)\0*.cimg\0\0", L"cimg");
                if (!p.empty())
                    saveImage(scene.images[lay()->image], p);
            }
            break;
        case ImportFont:
            if (ch()) {
                auto p = fileDialog(window, false, L"Comic font (*.cfont)\0*.cfont\0\0", L"cfont");
                if (!p.empty()) {
                    assignFont(loadFont(p));
                }
            }
            break;
        case ExportFont:
            if (ch()) {
                auto p = fileDialog(window, true, L"Comic font (*.cfont)\0*.cfont\0\0", L"cfont");
                if (!p.empty())
                    saveFont(scene.fonts[ch()->font], p);
            }
            break;
        case PickFont:
            if (ch()) {
                LOGFONTW lf = defaultFont();
                CHOOSEFONTW cf{};
                cf.lStructSize = sizeof(cf);
                cf.hwndOwner = window;
                cf.lpLogFont = &lf;
                cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                if (ChooseFontW(&cf)) {
                    assignFont(rasterFont(lf));
                    message("Font embedded; no font installation needed for playback");
                }
            }
            break;
        case FillColor:
        case EdgeColor:
        case TextColor:
            color(id);
            break;
        case Characters:
            if (notification == CBN_SELCHANGE) {
                int next = selected(id);
                if (settlePose()) {
                    character = next;
                    layer = joint = 0;
                    time = 0;
                }
                refresh();
            }
            break;
        case Layers:
            if (notification == CBN_SELCHANGE) {
                int next = selected(id);
                if (settlePose()) {
                    layer = next;
                    joint = 0;
                    time = 0;
                }
                refresh();
            }
            break;
        case Clips:
            if (notification == CBN_SELCHANGE && lay()) {
                int next = selected(id);
                if (settlePose()) {
                    lay()->active = uint32_t(next);
                    time = 0;
                    mark();
                }
                refresh();
            }
            break;
        case Joints:
            if (notification == CBN_SELCHANGE)
                joint = selected(id);
            break;
        case Play:
            if (settlePose()) {
                playing = !playing;
                bind = false;
                refresh();
            }
            break;
        case Restart:
            if (settlePose()) {
                time = 0;
                resetPose();
            }
            break;
        case Rig:
            rig = checked(Rig);
            break;
        case Mesh:
            mesh = checked(Mesh);
            break;
        case BindMode:
            if (settlePose()) {
                bind = checked(BindMode);
                playing = false;
                rig = true;
                refresh();
            } else
                refresh();
            break;
        case Visible:
            if (lay()) {
                lay()->visible = checked(Visible);
                mark();
            }
            break;
        case ComicRate:
            scene.poseRate = checked(ComicRate) ? 12.f : 0.f;
            mark();
            break;
        case SetKey:
            storeKey();
            break;
        case DeleteKey:
            if (clip()) {
                if (!settlePose())
                    break;
                auto &keys = clip()->keys;
                auto nearest = std::min_element(keys.begin(), keys.end(), [&](const Key &a, const Key &b) {
                    return std::abs(a.time - time) < std::abs(b.time - time);
                });
                if (nearest != keys.end() && std::abs(nearest->time - time) <= 1.f / 24) {
                    keys.erase(nearest);
                    mark();
                    message("Key deleted");
                } else
                    message("Scrub within 1/24 second of a key to delete it");
            }
            break;
        case AddJoint:
            if (lay()) {
                if (!bind)
                    throw std::runtime_error("Enable Edit bind rig first; select the new joint's parent.");
                auto &l = *lay();
                if (l.joints.size() >= 64)
                    throw std::runtime_error("Joint limit reached");
                int parent = std::clamp(joint, 0, int(l.joints.size()) - 1);
                Vec2 pos = l.joints[parent].bind + Vec2{35, 0};
                l.joints.push_back({"joint_" + std::to_string(l.joints.size()), parent, pos});
                for (auto &a : l.clips)
                    for (auto &k : a.keys)
                        k.points.push_back(k.points[parent] + Vec2{35, 0});
                joint = int(l.joints.size()) - 1;
                buildMesh(l, scene.images[l.image]);
                mark();
                refresh();
                message("New child joint added; drag it to place the bone");
            }
            break;
        case AddClip:
            if (lay()) {
                if (!settlePose())
                    break;
                auto &l = *lay();
                if (l.clips.size() >= 128)
                    throw std::runtime_error("Clip limit reached");
                Clip a;
                a.name = "clip_" + std::to_string(l.clips.size() + 1);
                while (std::any_of(l.clips.begin(), l.clips.end(),
                                   [&](const Clip &c) { return c.name == a.name; }))
                    a.name += "_";
                std::vector<Vec2> p;
                for (auto &j : l.joints)
                    p.push_back(j.bind);
                a.keys = {{0, p}, {2, p}};
                l.clips.push_back(a);
                l.active = uint32_t(l.clips.size()) - 1;
                time = 0;
                playing = false;
                mark();
                refresh();
            }
            break;
        case ApplyDuration:
            if (clip()) {
                if (!settlePose())
                    break;
                float d = number(Duration);
                if (d <= 0 || d > 3600)
                    throw std::runtime_error("Duration must be greater than 0 and at most 3600 seconds.");
                auto &c = *clip();
                float ratio = d / c.duration;
                for (auto &k : c.keys)
                    k.time *= ratio;
                c.duration = d;
                c.loop = checked(Loop);
                time = std::min(time, d);
                mark();
                message("Duration applied; key times scaled proportionally");
            }
            break;
        case ApplyDialogue:
            if (ch()) {
                ch()->text = get(Dialogue);
                mark();
                message("Dialogue applied");
            }
            break;
        case ApplyProperties:
            if (ch() && lay()) {
                Scene trial = scene;
                auto &c = trial.characters[character];
                c.position = {number(PositionX), number(PositionY)};
                c.layers[layer].scale = number(ImageScale);
                c.bubble.width = number(BubbleWidth);
                c.bubble.offset = {number(BubbleX), number(BubbleY)};
                validate(trial);
                scene = std::move(trial);
                mark();
                message("Placement and bubble size applied");
            }
            break;
        case ApplyNames:
            if (ch() && lay()) {
                Scene trial = scene;
                auto &c = trial.characters[character];
                c.id = get(CharacterName);
                c.layers[layer].id = get(LayerName);
                c.layers[layer].clips[c.layers[layer].active].name = get(ClipName);
                validate(trial);
                scene = std::move(trial);
                mark();
                refresh();
            }
            break;
        case DeleteLayer:
            if (ch() && lay()) {
                if (ch()->layers.size() == 1)
                    throw std::runtime_error("Use Remove character to delete its last image.");
                if (MessageBoxW(window, L"Remove this image layer and its animations?", L"Remove image",
                                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    ch()->layers.erase(ch()->layers.begin() + layer);
                    layer = joint = 0;
                    resetPose();
                    mark();
                    refresh();
                }
            }
            break;
        case DeleteCharacter:
            if (ch() && MessageBoxW(window, L"Remove this character and all its layers?", L"Remove character",
                                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
                scene.characters.erase(scene.characters.begin() + character);
                character = layer = joint = 0;
                resetPose();
                mark();
                refresh();
            }
            break;
        }
        InvalidateRect(canvas, nullptr, FALSE);
    }
    void mouseDown(int x, int y) {
        if (!editor || !lay() || !rig)
            return;
        auto &l = *lay();
        Vec2 p = (renderer.scenePoint(x, y) - ch()->position - l.offset) * (1.f / l.scale);
        auto points = poseDirty ? pose : sample(l, time, 0);
        if (bind) {
            points.clear();
            for (auto &j : l.joints)
                points.push_back(j.bind);
        }
        float best = 14 / (renderer.scale * l.scale);
        int hit = -1;
        for (size_t i = 0; i < points.size(); i++) {
            float dx = p.x - points[i].x, dy = p.y - points[i].y, d = std::sqrt(dx * dx + dy * dy);
            if (d < best) {
                best = d;
                hit = int(i);
            }
        }
        if (hit >= 0) {
            joint = hit;
            playing = false;
            dragging = true;
            if (!bind) {
                pose = std::move(points);
                poseDirty = true;
            }
            SetCapture(canvas);
            refresh();
        }
    }
    void mouseMove(int x, int y) {
        if (!dragging || !lay())
            return;
        auto &l = *lay();
        Vec2 p = (renderer.scenePoint(x, y) - ch()->position - l.offset) * (1.f / l.scale);
        if (bind) {
            Vec2 delta = p - l.joints[joint].bind;
            l.joints[joint].bind = p;
            for (auto &a : l.clips)
                for (auto &k : a.keys)
                    k.points[joint] = k.points[joint] + delta;
            buildMesh(l, scene.images[l.image]);
            mark();
        } else {
            Vec2 delta = p - pose[joint];
            for (size_t j = 0; j < pose.size(); j++) {
                int ancestor = int(j);
                while (ancestor >= 0 && ancestor != joint)
                    ancestor = l.joints[ancestor].parent;
                if (ancestor == joint)
                    pose[j] = pose[j] + delta;
            }
            message("Pose edited — click Set key to keep it");
        }
        InvalidateRect(canvas, nullptr, FALSE);
    }
    void keyboard(WPARAM k) {
        if (k == VK_SPACE) {
            if (editor)
                command(Play);
            else
                playing = !playing;
        } else if (k == VK_HOME) {
            if (!settlePose())
                return;
            time = 0;
            resetPose();
        } else if (k == VK_LEFT || k == VK_RIGHT) {
            if (!settlePose())
                return;
            playing = false;
            time = std::clamp(time + (k == VK_RIGHT ? 1.f : -1.f) / 12, 0.f,
                              animeTest ? 26.9999f
                              : clip()  ? clip()->duration
                                        : 3600.f);
            refresh();
        } else if (k >= '1' && k <= '9' && !editor) {
            if (animeTest) {
                time = float(k - '1') * 3;
                InvalidateRect(canvas, nullptr, FALSE);
                return;
            }
            for (auto &c : scene.characters)
                for (auto &l : c.layers)
                    if (size_t(k - '1') < l.clips.size())
                        l.active = uint32_t(k - '1');
            time = 0;
        } else if (k == VK_ESCAPE) {
            if (poseDirty) {
                resetPose();
                message("Pose edit discarded");
            }
        }
        InvalidateRect(canvas, nullptr, FALSE);
    }
    void tick() {
        bool wasPlaying = playing;
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (playing && !IsIconic(window)) {
            time += animeTest ? dt : std::min(dt, .1f);
            if (animeTest)
                time = std::fmod(time, 27.f);
            if (editor && clip() && time > clip()->duration) {
                if (clip()->loop)
                    time = std::fmod(time, clip()->duration);
                else {
                    time = clip()->duration;
                    playing = false;
                    set(Play, "Play");
                }
            }
        }
        if (editor && clip()) {
            SendMessageW(control(Timeline), TBM_SETPOS, TRUE, LPARAM(int(time / clip()->duration * 1000)));
        }
        if (wasPlaying && !IsIconic(window))
            InvalidateRect(canvas, nullptr, FALSE);
    }
    void draw() {
        if (!initialized)
            return;
        std::ostringstream label;
        float renderTime = time;
        if (animeTest) {
            auto step = applyAnimeDemo(scene, time);
            renderTime = step.localTime;
            label << animeStepLabel(step);
        } else if (editor) {
            label << (bind      ? "BIND RIG"
                      : playing ? "PREVIEW"
                                : "POSE")
                  << "   /   " << std::fixed << std::setprecision(2) << time << " s";
            if (clip()) {
                label << "   /   " << clip()->keys.size() << " keys:";
                for (auto &k : clip()->keys)
                    label << ' ' << std::setprecision(1) << k.time;
            }
            if (poseDirty)
                label << "   /   UNSAVED POSE";
        } else
            label << "SPACE pause   /   1-9 animation   /   HOME restart   /   "
                  << (scene.poseRate > 0 ? "12 fps comic poses" : "smooth poses");
        renderer.draw(scene, renderTime, editor && rig, editor && mesh, character, layer, joint, bind,
                      poseDirty ? &pose : nullptr, label.str(), editor && !playing ? 0.f : -1.f);
    }
    HWND make(const wchar_t *kind, const wchar_t *title, int id, int x, int y, int w, int h,
              DWORD style = 0) {
        HWND c = CreateWindowExW(kind == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, kind, title,
                                 WS_CHILD | WS_VISIBLE | style, x, y, w, h, window, HMENU(INT_PTR(id)),
                                 GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, WPARAM(font), TRUE);
        return c;
    }
    void label(const wchar_t *title, int x, int y, int w = 300) {
        make(L"STATIC", title, 0, x, y, w, 18);
    }
    void button(const wchar_t *title, int id, int x, int y, int w) {
        make(L"BUTTON", title, id, x, y, w, 25, WS_TABSTOP);
    }
    void edit(int id, int x, int y, int w, const wchar_t *title = L"") {
        make(L"EDIT", title, id, x, y, w, 23, WS_TABSTOP | ES_AUTOHSCROLL);
    }
    void controls() {
        font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        label(L"COMIC STUDIO / Scene & animation", 12, 10);
        button(L"Open", Open, 12, 34, 70);
        button(L"Save", Save, 88, 34, 70);
        button(L"Save as", SaveAs, 164, 34, 70);
        button(L"Demo", Demo, 240, 34, 72);
        label(L"Character", 12, 68);
        make(L"COMBOBOX", L"", Characters, 12, 87, 214, 230, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        button(L"+ Image", ImportCharacter, 232, 86, 80);
        label(L"Image layer", 12, 117);
        make(L"COMBOBOX", L"", Layers, 12, 136, 214, 230, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        button(L"+ Layer", ImportLayer, 232, 135, 80);
        label(L"Animation", 12, 166);
        make(L"COMBOBOX", L"", Clips, 12, 185, 214, 230, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        button(L"+ Clip", AddClip, 232, 184, 80);
        label(L"Joint (select parent before adding a joint)", 12, 215);
        make(L"COMBOBOX", L"", Joints, 12, 234, 214, 230, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
        button(L"+ Joint", AddJoint, 232, 233, 80);
        make(L"BUTTON", L"Skeleton", Rig, 12, 267, 88, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
        make(L"BUTTON", L"Mesh", Mesh, 106, 267, 65, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
        make(L"BUTTON", L"Edit bind rig", BindMode, 181, 267, 130, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
        button(L"Pause", Play, 12, 296, 68);
        button(L"Restart", Restart, 84, 296, 72);
        button(L"Set key", SetKey, 160, 296, 72);
        button(L"Delete key", DeleteKey, 236, 296, 76);
        label(L"Seconds", 12, 330, 60);
        edit(Duration, 74, 327, 64);
        make(L"BUTTON", L"Loop", Loop, 147, 327, 64, 24, BS_AUTOCHECKBOX | WS_TABSTOP);
        button(L"Apply time", ApplyDuration, 215, 327, 97);
        label(L"IDs: character / image / animation", 12, 361);
        edit(CharacterName, 12, 381, 95);
        edit(LayerName, 112, 381, 95);
        edit(ClipName, 212, 381, 100);
        button(L"Apply names", ApplyNames, 12, 410, 104);
        make(L"BUTTON", L"Visible", Visible, 122, 411, 75, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
        make(L"BUTTON", L"12 fps poses", ComicRate, 199, 411, 114, 22, BS_AUTOCHECKBOX | WS_TABSTOP);
        label(L"Character X / Y / image scale", 12, 445);
        edit(PositionX, 12, 465, 95);
        edit(PositionY, 112, 465, 95);
        edit(ImageScale, 212, 465, 100);
        label(L"Bubble width / offset X / offset Y", 12, 499);
        edit(BubbleWidth, 12, 519, 95);
        edit(BubbleX, 112, 519, 95);
        edit(BubbleY, 212, 519, 100);
        button(L"Apply placement & bubble size", ApplyProperties, 12, 550, 300);
        label(L"Dialogue (blank hides the bubble)", 12, 587);
        make(L"EDIT", L"", Dialogue, 12, 608, 300, 65,
             WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL);
        SendMessageW(control(Dialogue), EM_SETLIMITTEXT, 16000, 0);
        button(L"Apply text", ApplyDialogue, 12, 680, 145);
        button(L"Character font...", PickFont, 166, 680, 146);
        button(L"Bubble fill", FillColor, 12, 716, 95);
        button(L"Edge", EdgeColor, 112, 716, 95);
        button(L"Text color", TextColor, 212, 716, 100);
        make(L"STATIC", L"Ready", Status, 12, 758, 300, 50);
        make(TRACKBAR_CLASSW, L"", Timeline, panel + 8, 800, 600, 32, WS_TABSTOP | TBS_HORZ | TBS_NOTICKS);
        SendMessageW(control(Timeline), TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
        HMENU menu = CreateMenu(), file = CreatePopupMenu(), assets = CreatePopupMenu();
        AppendMenuW(file, MF_STRING, New, L"New empty scene");
        AppendMenuW(file, MF_STRING, Open, L"Open...\tCtrl+O");
        AppendMenuW(file, MF_STRING, Save, L"Save\tCtrl+S");
        AppendMenuW(file, MF_STRING, SaveAs, L"Save as...");
        AppendMenuW(file, MF_STRING, Manifest, L"Export capability manifest...");
        AppendMenuW(menu, MF_POPUP, UINT_PTR(file), L"File");
        AppendMenuW(assets, MF_STRING, ImportBackground, L"Import background...");
        AppendMenuW(assets, MF_STRING, ImportCharacter, L"Import character image...");
        AppendMenuW(assets, MF_STRING, ImportLayer, L"Add image to character...");
        AppendMenuW(assets, MF_STRING, ExportImage, L"Export selected image as CIMG...");
        AppendMenuW(assets, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(assets, MF_STRING, PickFont, L"Choose character font...");
        AppendMenuW(assets, MF_STRING, ImportFont, L"Import CFNT font...");
        AppendMenuW(assets, MF_STRING, ExportFont, L"Export character font...");
        AppendMenuW(assets, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(assets, MF_STRING, DeleteLayer, L"Remove selected image layer");
        AppendMenuW(assets, MF_STRING, DeleteCharacter, L"Remove selected character");
        AppendMenuW(menu, MF_POPUP, UINT_PTR(assets), L"Assets");
        SetMenu(window, menu);
    }
    void layout() {
        RECT r{};
        GetClientRect(window, &r);
        if (editor) {
            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
            info.nMin = 0;
            info.nMax = 820;
            info.nPage = UINT(r.bottom);
            info.nPos = inspectorScroll;
            SetScrollInfo(window, SB_VERT, &info, TRUE);
            scrollInspector(GetScrollPos(window, SB_VERT));
            GetClientRect(window, &r);
            MoveWindow(canvas, panel, 0, std::max(1L, r.right - panel), std::max(1L, r.bottom - 44), TRUE);
            MoveWindow(control(Timeline), panel + 8, std::max(0L, r.bottom - 40),
                       std::max(1L, r.right - panel - 16), 32, TRUE);
        } else
            MoveWindow(canvas, 0, 0, r.right, r.bottom, TRUE);
    }
    void scrollInspector(int next) {
        RECT client{};
        GetClientRect(window, &client);
        next = std::clamp(next, 0, std::max(0, 821 - int(client.bottom)));
        int delta = inspectorScroll - next;
        inspectorScroll = next;
        SetScrollPos(window, SB_VERT, next, TRUE);
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (child == canvas || child == control(Timeline))
                continue;
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
            SetWindowPos(child, nullptr, r.left, r.top + delta, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        RECT panelRect{0, 0, panel, client.bottom};
        InvalidateRect(window, &panelRect, TRUE);
    }
};
App app;
void error(const std::exception &e) {
    if (app.smokeFrames > 0) {
        app.exitCode = 1;
        PostQuitMessage(1);
    } else
        MessageBoxW(app.window, wide(e.what()).c_str(), L"Comic Engine", MB_OK | MB_ICONERROR);
}
LRESULT CALLBACK CanvasProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    try {
        switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            app.draw();
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetFocus(h);
            app.mouseDown(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            return 0;
        case WM_MOUSEMOVE:
            app.mouseMove(GET_X_LPARAM(l), GET_Y_LPARAM(l));
            return 0;
        case WM_LBUTTONUP:
            app.dragging = false;
            ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            app.dragging = false;
            return 0;
        case WM_KEYDOWN:
            app.keyboard(w);
            return 0;
        }
    } catch (const std::exception &e) {
        error(e);
    }
    return DefWindowProcW(h, m, w, l);
}
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    try {
        switch (m) {
        case WM_SIZE:
            if (app.canvas)
                app.layout();
            return 0;
        case WM_GETMINMAXINFO:
            if (editor) {
                auto *p = reinterpret_cast<MINMAXINFO *>(l);
                p->ptMinTrackSize = {900, 600};
            }
            return 0;
        case WM_COMMAND:
            app.command(LOWORD(w), HIWORD(w));
            return 0;
        case WM_TIMER:
            app.tick();
            return 0;
        case WM_VSCROLL:
            if (editor) {
                int next = app.inspectorScroll;
                switch (LOWORD(w)) {
                case SB_LINEUP:
                    next -= 32;
                    break;
                case SB_LINEDOWN:
                    next += 32;
                    break;
                case SB_PAGEUP:
                    next -= 240;
                    break;
                case SB_PAGEDOWN:
                    next += 240;
                    break;
                case SB_THUMBTRACK:
                    next = HIWORD(w);
                    break;
                }
                app.scrollInspector(next);
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (editor)
                app.scrollInspector(app.inspectorScroll - GET_WHEEL_DELTA_WPARAM(w) / 3);
            return 0;
        case WM_HSCROLL:
            if (editor && app.clip() && HWND(l) == app.control(Timeline)) {
                if (app.settlePose()) {
                    app.playing = false;
                    app.time = float(SendMessageW(app.control(Timeline), TBM_GETPOS, 0, 0)) / 1000 *
                               app.clip()->duration;
                    app.resetPose();
                    app.set(Play, "Play");
                    InvalidateRect(app.canvas, nullptr, FALSE);
                }
                return 0;
            }
            break;
        case WM_KEYDOWN:
            app.keyboard(w);
            return 0;
        case WM_CLOSE:
            if (!editor || app.leave()) {
                KillTimer(h, 1);
                app.initialized = false;
                app.renderer.close(app.canvas);
                DestroyWindow(h);
            }
            return 0;
        case WM_DESTROY:
            if (app.font)
                DeleteObject(app.font);
            PostQuitMessage(app.exitCode);
            return 0;
        }
    } catch (const std::exception &e) {
        error(e);
    }
    return DefWindowProcW(h, m, w, l);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
    InitCommonControlsEx(&common);
    try {
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::filesystem::path initial;
        bool createDemo = false, createAnime = false;
        for (int i = 1; i < argc; i++) {
            if (std::wstring(argv[i]) == L"--make-demo")
                createDemo = true;
            else if (std::wstring(argv[i]) == L"--make-anime-demo")
                createAnime = true;
            else if (std::wstring(argv[i]) == L"--anime-test")
                app.animeTest = true;
            else if (std::wstring(argv[i]) == L"--smoke" && i + 1 < argc) {
                app.screenshot = argv[++i];
                app.smokeFrames = 5;
            } else
                initial = argv[i];
        }
        LocalFree(argv);
        wchar_t exePath[32768]{};
        GetModuleFileNameW(nullptr, exePath, 32768);
        auto projectRoot = std::filesystem::path(exePath).parent_path().parent_path();
        if (createAnime) {
            auto art = projectRoot / L"samples" / L"anime";
            std::array<Image, 3> images = {importImage(art / L"standing.png", 768),
                                           importImage(art / L"seated.png", 768),
                                           importImage(art / L"kneeling.png", 768)};
            if (initial.empty())
                initial = projectRoot / L"samples" / L"anime-test.cscene";
            saveScene(animeDemo(rasterFont(defaultFont()), std::move(images)), initial);
            CoUninitialize();
            return 0;
        }
        if (app.animeTest && editor)
            throw std::runtime_error(
                "Use ComicPlayer for the timed anime test; open the scene normally in Studio to edit it.");
        if (app.animeTest && initial.empty())
            initial = projectRoot / L"samples" / L"anime-test.cscene";
        if (createDemo) {
            if (initial.empty())
                initial = L"sample.cscene";
            saveScene(demo(rasterFont(defaultFont())), initial);
            CoUninitialize();
            return 0;
        }
        WNDCLASSW wc{};
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"ComicEngineWindow";
        wc.lpfnWndProc = WindowProc;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassW(&wc);
        wc.style = CS_OWNDC;
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"ComicEngineCanvas";
        wc.lpfnWndProc = CanvasProc;
        RegisterClassW(&wc);
        app.window =
            CreateWindowExW(0, L"ComicEngineWindow", editor ? L"Comic Studio" : L"Comic Player",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                            editor ? 1320 : 1000, editor ? 910 : 730, nullptr, nullptr, instance, nullptr);
        if (!app.window)
            throw std::runtime_error("Cannot create application window");
        app.canvas = CreateWindowExW(0, L"ComicEngineCanvas", L"", WS_CHILD | WS_VISIBLE, 0, 0, 800, 600,
                                     app.window, nullptr, instance, nullptr);
        app.renderer.initialize(app.canvas);
        if (editor)
            app.controls();
        app.layout();
        if (initial.empty()) {
            wchar_t exe[32768]{};
            GetModuleFileNameW(nullptr, exe, 32768);
            auto bundled =
                std::filesystem::path(exe).parent_path().parent_path() / L"samples" / L"midnight.cscene";
            if (std::filesystem::exists(bundled))
                initial = bundled;
        }
        if (initial.empty())
            app.install(demo(rasterFont(defaultFont())));
        else
            app.install(loadScene(initial), initial);
        if (app.animeTest)
            validateAnimeDemo(app.scene);
        app.initialized = true;
        if (app.smokeFrames) {
            app.renderer.capturePath = app.screenshot;
            app.draw();
            if (app.animeTest) {
                std::ofstream log(app.screenshot.string() + ".timeline.csv");
                log << "start_seconds,end_seconds,pose,animation,visible_layers\n";
                for (int segment = 0; segment < 9; ++segment) {
                    app.time = segment * 3.f + 1.125f;
                    app.renderer.capturePath =
                        app.screenshot.string() + ".motion-" + std::to_string(segment + 1) + ".ppm";
                    app.draw();
                    auto step = animeStep(app.time);
                    int visible = 0;
                    for (auto &layer : app.scene.characters[0].layers)
                        visible += layer.visible ? 1 : 0;
                    if (visible != 1)
                        throw std::runtime_error("Anime test must show exactly one pose");
                    log << segment * 3 << ',' << (segment + 1) * 3 << ','
                        << app.scene.characters[0].layers[step.pose].id << ','
                        << app.scene.characters[0].layers[step.pose].clips[step.animation].name << ','
                        << visible << '\n';
                }
                app.time = 27;
                auto wrapped = applyAnimeDemo(app.scene, app.time);
                if (wrapped.pose != 0 || wrapped.animation != 0 || wrapped.localTime != 0)
                    throw std::runtime_error("Anime sequence did not loop at 27 seconds");
                if (!log)
                    throw std::runtime_error("Could not write anime smoke-test log");
            } else if (editor) {
                app.playing = false;
                app.time = .5f;
                auto point = sample(*app.lay(), app.time, 0)[2];
                point = app.ch()->position + app.lay()->offset + point * app.lay()->scale;
                int x = int(point.x * app.renderer.scale + app.renderer.originX);
                int y = int(point.y * app.renderer.scale + app.renderer.originY);
                app.mouseDown(x, y);
                app.mouseMove(x + 12, y);
                app.dragging = false;
                ReleaseCapture();
                if (!app.poseDirty || app.joint != 2)
                    throw std::runtime_error("Editor joint picking failed");
                app.storeKey();
                app.bind = true;
                app.joint = 2;
                app.command(AddJoint);
                app.bind = false;
                app.set(Duration, "4");
                app.command(ApplyDuration);
                app.set(Dialogue, "Smoke test dialogue");
                app.command(ApplyDialogue);
                app.path = app.screenshot;
                app.path += L".cscene";
                app.save();
                auto reloaded = loadScene(app.path);
                if (reloaded.characters[0].text != "Smoke test dialogue" ||
                    reloaded.characters[0].layers[0].joints.size() != 14 ||
                    reloaded.characters[0].layers[0].clips[0].keys.size() != 4)
                    throw std::runtime_error("Editor smoke test failed");
            } else if (play(app.scene, "atlas", "body", "wave")) {
                app.time = .35f;
                app.renderer.capturePath = app.screenshot;
                app.renderer.capturePath += L".wave.ppm";
                app.draw();
            }
            app.initialized = false;
            app.renderer.close(app.canvas);
            DestroyWindow(app.window);
            CoUninitialize();
            return 0;
        }
        app.last = std::chrono::steady_clock::now();
        SetTimer(app.window, 1, 16, nullptr);
        ShowWindow(app.window, app.smokeFrames ? SW_SHOWNOACTIVATE : show);
        UpdateWindow(app.window);
        ACCEL shortcuts[] = {{FVIRTKEY | FCONTROL, 'S', Save}, {FVIRTKEY | FCONTROL, 'O', Open}};
        HACCEL accelerators = CreateAcceleratorTableW(shortcuts, 2);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (editor && TranslateAcceleratorW(app.window, accelerators, &msg))
                continue;
            if (editor && IsDialogMessageW(app.window, &msg))
                continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        DestroyAcceleratorTable(accelerators);
        CoUninitialize();
        return int(msg.wParam);
    } catch (const std::exception &e) {
        if (!app.screenshot.empty()) {
            std::ofstream f(app.screenshot.string() + ".error.txt");
            f << e.what();
        } else
            MessageBoxW(nullptr, wide(e.what()).c_str(), L"Comic Engine could not start",
                        MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
}
