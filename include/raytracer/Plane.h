#ifndef PLANE_H
#define PLANE_H

#include "Hittable.h"
#include <cmath>

// An infinite plane defined by a point and a (unit) normal.
// Optionally renders a checkerboard by alternating between two materials.
class Plane : public Hittable {
public:
    Vec3     point;        // any point on the plane
    Vec3     normal;       // unit normal
    Material material;     // primary material
    bool     checker;      // enable checkerboard pattern
    Material material2;    // secondary material for checker tiles
    double   tileSize;     // size of each checker tile

    Plane(const Vec3& p, const Vec3& n, const Material& m)
        : point(p), normal(unit_vector(n)), material(m),
          checker(false), material2(m), tileSize(1.0) {}

    Plane(const Vec3& p, const Vec3& n,
          const Material& m1, const Material& m2, double tile)
        : point(p), normal(unit_vector(n)), material(m1),
          checker(true), material2(m2), tileSize(tile) {}

    bool hit(const Ray& r, double t_min, double t_max,
             HitRecord& rec) const override {
        double denom = dot(normal, r.dir);
        if (std::fabs(denom) < 1e-8) return false;     // ray parallel to plane

        double t = dot(point - r.orig, normal) / denom;
        if (t < t_min || t > t_max) return false;

        rec.t = t;
        rec.point = r.at(t);
        // Always return a normal facing back toward the incoming ray.
        rec.normal = (denom < 0.0) ? normal : -normal;

        rec.material = material;
        if (checker) {
            int ix = static_cast<int>(std::floor(rec.point.x / tileSize));
            int iz = static_cast<int>(std::floor(rec.point.z / tileSize));
            if (((ix + iz) & 1) == 0) rec.material = material2;
        }
        return true;
    }
};

#endif // PLANE_H
