#ifndef BOX_H
#define BOX_H

#include "Hittable.h"
#include <cmath>

// An axis-aligned box defined by its two opposite corners (min/max).
// Uses the slab method for intersection and derives the face normal.
class Box : public Hittable {
public:
    Vec3     bmin;
    Vec3     bmax;
    Material material;

    Box(const Vec3& a, const Vec3& b, const Material& m)
        : bmin(std::fmin(a.x, b.x), std::fmin(a.y, b.y), std::fmin(a.z, b.z)),
          bmax(std::fmax(a.x, b.x), std::fmax(a.y, b.y), std::fmax(a.z, b.z)),
          material(m) {}

    // Build a cube centered at `center` with the given side length.
    static Box cube(const Vec3& center, double side, const Material& m) {
        Vec3 h(side * 0.5, side * 0.5, side * 0.5);
        return Box(center - h, center + h, m);
    }

    bool hit(const Ray& r, double t_min, double t_max,
             HitRecord& rec) const override {
        double tmin = t_min;
        double tmax = t_max;
        int hitAxis = -1;
        double sign = 1.0;

        const double o[3] = { r.orig.x, r.orig.y, r.orig.z };
        const double d[3] = { r.dir.x,  r.dir.y,  r.dir.z  };
        const double lo[3] = { bmin.x, bmin.y, bmin.z };
        const double hi[3] = { bmax.x, bmax.y, bmax.z };

        for (int a = 0; a < 3; ++a) {
            double invD = 1.0 / d[a];
            double t0 = (lo[a] - o[a]) * invD;
            double t1 = (hi[a] - o[a]) * invD;
            double s = -1.0;                  // normal points toward -axis on near face
            if (invD < 0.0) { std::swap(t0, t1); s = 1.0; }
            if (t0 > tmin) { tmin = t0; hitAxis = a; sign = s; }
            if (t1 < tmax) { tmax = t1; }
            if (tmax <= tmin) return false;
        }

        if (hitAxis < 0) return false;

        rec.t = tmin;
        rec.point = r.at(tmin);
        Vec3 n(0, 0, 0);
        if (hitAxis == 0) n = Vec3(sign, 0, 0);
        else if (hitAxis == 1) n = Vec3(0, sign, 0);
        else n = Vec3(0, 0, sign);
        rec.normal = n;

        // Per-face texture coordinates (world units along the two in-plane
        // axes) so wall/box textures tile consistently across faces.
        if (hitAxis == 0)      { rec.u = rec.point.z; rec.v = rec.point.y; }
        else if (hitAxis == 1) { rec.u = rec.point.x; rec.v = rec.point.z; }
        else                   { rec.u = rec.point.x; rec.v = rec.point.y; }

        rec.material = material;
        return true;
    }
};

#endif // BOX_H
