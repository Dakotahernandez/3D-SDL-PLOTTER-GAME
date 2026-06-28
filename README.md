# 3D-SDL-PLOTTER-GAME

A 3D SDL game project, powered by an embeddable, object-oriented **ray tracing
library**. The ray tracer is written so its core has **no dependency on SDL** —
you build a scene, pick a camera and a shading model, render into a plain
32-bit ARGB pixel buffer, and present it however the game likes (SDL, OpenGL,
Metal, an off-screen image, ...). An optional `SDLViewport` adapter is included
for quick SDL integration.
based off of my work in this repo 
https://github.com/Dakotahernandez/c-_raytracing-rendering_using_SDL_PLOTTER
## Layout

```
include/raytracer/   Public headers (the library API)
  Vec3.h             Vector / color math
  Ray.h              Ray type
  Material.h         Surface material + ready-made presets
  Texture.h          Texture interface + Solid/Checker/Gradient/Image (PPM)
  Hittable.h         Abstract intersectable + HitRecord (with UVs)
  Sphere.h Plane.h Box.h   Concrete shapes (hot-swappable geometry)
  Light.h            Light interface + PointLight / DirectionalLight
  Camera.h           Camera interface + Perspective / Orthographic
  Shader.h           Shader interface + Phong / Normal / Flat models
  RayCaster.h        Interface shaders use to spawn secondary rays
  Scene.h            Geometry + lights container
  RayTracer.h        The Renderer (owns Camera + Shader strategies)
  SDLViewport.h      Optional SDL window/texture adapter
src/                 RayTracer.cpp, Sphere.cpp, ImageIO.cpp
examples/            demo_main.cpp (interactive SDL demo)
game/                Player.h Assets.h Map.h World.h Editor.h main.cpp
assets/              materials.def, prefabs.def, textures/*
maps/                arena.map (default level)
third_party/         stb_image.h (vendored PNG/JPG/... decoder)
```

## OOP / hot-swapping design

Every major behavior is a polymorphic strategy you can replace at runtime:

| Concept   | Base class  | Built-in implementations                     |
|-----------|-------------|----------------------------------------------|
| Geometry  | `Hittable`  | `Sphere`, `Plane`, `Box`                     |
| Lights    | `Light`     | `PointLight`, `DirectionalLight`             |
| Camera    | `Camera`    | `PerspectiveCamera`, `OrthographicCamera`    |
| Shading   | `Shader`    | `PhongShader`, `NormalShader`, `FlatShader`  |
| Texture   | `Texture`   | `SolidColor`, `CheckerTexture`, `GradientTexture`, `ImageTexture` |

The `Renderer` (`RayTracer`) holds `std::shared_ptr<Camera>` and
`std::shared_ptr<Shader>`, so swapping the look or the projection is a single
assignment — no rebuild, no branching in the render loop.

```cpp
#include <raytracer/RayTracer.h>
#include <raytracer/Sphere.h>
#include <raytracer/Light.h>
#include <raytracer/Camera.h>
#include <raytracer/Shader.h>

Renderer tracer(width, height);
tracer.camera = std::make_shared<PerspectiveCamera>();
tracer.shader = std::make_shared<PhongShader>();

tracer.scene.add(std::make_shared<Sphere>(
    Vec3(0, 0, -5), 1.0, Materials::glossy(Color(0.9, 0.25, 0.25))));
tracer.scene.addLight(std::make_shared<PointLight>(Vec3(0, 3, -3)));

std::vector<uint32_t> pixels(size_t(width) * height);
tracer.render(pixels.data());          // fills an ARGB8888 buffer

// Hot-swap behavior at runtime:
tracer.shader = std::make_shared<NormalShader>();   // different look
tracer.camera = std::make_shared<OrthographicCamera>();
```

## Building

Requires CMake 3.15+, a C++17 compiler, and (for the demo) SDL2.

```sh
cmake -S . -B build
cmake --build build
./build/raytracer_demo      # if SDL2 was found
./build/fps_game            # the ray-traced FPS game
```

The core `raytracer` static library builds even without SDL2. To use it from
the game's own CMake target:

```cmake
add_subdirectory(path/to/this/repo)
target_link_libraries(my_game PRIVATE raytracer::raytracer)
```

## The FPS game

`game/main.cpp` is a small first-person shooter rendered **entirely** with the
ray tracer — every pixel you see is a traced ray, including the mirror pillars'
reflections. It demonstrates how to embed the library in a real-time loop:

1. read input and update the `Player` (mouse look + WASD, with wall collision),
2. push the player's pose onto the renderer's `PerspectiveCamera`,
3. ray trace the scene into a low-res ARGB buffer, and
4. upscale to the window and draw the crosshair + HUD.

