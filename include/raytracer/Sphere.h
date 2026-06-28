#ifndef SPHERE_H
#define SPHERE_H

#include "Hittable.h"

class Sphere : public Hittable {
public:
    Vec3     center;
    double   radius;
    Material material;

    Sphere() : center(), radius(0), material() {}
    Sphere(const Vec3& c, double r)
        : center(c), radius(r), material() {}
    Sphere(const Vec3& c, double r, const Material& m)
        : center(c), radius(r), material(m) {}

    // Ray-sphere intersection. Fills `rec` (point, normal, t, material) on hit.
    bool hit(const Ray& r, double t_min, double t_max,
             HitRecord& rec) const override;
};

#endif // SPHERE_H

