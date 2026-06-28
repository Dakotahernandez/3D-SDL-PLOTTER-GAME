#include <SDL2/SDL.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include <raytracer/RayTracer.h>
#include <raytracer/SDLViewport.h>
#include <raytracer/Sphere.h>
#include <raytracer/Plane.h>
#include <raytracer/Box.h>
#include <raytracer/Light.h>
#include <raytracer/Camera.h>
#include <raytracer/Shader.h>
#include <raytracer/Material.h>

// ---------------------------------------------------------------------------
// This file is just a DEMO / driver. The ray tracer itself (everything in
// RayTracer.* plus the *.h strategy headers) is an SDL-free library you can
// drop into a 3D SDL game: build a Scene, pick a Camera + Shader, call
// render() into any 32-bit ARGB buffer, and blit it however you like.
// ---------------------------------------------------------------------------

static double frand(double lo, double hi) {
    return lo + (hi - lo) * (double(std::rand()) / double(RAND_MAX));
}

// Build the demo scene. Returns the "hero" sphere and the movable light so the
// driver can mutate them interactively.
static void buildScene(Renderer& tracer,
                       std::shared_ptr<Sphere>& heroOut,
                       std::shared_ptr<PointLight>& lightOut) {
    tracer.scene.clear();
    tracer.scene.clearLights();

    // Checkerboard floor.
    Material lightTile = Materials::matte(Color(0.9, 0.9, 0.9));
    Material darkTile  = Materials::matte(Color(0.25, 0.25, 0.28));
    tracer.scene.add(std::make_shared<Plane>(
        Vec3(0, -1, 0), Vec3(0, 1, 0), lightTile, darkTile, 1.0));

    // Hero sphere (its material is hot-swapped with the S key).
    auto hero = std::make_shared<Sphere>(
        Vec3(0, 0, -5), 1.0, Materials::glossy(Color(0.9, 0.25, 0.25)));
    tracer.scene.add(hero);
    heroOut = hero;

    // A mirror sphere and a matte sphere flanking it.
    tracer.scene.add(std::make_shared<Sphere>(
        Vec3(-2.2, -0.3, -4.5), 0.7, Materials::mirror(Color(0.9, 0.9, 1.0))));
    tracer.scene.add(std::make_shared<Sphere>(
        Vec3(2.1, -0.4, -4.2), 0.6, Materials::matte(Color(0.2, 0.6, 0.9))));

    // A cube for some flat surfaces.
    tracer.scene.add(std::make_shared<Box>(
        Box::cube(Vec3(0.9, -0.55, -3.2), 0.9,
                  Materials::glossy(Color(0.35, 0.8, 0.4)))));

    // Movable point light.
    auto light = std::make_shared<PointLight>(
        Vec3(0.0, 3.0, -3.0), Color(1, 1, 1), 1.4, 0.015);
    tracer.scene.addLight(light);
    lightOut = light;

    // A dim fill light so shadows are not pitch black.
    tracer.scene.addLight(std::make_shared<DirectionalLight>(
        Vec3(-0.5, -1.0, -0.3), Color(0.4, 0.45, 0.55), 0.4));
}

// Map the mouse onto a horizontal plane the light glides over.
static Vec3 mapMouseToLight(int mx, int my, int w, int h) {
    double worldX = (double(mx) / w) * 8.0 - 4.0;     // x in [-4, 4]
    double worldZ = -1.0 - (double(my) / h) * 7.0;    // z in [-1, -8]
    return Vec3(worldX, 3.0, worldZ);
}

