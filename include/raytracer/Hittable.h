#ifndef HITTABLE_H
#define HITTABLE_H

#include "Vec3.h"
#include "Ray.h"
#include "Material.h"

// Records everything the shader needs about a ray/surface intersection.
struct HitRecord {
    Vec3     point;     // world-space hit position
    Vec3     normal;    // unit surface normal at the hit (faces the ray)
    double   t = 0.0;   // ray parameter at the hit
    Material material;   // surface material at the hit point
};

// Abstract base class for anything a ray can intersect.
class Hittable {
public:
    virtual ~Hittable() {}

    // Returns true and fills `rec` if the ray hits within (t_min, t_max).
    virtual bool hit(const Ray& r, double t_min, double t_max,
                     HitRecord& rec) const = 0;
};

#endif // HITTABLE_H
