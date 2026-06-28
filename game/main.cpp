// ---------------------------------------------------------------------------
// 3D-SDL-PLOTTER-GAME  ::  ray-traced FPS + in-game level editor
// ---------------------------------------------------------------------------
// A real-time first-person shooter rendered entirely with the SDL-free ray
// tracing library in this repo, plus a built-in map editor and a fully
// data-driven content pipeline:
//
//   assets/materials.def  -> surface looks (colors, textures, finishes)
//   assets/prefabs.def     -> enemy / weapon / prop "designs"
//   maps/*.map             -> levels that reference those names
//
// Press F1 to flip between PLAY and EDIT. In edit mode you fly around and drop
// in walls, props, enemies, weapons and lights using a brush built from the
// asset library, then F5 to save the map back to disk. Nothing here is
// hard-coded: add a material or a prefab to a .def file and it becomes
// placeable immediately.
//
//   fps_game [path/to/level.map]
// ---------------------------------------------------------------------------

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
#include "Map.h"
#include "Player.h"
#include "World.h"

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

namespace {

// Window size (what you see) vs render size (what we ray trace). The renderer
// fills the smaller buffer and SDL upscales it, which is what keeps a
// per-pixel ray traced game interactive.
constexpr int kWindowW = 960;
constexpr int kWindowH = 540;
constexpr int kRenderW = 480;
constexpr int kRenderH = 270;

constexpr double kEyeHeight = 1.2;

// Nearest live enemy hit by `ray`, or nullptr. Uses the same analytic
// ray-sphere test the renderer uses but only against enemies, so we know which
// one was struck (the renderer's HitRecord does not expose the object).
LiveEnemy* pickEnemy(const Ray& ray, std::vector<LiveEnemy>& enemies) {
    LiveEnemy* best = nullptr;
    double bestT = std::numeric_limits<double>::infinity();
    HitRecord rec;
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.sphere->hit(ray, 0.001, bestT, rec)) {
            bestT = rec.t;
            best = &e;
        }
    }
    return best;
}

void drawCrosshair(SDLViewport& viewport, bool editMode) {
    SDL_Renderer* r = viewport.renderer();
    int cx = viewport.windowWidth()  / 2;
    int cy = viewport.windowHeight() / 2;
    const int arm = 10, gap = 4;
    if (editMode) SDL_SetRenderDrawColor(r, 120, 230, 120, 230);   // green = edit
    else          SDL_SetRenderDrawColor(r, 255, 255, 255, 220);
    SDL_RenderDrawLine(r, cx - arm, cy, cx - gap, cy);
    SDL_RenderDrawLine(r, cx + gap, cy, cx + arm, cy);
    SDL_RenderDrawLine(r, cx, cy - arm, cx, cy - gap);
    SDL_RenderDrawLine(r, cx, cy + gap, cx, cy + arm);
}

} // namespace