For interactivity the scene is traced at a reduced internal resolution
(`480x270`) and the GPU upscales it to the `960x540` window — that split is
handled by the optional `renderWidth/renderHeight` arguments now accepted by
`SDLViewport`.

Walk around the arena and shoot the enemies; clear them all to win. The score,
enemies remaining, and current FPS show in the title bar. Everything in the
level is loaded from data files (see **Dev tools** below) — nothing about the
arena is hard-coded.

### Game controls

| Input          | Action                                   |
|----------------|------------------------------------------|
| `W` `A` `S` `D`| Move (collides with walls and pillars)   |
| Mouse          | Look around                              |
| Left click     | Shoot the enemy under the crosshair      |
| `R`            | Reset the level and score                |
| `F1`           | Toggle the **map editor**                |
| `Esc`          | Quit                                     |

Run a specific level with `./build/fps_game path/to/level.map` (defaults to
`maps/arena.map`).

## Dev tools: textures, designs & the map editor

The game is fully **data-driven** so you can build levels and drop in new
content without recompiling. Three kinds of plain-text files drive everything:

```
assets/materials.def   named surface looks (color / checker / gradient / image)
assets/prefabs.def      enemy / weapon / prop "designs"
assets/textures/*       image textures (PNG / JPG / BMP / PPM ...)
maps/*.map              levels that reference the names above
```

### Textures

Materials can be a flat color, a procedural checkerboard or gradient, or an
**image texture**. Image textures load common formats directly — **PNG**, JPG,
BMP, GIF, TGA, PSD (via the vendored public-domain
[stb_image](https://github.com/nothings/stb)) as well as binary PPM (P6). Just
drop a file into `assets/textures/` and point a material at it; no conversion
needed:

```
convert logo.svg assets/textures/logo.png   # or just copy any .png in
```

UVs are generated for spheres (spherical), planes (world X/Z) and boxes
(per-face), so walls, floors, crates and props all texture correctly. Example
`materials.def` entries:

```
material metal image    textures/metal.png 1.0 glossy
material brick image    textures/brick.ppm 1.0 matte
material floor checker   0.82 0.82 0.86  0.16 0.17 0.22  1.0 matte
material accent gradient 0.10 0.30 0.55  0.85 0.35 0.15  0.0 3.0 glossy
material chrome color    0.80 0.85 0.95  mirror
```

### Dropping in characters / enemies / weapons

Define a design once in `prefabs.def` and reference it by name from any map or
the editor — no code changes:

```
enemy  brute  material enemy2 radius 0.70 speed 0.8 hp 3
weapon shotgun material weapon size 0.45 damage 3 label Shotgun
prop   crate  shape box       material crate size 1.0
```

### The in-game map editor

Press `F1` to enter **EDIT** mode. You free-fly through the level and place
content with a "brush" built automatically from your asset library, so anything
you add to the `.def` files is immediately placeable. Hit `F5` to write the map
back to disk in the same human-readable format you can also hand-edit.

| Input            | Action                                            |
|------------------|---------------------------------------------------|
| `W A S D` `Q`/`E`| Fly (Q/E = down/up), mouse looks                  |
| `[` / `]`        | Cycle brush **category** (wall/box/sphere/prop/enemy/weapon/light) |
| `,` / `.` or wheel| Cycle the **item** within the category           |
| Left click       | Place the current brush where the crosshair hits the floor |
| Right click      | Delete the nearest placed entity                  |
| `G`              | Toggle grid snapping                              |
| `Shift`+wheel    | Raise / lower the placement height                |
| `F5` / `F9`      | Save / reload the map                             |
| `F1`             | Back to PLAY                                       |

The map format (`maps/*.map`) is documented at the top of
[game/Map.h](game/Map.h); the asset grammars are documented inside
[assets/materials.def](assets/materials.def) and
[assets/prefabs.def](assets/prefabs.def). Source layout:

```
game/Player.h    FPS controller + collision
game/Assets.h    material/texture + prefab loaders
game/Map.h       level data + load/save
game/World.h     instantiate a Map into renderer geometry + game state
game/Editor.h    the in-game map editor
game/main.cpp    glue: play loop + edit loop
```


## Demo controls

| Key            | Action                                   |
|----------------|------------------------------------------|
| Mouse move     | Move the light (when in FOLLOW mode)     |
| `Space`        | Hold / release the light in place        |
| `1` / `2` / `3`| Phong / Normals / Flat shader            |
| `C`            | Toggle perspective / orthographic camera |
| `S`            | Cycle the hero sphere's material         |
| `A`            | Add a random sphere or box               |
| `X`            | Clear added shapes                       |
| `+` / `-`      | Increase / decrease anti-aliasing        |
| `Esc`          | Quit                                     |
