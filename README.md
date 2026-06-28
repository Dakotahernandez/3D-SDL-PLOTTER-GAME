# 3D-SDL-PLOTTER-GAME

A 3D SDL game project, powered by an embeddable, object-oriented **ray tracing
library**. The ray tracer is written so its core has **no dependency on SDL** —
you build a scene, pick a camera and a shading model, render into a plain
32-bit ARGB pixel buffer, and present it however the game likes (SDL, OpenGL,
Metal, an off-screen image, ...). An optional `SDLViewport` adapter is included
for quick SDL integration.

## Layout

```
include/raytracer/   Public headers (the library API)
  Vec3.h             Vector / color math
  Ray.h              Ray type
  Material.h         Surface material + ready-made presets
  Hittable.h         Abstract intersectable + HitRecord
  Sphere.h Plane.h Box.h   Concrete shapes (hot-swappable geometry)
  Light.h            Light interface + PointLight / DirectionalLight
  Camera.h           Camera interface + Perspective / Orthographic
  Shader.h           Shader interface + Phong / Normal / Flat models
  RayCaster.h        Interface shaders use to spawn secondary rays
  Scene.h            Geometry + lights container
  RayTracer.h        The Renderer (owns Camera + Shader strategies)
  SDLViewport.h      Optional SDL window/texture adapter
src/                 RayTracer.cpp, Sphere.cpp
examples/            demo_main.cpp (interactive SDL demo)
```

## OOP / hot-swapping design

Every major behavior is a polymorphic strategy you can replace at runtime:

| Concept   | Base class  | Built-in implementations                     |
|-----------|-------------|----------------------------------------------|
| Geometry  | `Hittable`  | `Sphere`, `Plane`, `Box`                     |
| Lights    | `Light`     | `PointLight`, `DirectionalLight`             |
| Camera    | `Camera`    | `PerspectiveCamera`, `OrthographicCamera`    |
| Shading   | `Shader`    | `PhongShader`, `NormalShader`, `FlatShader`  |

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
```

The core `raytracer` static library builds even without SDL2. To use it from
the game's own CMake target:

```cmake
add_subdirectory(path/to/this/repo)
target_link_libraries(my_game PRIVATE raytracer::raytracer)
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