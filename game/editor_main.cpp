// ---------------------------------------------------------------------------
// 3D-SDL-PLOTTER-GAME  ::  level_editor  (the "dev view")
// ---------------------------------------------------------------------------
// An Unreal-style editor for building levels that the main game (fps_game)
// then loads and plays. The 3D viewport IS the ray tracer; editor chrome
// (toolbar / palette / outliner / details) is drawn on top via EditorUI.
//
// Navigation (Unreal-like): hold RIGHT mouse to look, WASD + Q/E to fly. Left
// mouse runs the active tool in the viewport, or clicks the panels.
//
//   Tools:   1 Select   2 Place   3 Delete   (or the toolbar buttons)
//   Place:   pick a category + item in the palette, click the floor to drop it
//   Select:  click an object; arrows move it, PgUp/PgDn raise/lower,
//            - / = scale, Ctrl+D duplicate, Delete remove
//   View:    T toggles top-down orthographic, G toggles grid snap
//   File:    Ctrl+S / F5 save, Ctrl+N new
//   Test:    P plays the level in-editor; Esc returns to the editor
//   Esc:     quit (from the editor)
//
//   level_editor [path/to/level.map]
// ---------------------------------------------------------------------------

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <raytracer/RayTracer.h>
#include <raytracer/SDLViewport.h>
#include <raytracer/Sphere.h>
#include <raytracer/Camera.h>
#include <raytracer/Shader.h>

#include "Assets.h"
#include "Editor.h"
#include "EditorCamera.h"
#include "EditorUI.h"
#include "Map.h"
#include "Overlay.h"
#include "Player.h"
#include "World.h"

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;
constexpr int kRenderW = 512;
constexpr int kRenderH = 288;
constexpr double kEyeHeight = 1.2;

LiveEnemy* pickEnemy(const Ray& ray, std::vector<LiveEnemy>& enemies) {
    LiveEnemy* best = nullptr;
    double bestT = std::numeric_limits<double>::infinity();
    HitRecord rec;
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.sphere->hit(ray, 0.001, bestT, rec)) { bestT = rec.t; best = &e; }
    }
    return best;
}

