#ifndef RAYCASTER_H
#define RAYCASTER_H

#include "Vec3.h"
#include "Ray.h"

// Minimal interface a shader uses to spawn secondary rays (reflection,
// refraction, ambient occlusion, etc.) without depending on the concrete
// renderer. The Renderer implements this so shaders stay hot-swappable and
// decoupled from rendering internals.
class RayCaster {
public:
    virtual ~RayCaster() {}

    // Trace a ray through the whole scene and return its radiance.
    // `depth` is the remaining bounce budget.
    virtual Color trace(const Ray& r, int depth) const = 0;
};

#endif // RAYCASTER_H
