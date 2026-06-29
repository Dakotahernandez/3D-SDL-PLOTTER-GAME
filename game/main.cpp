// ---------------------------------------------------------------------------
// 3D-SDL-PLOTTER-GAME  ::  fps_game  (the main app)
// ---------------------------------------------------------------------------
// Boots into a level-select MENU that lists every maps/*.map, then PLAYS the
// chosen level. Levels are built in the separate `level_editor` dev view and
// saved as .map files; this app loads and plays them. Everything is rendered
// with the SDL-free ray tracer.
//
//   States:
//     MENU  - pick a level (mouse or up/down + Enter), Esc quits
//     PLAY  - WASD move, mouse look, LMB shoot enemies; clear them all to win
//             R resets, M / Esc returns to the menu
//
//   fps_game [path/to/level.map]      # skip the menu and play one level
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
#include "LevelBrowser.h"
#include "Map.h"
#include "Overlay.h"
#include "Player.h"
#include "World.h"

#ifndef PROJECT_ROOT
#define PROJECT_ROOT "."
#endif

namespace {

constexpr int kWindowW = 1024;
constexpr int kWindowH = 576;
constexpr int kRenderW = 512;
constexpr int kRenderH = 288;
constexpr double kEyeHeight = 1.2;

enum class State { Menu, Play };

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

void drawPlayHUD(SDL_Renderer* r, int score, int remaining, int total, double fps) {
    ui::panel(r, 12, 12, 260, 56, ui::theme::panelDark());
    ui::text(r, 22, 20, "SCORE " + std::to_string(score), ui::theme::text(), 2);
    ui::text(r, 22, 40, "ENEMIES " + std::to_string(remaining), ui::theme::text(), 2);
    if (total > 0 && remaining == 0)
        ui::text(r, 22, 56, "LEVEL CLEARED - M FOR MENU", ui::theme::good(), 1);
    else
        ui::text(r, 22, 56, "WASD MOVE  LMB SHOOT  M MENU  R RESET", ui::theme::textDim(), 1);

    std::string f = std::to_string(int(fps + 0.5)) + " FPS";
    ui::text(r, 1024 - ui::textWidth(f, 2) - 16, 18, f, ui::theme::textDim(), 2);

    int cx = 1024 / 2, cy = 576 / 2, a = 10, g = 4;
    ui::setColor(r, ui::rgb(255, 255, 255, 220));
    SDL_RenderDrawLine(r, cx - a, cy, cx - g, cy);
    SDL_RenderDrawLine(r, cx + g, cy, cx + a, cy);
    SDL_RenderDrawLine(r, cx, cy - a, cx, cy - g);
    SDL_RenderDrawLine(r, cx, cy + g, cx, cy + a);
}

// Draws the menu and returns the hovered/clicked level index under the cursor,
// or -1. `keyboardIndex` is highlighted too (for up/down navigation).
int drawMenu(SDL_Renderer* r, const std::vector<LevelEntry>& levels,
             int keyboardIndex, int mx, int my) {
    ui::fillRect(r, 0, 0, kWindowW, kWindowH, ui::theme::panelDark());

    // Title.
    ui::text(r, kWindowW / 2 - ui::textWidth("RAY-TRACED FPS", 4) / 2, 60,
             "RAY-TRACED FPS", ui::theme::accent(), 4);
    ui::text(r, kWindowW / 2 - ui::textWidth("SELECT A LEVEL", 2) / 2, 110,
             "SELECT A LEVEL", ui::theme::textDim(), 2);

    int clicked = -1;
    int listX = kWindowW / 2 - 220;
    int listY = 160;
    int rowW = 440, rowH = 40, gap = 10;

    if (levels.empty()) {
        ui::text(r, kWindowW / 2 - ui::textWidth("NO MAPS FOUND", 2) / 2, 240,
                 "NO MAPS FOUND", ui::theme::warn(), 2);
        ui::text(r, kWindowW / 2 - ui::textWidth("BUILD ONE IN LEVEL_EDITOR", 1) / 2, 270,
                 "BUILD ONE IN LEVEL_EDITOR", ui::theme::textDim(), 1);
    }

    for (size_t i = 0; i < levels.size(); ++i) {
        int y = listY + int(i) * (rowH + gap);
        ui::Rect b{listX, y, rowW, rowH};
        bool hover = b.contains(mx, my);
        bool active = (int(i) == keyboardIndex) || hover;
        ui::panel(r, b.x, b.y, b.w, b.h, active ? ui::theme::accent() : ui::theme::panel());
        ui::Color tc = active ? ui::rgb(15, 18, 24) : ui::theme::text();
        ui::text(r, b.x + 16, b.y + (rowH - ui::lineHeight(2)) / 2 + 1,
                 levels[i].name, tc, 2);
        std::string play = "PLAY >";
        ui::text(r, b.x + rowW - ui::textWidth(play, 2) - 14,
                 b.y + (rowH - ui::lineHeight(2)) / 2 + 1, play,
                 active ? ui::rgb(15,18,24) : ui::theme::textDim(), 2);
        if (hover) clicked = int(i);
    }

    ui::text(r, kWindowW / 2 - ui::textWidth("UP/DOWN + ENTER   OR CLICK   -   ESC QUITS", 1) / 2,
             kWindowH - 36, "UP/DOWN + ENTER   OR CLICK   -   ESC QUITS",
             ui::theme::textDim(), 1);
    return clicked;
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string root = PROJECT_ROOT;
    const std::string assetsDir = root + "/assets";
    const std::string mapsDir   = root + "/maps";

    AssetLibrary assets;
    assets.baseDir = assetsDir;
    assets.loadMaterials(assetsDir + "/materials.def");
    assets.loadPrefabs(assetsDir + "/prefabs.def");

    SDLViewport viewport("Ray-Traced FPS", kWindowW, kWindowH, kRenderW, kRenderH);
    if (!viewport.valid()) {
        std::cerr << "Failed to create SDL viewport: " << SDL_GetError() << "\n";
        return 1;
    }
    std::vector<uint32_t> pixels(size_t(kRenderW) * kRenderH);
    SDL_Renderer* gpu = viewport.renderer();

    Renderer tracer(kRenderW, kRenderH);
    auto fpsCam = std::make_shared<PerspectiveCamera>(
        Vec3(0, kEyeHeight, 0), Vec3(0, kEyeHeight, -1), Vec3(0, 1, 0), 75.0);
    tracer.camera = fpsCam;
    tracer.shader = std::make_shared<PhongShader>();
    tracer.maxDepth = 3;
    tracer.samples  = 1;

    std::vector<LevelEntry> levels = scanLevels(mapsDir);

    Map   map;
    World world;
    Player player(Vec3(0, kEyeHeight, 6), 0, 0);
    int score = 0, remaining = 0;

    auto loadAndStart = [&](const std::string& path) {
        if (!map.load(path)) std::cerr << "[game] failed to load " << path << "\n";
        world = instantiate(map, assets, tracer);
        remaining = world.enemyCount;
        player = Player(map.spawn, map.spawnYaw, 0.0);
        tracer.camera = fpsCam;
    };

    // Start in the menu, unless a level was passed on the command line.
    State state = State::Menu;
    int   hoverIndex = 0;
    if (argc > 1) { loadAndStart(argv[1]); state = State::Play;
                    SDL_SetRelativeMouseMode(SDL_TRUE); }

    int  mouseX = kWindowW / 2, mouseY = kWindowH / 2;
    bool menuClick = false;
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = double(SDL_GetPerformanceFrequency());

    auto gotoMenu = [&]() {
        state = State::Menu;
        SDL_SetRelativeMouseMode(SDL_FALSE);
        levels = scanLevels(mapsDir);   // pick up anything saved in the editor
    };
    auto startLevel = [&](int idx) {
        if (idx < 0 || idx >= int(levels.size())) return;
        loadAndStart(levels[idx].path);
        state = State::Play;
        score = 0;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    };

    bool quit = false;
    while (!quit) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = double(now - prev) / freq;
        prev = now;
        if (dt > 0.1) dt = 0.1;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { quit = true; break; }

            if (state == State::Menu) {
                switch (e.type) {
                    case SDL_MOUSEMOTION:
                        mouseX = e.motion.x; mouseY = e.motion.y; break;
                    case SDL_MOUSEBUTTONDOWN:
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            // The clicked index is computed during draw; store
                            // the click and let the draw pass resolve it.
                            menuClick = true;
                        }
                        break;
                    case SDL_KEYDOWN:
                        switch (e.key.keysym.sym) {
                            case SDLK_ESCAPE: quit = true; break;
                            case SDLK_UP:
                                if (!levels.empty())
                                    hoverIndex = (hoverIndex - 1 + int(levels.size())) % int(levels.size());
                                break;
                            case SDLK_DOWN:
                                if (!levels.empty())
                                    hoverIndex = (hoverIndex + 1) % int(levels.size());
                                break;
                            case SDLK_RETURN: case SDLK_KP_ENTER:
                                startLevel(hoverIndex); break;
                            case SDLK_r:
                                levels = scanLevels(mapsDir); break;
                            default: break;
                        }
                        break;
                    default: break;
                }
            } else { // Play
                switch (e.type) {
                    case SDL_MOUSEMOTION:
                        player.addLook(e.motion.xrel, e.motion.yrel); break;
                    case SDL_MOUSEBUTTONDOWN:
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
                    case SDL_KEYDOWN:
                        switch (e.key.keysym.sym) {
                            case SDLK_ESCAPE: case SDLK_m: gotoMenu(); break;
                            case SDLK_r:
                                world = instantiate(map, assets, tracer);
                                remaining = world.enemyCount;
                                player = Player(map.spawn, map.spawnYaw, 0.0);
                                score = 0;
                                break;
                            default: break;
                        }
                        break;
                    default: break;
                }
            }
        }

        // ----- update + render --------------------------------------------
        if (state == State::Menu) {
            // Render a dim ray-traced backdrop if a level is loaded; else flat.
            ui::fillRect(gpu, 0, 0, kWindowW, kWindowH, ui::theme::panelDark());
            // (We draw the menu directly with the GPU renderer; no 3D needed.)
            SDL_RenderClear(gpu);
            int clicked = drawMenu(gpu, levels, hoverIndex, mouseX, mouseY);
            if (menuClick) { if (clicked >= 0) startLevel(clicked); menuClick = false; }
            SDL_RenderPresent(gpu);
        } else {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
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

            tracer.render(pixels.data());
            viewport.beginFrame(pixels.data());
            drawPlayHUD(gpu, score, remaining, world.enemyCount, dt > 0 ? 1.0 / dt : 0.0);
            viewport.endFrame();
        }
    }

    SDL_SetRelativeMouseMode(SDL_FALSE);
    return 0;
}