int main(int argc, char* argv[]) {
    // ----- Resolve content paths ------------------------------------------
    const std::string root = PROJECT_ROOT;
    const std::string assetsDir = root + "/assets";
    std::string mapPath = (argc > 1) ? argv[1] : (root + "/maps/arena.map");

    // ----- Load the data-driven content -----------------------------------
    AssetLibrary assets;
    assets.baseDir = assetsDir;
    assets.loadMaterials(assetsDir + "/materials.def");
    assets.loadPrefabs(assetsDir + "/prefabs.def");

    Map map;
    if (!map.load(mapPath)) {
        std::cerr << "[game] starting from an empty map (" << mapPath << ")\n";
    }

    // ----- SDL + renderer --------------------------------------------------
    SDLViewport viewport("Ray-Traced FPS + Editor", kWindowW, kWindowH,
                         kRenderW, kRenderH);
    if (!viewport.valid()) {
        std::cerr << "Failed to create SDL viewport: " << SDL_GetError() << "\n";
        return 1;
    }
    std::vector<uint32_t> pixels(size_t(kRenderW) * kRenderH);

    Renderer tracer(kRenderW, kRenderH);
    auto fpsCam = std::make_shared<PerspectiveCamera>(
        Vec3(0, kEyeHeight, 0), Vec3(0, kEyeHeight, -1), Vec3(0, 1, 0), 75.0);
    tracer.camera = fpsCam;
    tracer.shader = std::make_shared<PhongShader>();
    tracer.maxDepth = 3;
    tracer.samples  = 1;

    World world = instantiate(map, assets, tracer);

    Player player(world.spawn, world.spawnYaw, 0.0);
    Editor editor(assets);

    bool editMode = false;
    int  score    = 0;
    int  remaining = world.enemyCount;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    auto rebuildWorld = [&]() {
        world = instantiate(map, assets, tracer);
        remaining = world.enemyCount;
    };

    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());

    auto updateTitle = [&](double fps) {
        std::string s;
        if (editMode) {
            s  = "EDIT  |  brush: " + editor.categoryName() + " / " + editor.itemName();
            s += editor.snapping() ? "  [grid]" : "  [free]";
            s += "  |  LMB place  RMB delete  [ ] category  , . item  G grid"
                 "  F5 save  F9 reload  F1 play";
        } else {
            s  = "PLAY  |  score " + std::to_string(score);
            s += "  |  enemies left " + std::to_string(remaining);
            if (remaining == 0 && world.enemyCount > 0) s += "   *** CLEARED ***";
            s += "  |  " + std::to_string(int(fps + 0.5)) + " fps";
            s += "   [WASD move  mouse look  LMB shoot  F1 edit  Esc quit]";
        }
        viewport.setTitle(s.c_str());
    };

    bool quit = false;
    while (!quit) {
        // ----- Timing ------------------------------------------------------
        Uint64 now = SDL_GetPerformanceCounter();
        double dt  = double(now - prev) / freq;
        prev = now;
        if (dt > 0.1) dt = 0.1;

        // ----- Input -------------------------------------------------------
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    quit = true;
                    break;

                case SDL_MOUSEMOTION:
                    player.addLook(e.motion.xrel, e.motion.yrel);
                    break;

                case SDL_MOUSEWHEEL:
                    if (editMode) {
                        if (SDL_GetModState() & KMOD_SHIFT)
                            editor.adjustHeight(e.wheel.y * 0.25);
                        else
                            editor.nextItem(e.wheel.y > 0 ? 1 : -1);
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (editMode) {
                        if (e.button.button == SDL_BUTTON_LEFT)
                            editor.place(player.aimRay(), map);
                        else if (e.button.button == SDL_BUTTON_RIGHT)
                            editor.deleteNearest(player.aimRay(), map);
                        if (editor.dirty) { rebuildWorld(); editor.dirty = false; }
                    } else if (e.button.button == SDL_BUTTON_LEFT) {
                        if (LiveEnemy* hit = pickEnemy(player.aimRay(), world.enemies)) {
                            if (--hit->hp <= 0) {
                                hit->alive = false;
                                auto& objs = tracer.scene.objects;
                                objs.erase(std::remove(objs.begin(), objs.end(),
                                    std::static_pointer_cast<Hittable>(hit->sphere)),
                                    objs.end());
                                ++score;
                                --remaining;
                            }
                        }
                    }
                    break;

                case SDL_KEYDOWN:
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;

                        case SDLK_F1:
                            editMode = !editMode;
                            if (!editMode) {                 // back to play
                                player.position = world.spawn;
                                player.position.y = kEyeHeight;
                            }
                            break;

                        // ----- Editor controls -----
                        case SDLK_LEFTBRACKET:  if (editMode) editor.nextCategory(-1); break;
                        case SDLK_RIGHTBRACKET: if (editMode) editor.nextCategory(+1); break;
                        case SDLK_COMMA:        if (editMode) editor.nextItem(-1); break;
                        case SDLK_PERIOD:       if (editMode) editor.nextItem(+1); break;
                        case SDLK_g:            if (editMode) editor.toggleSnap(); break;
                        case SDLK_F5:
                            if (editMode && map.save(mapPath))
                                std::cout << "[game] map saved\n";
                            break;
                        case SDLK_F9:
                            if (editMode && map.load(mapPath)) rebuildWorld();
                            break;

                        // ----- Play controls -----
                        case SDLK_r:
                            if (!editMode) {
                                rebuildWorld();
                                score = 0;
                                player.position = world.spawn;
                                player.position.y = kEyeHeight;
                            }
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
        }

        // ----- Movement ----------------------------------------------------
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Vec3 move(0, 0, 0);
        if (keys[SDL_SCANCODE_W]) move += player.forwardFlat();
        if (keys[SDL_SCANCODE_S]) move += -player.forwardFlat();
        if (keys[SDL_SCANCODE_D]) move += player.right();
        if (keys[SDL_SCANCODE_A]) move += -player.right();

        if (editMode) {
            // Free-fly: no collision, Q/E change altitude.
            Vec3 fly = move;
            if (keys[SDL_SCANCODE_E]) fly += Vec3(0, 1, 0);
            if (keys[SDL_SCANCODE_Q]) fly += Vec3(0, -1, 0);
            if (fly.length_squared() > 1e-9)
                player.position += unit_vector(fly) * (player.moveSpeed * 1.6 * dt);
        } else {
            if (move.length_squared() > 1e-9) {
                move = unit_vector(move) * (player.moveSpeed * dt);
                moveWithCollision(player, move, world.walls);
            }
            player.position.y = kEyeHeight;

            // Enemies drift toward the player on the ground plane.
            for (auto& en : world.enemies) {
                if (!en.alive || en.speed <= 0.0) continue;
                Vec3 to = player.position - en.sphere->center;
                to.y = 0.0;
                double d = to.length();
                if (d > 0.8) {
                    en.sphere->center += (to / d) * (en.speed * dt);
                }
            }
        }
        player.applyToCamera(*fpsCam);

        // ----- Render ------------------------------------------------------
        tracer.render(pixels.data());
        viewport.beginFrame(pixels.data());
        drawCrosshair(viewport, editMode);
        viewport.endFrame();

        updateTitle(dt > 0.0 ? 1.0 / dt : 0.0);
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    return 0;
}
