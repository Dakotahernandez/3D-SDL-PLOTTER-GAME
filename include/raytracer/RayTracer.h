#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <cstdint>
#include <memory>
#include <functional>
#include "Vec3.h"
#include "Ray.h"
#include "Scene.h"
#include "Camera.h"
#include "Shader.h"
#include "RayCaster.h"

// The core renderer. It owns a Scene plus hot-swappable Camera and Shader
// strategies, and implements RayCaster so shaders can spawn secondary rays.
//
// Designed to be embedded as a library: it knows nothing about SDL or any
// windowing system and simply fills a caller-provided 32-bit ARGB buffer.
class RayTracer : public RayCaster {
public:
    RayTracer(int width, int height);

    // Render the whole scene into `pixels` (size width*height, ARGB8888).
    void render(uint32_t* pixels);

    // RayCaster: trace a single ray through the scene.
    Color trace(const Ray& r, int depth) const override;

    // ----- Hot-swappable components ---------------------------------------
    Scene                     scene;
    std::shared_ptr<Camera>   camera;   // swap to change projection / view
    std::shared_ptr<Shader>   shader;   // swap to change the lighting model

    // Background is a plain function object so it can be swapped too.
    std::function<Color(const Ray&)> background;

    // ----- Quality knobs ---------------------------------------------------
    int maxDepth = 4;   // reflection bounce limit
    int samples  = 1;   // anti-aliasing samples per axis (1 = off, 2 = 4x...)

    // ----- Convenience -----------------------------------------------------
    void setSize(int width, int height);
    int  getWidth()  const { return width_; }
    int  getHeight() const { return height_; }

    // Moves the first PointLight in the scene (handy for interactive demos).
    void setLightPosition(const Vec3& pos);

private:
    void renderBand(uint32_t* pixels, int y0, int y1) const;

    int width_;
    int height_;
};

// Friendly alias for library users.
using Renderer = RayTracer;

#endif // RAYTRACER_H