int main(int /*argc*/, char* /*argv*/[]) {
    const int width = 800;
    const int height = 600;

    SDLViewport viewport("Interactive Ray Tracer", width, height);
    if (!viewport.valid()) {
        std::cerr << "Failed to create SDL viewport: " << SDL_GetError() << "\n";
        return 1;
    }

    std::vector<uint32_t> pixels(size_t(width) * height);

    // ----- Build the library renderer -------------------------------------
    Renderer tracer(width, height);
    tracer.camera = std::make_shared<PerspectiveCamera>(
        Vec3(0, 1, 1), Vec3(0, 0, -5), Vec3(0, 1, 0), 60.0);
    tracer.shader = std::make_shared<PhongShader>();
    tracer.maxDepth = 5;
    tracer.samples = 1;

    std::shared_ptr<Sphere> hero;
    std::shared_ptr<PointLight> light;
    buildScene(tracer, hero, light);

    // Pre-made strategies we can hot-swap between at runtime.
    std::shared_ptr<Shader> phong  = tracer.shader;
    std::shared_ptr<Shader> normals = std::make_shared<NormalShader>();
    std::shared_ptr<Shader> flat    = std::make_shared<FlatShader>();

    std::shared_ptr<Camera> perspCam = tracer.camera;
    std::shared_ptr<Camera> orthoCam = std::make_shared<OrthographicCamera>(
        Vec3(0, 1, 1), Vec3(0, 0, -5), Vec3(0, 1, 0), 6.0);

    std::vector<Material> heroMaterials = {
        Materials::glossy(Color(0.9, 0.25, 0.25)),
        Materials::matte (Color(0.95, 0.8, 0.2)),
        Materials::mirror(Color(0.95, 0.95, 0.98)),
    };
    int heroMatIndex = 0;

    size_t baseObjectCount = tracer.scene.objects.size();

    bool lightFollowsMouse = true;
    int mouseX = width / 2;
    int mouseY = height / 2;

    auto updateTitle = [&]() {
        std::string s = "Ray Tracer | ";
        s += lightFollowsMouse ? "light: FOLLOW" : "light: HELD";
        s += " | samples: " + std::to_string(tracer.samples);
        s += " | [1]Phong [2]Normals [3]Flat  [S]material [C]camera"
             "  [A]add [X]clear  [Space]hold light";
        viewport.setTitle(s.c_str());
    };
    updateTitle();

    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEMOTION) {
                mouseX = e.motion.x;
                mouseY = e.motion.y;
                if (lightFollowsMouse) {
                    light->position = mapMouseToLight(mouseX, mouseY, width, height);
                }
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: quit = true; break;

                    // --- Hot-swap the shading model (the "function") --------
                    case SDLK_1: tracer.shader = phong;   updateTitle(); break;
                    case SDLK_2: tracer.shader = normals; updateTitle(); break;
                    case SDLK_3: tracer.shader = flat;    updateTitle(); break;

                    // --- Hot-swap the camera projection ---------------------
                    case SDLK_c:
                        tracer.camera = (tracer.camera == perspCam)
                                            ? orthoCam : perspCam;
                        updateTitle();
                        break;

                    // --- Change the hero sphere's material/shape ------------
                    case SDLK_s:
                        heroMatIndex = (heroMatIndex + 1) % heroMaterials.size();
                        hero->material = heroMaterials[heroMatIndex];
                        break;

                    // --- Hold / release the light ---------------------------
                    case SDLK_SPACE:
                        lightFollowsMouse = !lightFollowsMouse;
                        updateTitle();
                        break;

                    // --- Add a random shape ---------------------------------
                    case SDLK_a: {
                        Vec3 pos(frand(-3.5, 3.5), frand(-0.7, 1.5),
                                 frand(-7.0, -3.0));
                        Color col(frand(0.2, 1.0), frand(0.2, 1.0), frand(0.2, 1.0));
                        if (std::rand() & 1) {
                            tracer.scene.add(std::make_shared<Sphere>(
                                pos, frand(0.3, 0.7), Materials::glossy(col)));
                        } else {
                            tracer.scene.add(std::make_shared<Box>(
                                Box::cube(pos, frand(0.4, 0.9),
                                          Materials::matte(col))));
                        }
                        break;
                    }

                    // --- Clear the shapes we added --------------------------
                    case SDLK_x:
                        tracer.scene.objects.resize(baseObjectCount);
                        break;

                    // --- Anti-aliasing quality ------------------------------
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                        tracer.samples = std::min(4, tracer.samples + 1);
                        updateTitle();
                        break;
                    case SDLK_MINUS:
                        tracer.samples = std::max(1, tracer.samples - 1);
                        updateTitle();
                        break;
                }
            }
        }

        // Render the scene into our buffer using the current strategies.
        tracer.render(pixels.data());

        // Present, with a small overlay marker showing the light's screen pos.
        viewport.beginFrame(pixels.data());
        SDL_Renderer* rr = viewport.renderer();
        SDL_SetRenderDrawColor(rr, 255, 230, 60, 255);
        SDL_Rect marker{ mouseX - 3, mouseY - 3, 6, 6 };
        SDL_RenderFillRect(rr, &marker);
        viewport.endFrame();
    }

    return 0;
}

