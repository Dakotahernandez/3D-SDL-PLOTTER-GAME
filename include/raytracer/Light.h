#ifndef LIGHT_H
#define LIGHT_H

#include "Vec3.h"
#include <limits>
#include <cmath>

// What a shader needs to know about a light at a given surface point.
struct LightSample {
    Vec3   direction;   // unit vector from the surface point TOWARD the light
    double distance;    // distance to the light (infinity for directional)
    Color  radiance;    // incoming light color/intensity at the point
};

// Abstract light. Subclass this to add new light types; assign instances into
// Scene::lights to hot-swap the lighting setup at runtime.
class Light {
public:
    virtual ~Light() {}

    // Evaluate the light as seen from `point`.
    virtual LightSample sample(const Vec3& point) const = 0;
};

// An omnidirectional point light with quadratic distance attenuation.
class PointLight : public Light {
public:
    Vec3   position;
    Color  color;
    double intensity;
    double attenuation;   // quadratic falloff coefficient

    PointLight(const Vec3& pos = Vec3(0, 3, 0),
               const Color& col = Color(1, 1, 1),
               double inten = 1.0,
               double atten = 0.02)
        : position(pos), color(col), intensity(inten), attenuation(atten) {}

    LightSample sample(const Vec3& point) const override {
        Vec3 toLight = position - point;
        double dist = toLight.length();
        double falloff = 1.0 / (1.0 + attenuation * dist * dist);
        LightSample s;
        s.direction = (dist > 1e-9) ? toLight / dist : Vec3(0, 1, 0);
        s.distance = dist;
        s.radiance = color * (intensity * falloff);
        return s;
    }
};

// A light infinitely far away (sun-like): constant direction, no attenuation.
class DirectionalLight : public Light {
public:
    Vec3  direction;   // direction the light travels (e.g. pointing down)
    Color color;
    double intensity;

    DirectionalLight(const Vec3& dir = Vec3(0, -1, 0),
                     const Color& col = Color(1, 1, 1),
                     double inten = 1.0)
        : direction(unit_vector(dir)), color(col), intensity(inten) {}

    LightSample sample(const Vec3& /*point*/) const override {
        LightSample s;
        s.direction = -direction;   // toward the light
        s.distance = std::numeric_limits<double>::infinity();
        s.radiance = color * intensity;
        return s;
    }
};

#endif // LIGHT_H