void drawPlayHUD(SDL_Renderer* r, int score, int remaining, int total) {
    ui::panel(r, 12, 12, 240, 56, ui::theme::panelDark());
    ui::text(r, 22, 20, "PLAY MODE", ui::theme::good(), 2);
    ui::text(r, 22, 40, "SCORE " + std::to_string(score) +
                        "   LEFT " + std::to_string(remaining), ui::theme::text(), 2);
    if (total > 0 && remaining == 0)
        ui::text(r, 22, 56, "CLEARED - ESC TO EDIT", ui::theme::warn(), 1);
    else
        ui::text(r, 22, 56, "ESC TO RETURN TO EDITOR", ui::theme::textDim(), 1);
    // Crosshair.
    int cx = 1280 / 2, cy = 720 / 2, a = 10, g = 4;
    ui::setColor(r, ui::rgb(255, 255, 255, 220));
    SDL_RenderDrawLine(r, cx - a, cy, cx - g, cy);
    SDL_RenderDrawLine(r, cx + g, cy, cx + a, cy);
    SDL_RenderDrawLine(r, cx, cy - a, cx, cy - g);
    SDL_RenderDrawLine(r, cx, cy + g, cx, cy + a);
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string root = PROJECT_ROOT;
    const std::string assetsDir = root + "/assets";
    std::string mapPath = (argc > 1) ? argv[1] : (root + "/maps/arena.map");

    AssetLibrary assets;
    assets.baseDir = assetsDir;
    assets.loadMaterials(assetsDir + "/materials.def");
    assets.loadPrefabs(assetsDir + "/prefabs.def");

    Map map;
    map.load(mapPath);   // ok if missing; you can Save to create it

    SDLViewport viewport("Level Editor - dev view", kWindowW, kWindowH,
                         kRenderW, kRenderH);
    if (!viewport.valid()) {
        std::cerr << "Failed to create SDL viewport: " << SDL_GetError() << "\n";
        return 1;
    }
    std::vector<uint32_t> pixels(size_t(kRenderW) * kRenderH);
    SDL_Renderer* gpu = viewport.renderer();

    Renderer tracer(kRenderW, kRenderH);
    tracer.shader = std::make_shared<PhongShader>();
    tracer.maxDepth = 3;
    tracer.samples  = 1;

    Editor editor(assets);
    EditorCamera cam;
    cam.setAspect(double(kWindowW) / double(kWindowH));
    EditorUI gui(kWindowW, kWindowH);

    bool playing = false;
    auto fpsCam = std::make_shared<PerspectiveCamera>(
        Vec3(0, kEyeHeight, 0), Vec3(0, kEyeHeight, -1), Vec3(0, 1, 0), 75.0);
    Player player(map.spawn, map.spawnYaw, 0.0);
    int score = 0, remaining = 0;

    World world;
    auto rebuildEditor = [&]() {
        world = instantiate(map, assets, tracer, editor.selKind, editor.selIndex);
        tracer.camera = cam.makeRenderCamera();
    };
    auto rebuildPlay = [&]() {
        world = instantiate(map, assets, tracer);
        remaining = world.enemyCount;
        tracer.camera = fpsCam;
    };
    rebuildEditor();

    int  mouseX = kWindowW / 2, mouseY = kWindowH / 2;
    bool looking = false;
    int  outlinerScroll = 0;
    bool clickPending = false;
    std::string status = "loaded " + mapPath;

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());

    auto enterPlay = [&]() {
        playing = true;
        player = Player(map.spawn, map.spawnYaw, 0.0);
        score = 0;
        rebuildPlay();
        SDL_SetRelativeMouseMode(SDL_TRUE);
        status = "playing";
    };
    auto exitPlay = [&]() {
        playing = false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        rebuildEditor();
        status = "back in editor";
    };
    auto moveStep = [&]() { return editor.snap ? 1.0 : 0.25; };

    bool quit = false;
    while (!quit) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = double(now - prev) / freq;
        prev = now;
        if (dt > 0.1) dt = 0.1;
        bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: quit = true; break;

                case SDL_MOUSEMOTION:
                    if (playing) player.addLook(e.motion.xrel, e.motion.yrel);
                    else if (looking) cam.addLook(e.motion.xrel, e.motion.yrel);
                    else { mouseX = e.motion.x; mouseY = e.motion.y; }
                    break;

                case SDL_MOUSEWHEEL:
                    if (!playing) {
                        if (SDL_GetModState() & KMOD_SHIFT)
                            editor.placeHeight = std::max(0.0, editor.placeHeight + e.wheel.y * 0.25);
                        else if (mouseX < gui.leftW())
                            outlinerScroll -= e.wheel.y;
                        else
                            editor.nextItem(e.wheel.y > 0 ? 1 : -1);
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (playing) {
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            if (LiveEnemy* hit = pickEnemy(player.aimRay(), world.enemies)) {
                                if (--hit->hp <= 0) {
                                    hit->alive = false;
                                    auto& o = tracer.scene.objects;
                                    o.erase(std::remove(o.begin(), o.end(),
                                        std::static_pointer_cast<Hittable>(hit->sphere)), o.end());
                                    ++score; --remaining;
                                }
                            }
                        }
                        break;
                    }
                    if (e.button.button == SDL_BUTTON_RIGHT) {
                        looking = true;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    } else if (e.button.button == SDL_BUTTON_LEFT) {
                        if (gui.inPanels(mouseX, mouseY)) {
                            clickPending = true;   // resolved during UI draw
                        } else {
                            Ray r = cam.pickRay(mouseX, mouseY, kWindowW, kWindowH);
                            if (editor.tool == Editor::Tool::Place) editor.place(r, map);
                            else if (editor.tool == Editor::Tool::Delete) editor.deleteAt(r, map);
                            else editor.selectAt(r, map);
                            if (editor.dirty) { rebuildEditor(); editor.dirty = false; }
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_RIGHT && looking) {
                        looking = false;
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    }
                    break;

                case SDL_KEYDOWN: {
                    SDL_Keycode k = e.key.keysym.sym;
                    if (playing) { if (k == SDLK_ESCAPE) exitPlay(); break; }
                    switch (k) {
                        case SDLK_ESCAPE: quit = true; break;
                        case SDLK_1: editor.setTool(Editor::Tool::Select); break;
                        case SDLK_2: editor.setTool(Editor::Tool::Place);  break;
                        case SDLK_3: editor.setTool(Editor::Tool::Delete); break;
                        case SDLK_LEFTBRACKET:  editor.nextCategory(-1); break;
                        case SDLK_RIGHTBRACKET: editor.nextCategory(+1); break;
                        case SDLK_COMMA:  editor.nextItem(-1); break;
                        case SDLK_PERIOD: editor.nextItem(+1); break;
                        case SDLK_g: editor.toggleSnap(); break;
                        case SDLK_t:
                            cam.mode = (cam.mode == EditorCamera::Mode::Fly)
                                ? EditorCamera::Mode::TopDown : EditorCamera::Mode::Fly;
                            break;
                        case SDLK_p: case SDLK_F2: enterPlay(); break;
                        case SDLK_F5:
                            if (map.save(mapPath)) status = "saved " + mapPath; break;
                        case SDLK_s:
                            if (ctrl && map.save(mapPath)) status = "saved " + mapPath; break;
                        case SDLK_n:
                            if (ctrl) { map = Map(); editor.clearSelection(); status = "new map"; } break;
                        case SDLK_d:
                            if (ctrl) editor.duplicateSelected(map); break;
                        case SDLK_DELETE: case SDLK_BACKSPACE:
                            editor.deleteSelected(map); break;
                        case SDLK_UP:    editor.moveSelected(cam.forwardFlat() * moveStep(), map); break;
                        case SDLK_DOWN:  editor.moveSelected(-cam.forwardFlat() * moveStep(), map); break;
                        case SDLK_RIGHT: editor.moveSelected(cam.right() * moveStep(), map); break;
                        case SDLK_LEFT:  editor.moveSelected(-cam.right() * moveStep(), map); break;
                        case SDLK_PAGEUP:   editor.moveSelected(Vec3(0, moveStep(), 0), map); break;
                        case SDLK_PAGEDOWN: editor.moveSelected(Vec3(0, -moveStep(), 0), map); break;
                        case SDLK_MINUS: editor.scaleSelected(0.8, map); break;
                        case SDLK_EQUALS: case SDLK_PLUS: editor.scaleSelected(1.25, map); break;
                        default: break;
                    }
                    if (editor.dirty) { rebuildEditor(); editor.dirty = false; }
                    break;
                }
                default: break;
            }
        }

        // ----- movement ----------------------------------------------------
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (playing) {
            Vec3 move(0, 0, 0);
            if (keys[SDL_SCANCODE_W]) move += player.forwardFlat();
            if (keys[SDL_SCANCODE_S]) move += -player.forwardFlat();
            if (keys[SDL_SCANCODE_D]) move += player.right();
            if (keys[SDL_SCANCODE_A]) move += -player.right();
            if (move.length_squared() > 1e-9) {
                move = unit_vector(move) * (player.moveSpeed * dt);
                moveWithCollision(player, move, world.walls);
            }
            player.position.y = kEyeHeight;
            for (auto& en : world.enemies) {
                if (!en.alive || en.speed <= 0.0) continue;
                Vec3 to = player.position - en.sphere->center; to.y = 0.0;
                double d = to.length();
                if (d > 0.8) en.sphere->center += (to / d) * (en.speed * dt);
            }
            player.applyToCamera(*fpsCam);
        } else {
            Vec3 fly(0, 0, 0);
            if (keys[SDL_SCANCODE_W]) fly += cam.forward();
            if (keys[SDL_SCANCODE_S]) fly += -cam.forward();
            if (keys[SDL_SCANCODE_D]) fly += cam.right();
            if (keys[SDL_SCANCODE_A]) fly += -cam.right();
            if (keys[SDL_SCANCODE_E]) fly += Vec3(0, 1, 0);
            if (keys[SDL_SCANCODE_Q]) fly += Vec3(0, -1, 0);
            if (fly.length_squared() > 1e-9)
                cam.position += unit_vector(fly) * (cam.flySpeed * dt);
            tracer.camera = cam.makeRenderCamera();
        }

        // ----- render ------------------------------------------------------
        tracer.render(pixels.data());
        viewport.beginFrame(pixels.data());

        if (playing) {
            drawPlayHUD(gpu, score, remaining, world.enemyCount);
        } else {
            UIResult res = gui.draw(gpu, editor, cam, map, outlinerScroll,
                                    mouseX, mouseY, clickPending, mapPath, status);
            clickPending = false;

            using A = UIResult::Action;
            switch (res.action) {
                case A::ToolSelect: editor.setTool(Editor::Tool::Select); break;
                case A::ToolPlace:  editor.setTool(Editor::Tool::Place);  break;
                case A::ToolDelete: editor.setTool(Editor::Tool::Delete); break;
                case A::ToggleSnap: editor.toggleSnap(); break;
                case A::ToggleTopDown:
                    cam.mode = (cam.mode == EditorCamera::Mode::Fly)
                        ? EditorCamera::Mode::TopDown : EditorCamera::Mode::Fly; break;
                case A::Save: if (map.save(mapPath)) status = "saved " + mapPath; break;
                case A::NewMap: map = Map(); editor.clearSelection(); status = "new map"; break;
                case A::Play: enterPlay(); break;
                case A::PickCategory:
                    while (int(editor.category) != res.arg) editor.nextCategory(1); break;
                case A::PickItem: editor.itemIndex = res.arg; break;
                case A::PickOutliner: editor.select(res.arg, res.arg2); break;
                case A::Duplicate: editor.duplicateSelected(map); break;
                case A::DeleteSel: editor.deleteSelected(map); break;
                default: break;
            }
            if (editor.dirty) { rebuildEditor(); editor.dirty = false; }
        }
        viewport.endFrame();
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    return 0;
}
