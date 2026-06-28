#include "Sphere.h"
#include <cmath>

bool Sphere::hit(const Ray& r, double t_min, double t_max,
                 HitRecord& rec) const {
    // oc = origin - center
    Vec3 oc = r.orig - center;
    double a = dot(r.dir, r.dir);
    double b = 2.0 * dot(oc, r.dir);
    double c = dot(oc, oc) - radius * radius;
    double discriminant = b*b - 4*a*c;
    if (discriminant < 0) {
        return false;
    }

    double sqrt_disc = std::sqrt(discriminant);
    double t = (-b - sqrt_disc) / (2*a);
    if (t < t_min || t > t_max) {
        t = (-b + sqrt_disc) / (2*a);
        if (t < t_min || t > t_max) {
            return false;
        }
    }

    rec.t = t;
    rec.point = r.at(t);
    Vec3 outward = unit_vector(rec.point - center);
    // Flip the normal so it always faces against the incoming ray
    // (lets us shade the inside of a sphere correctly too).
    rec.normal = (dot(r.dir, outward) < 0.0) ? outward : -outward;

    // Spherical texture coordinates in [0,1] from the outward normal.
    double theta = std::acos(std::min(1.0, std::max(-1.0, -outward.y)));
    double phi   = std::atan2(-outward.z, outward.x) + M_PI;
    rec.u = phi / (2.0 * M_PI);
    rec.v = theta / M_PI;

    rec.material = material;
    return true;
}

