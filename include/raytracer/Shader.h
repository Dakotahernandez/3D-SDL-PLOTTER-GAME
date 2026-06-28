#ifndef SHADER_H
#define SHADER_H

#include "Vec3.h"
#include "Ray.h"
#include "Hittable.h"
#include "Scene.h"
#include "RayCaster.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Abstract shading model. Assign a different Shader to the Renderer to
// hot-swap the entire lighting/look of the scene at runtime.
class Shader {
public:
    virtual ~Shader() {}

    // Compute the color for a surface hit. `caster` lets the shader spawn
    // secondary rays (reflections, etc.) without knowing the renderer type.
    virtual Color shade(const Ray& r, const HitRecord& rec,
                        const Scene& scene, int depth,
                        const RayCaster& caster) const = 0;
};

// Classic Blinn-Phong: ambient + diffuse + specular, with hard shadows and
// optional recursive mirror reflections. Loops over every light in the scene.
class PhongShader : public Shader {
public:
    Color shade(const Ray& r, const HitRecord& rec,
                const Scene& scene, int depth,
                const RayCaster& caster) const override {
        const Material& m = rec.material;
        const double kInf = std::numeric_limits<double>::infinity();

        // Base color may come from a texture (tinted by albedo) or be flat.
        Color base = m.baseColor(rec.u, rec.v, rec.point);

        Color result = scene.ambient * base;
        Vec3 viewDir = unit_vector(-r.dir);

        for (const auto& light : scene.lights) {
            LightSample ls = light->sample(rec.point);

            // Shadow ray toward the light.
            Ray shadowRay(rec.point + rec.normal * 1e-4, ls.direction);
            double maxT = std::isinf(ls.distance) ? kInf : ls.distance - 1e-3;
            if (scene.occluded(shadowRay, 0.001, maxT)) continue;

            // Diffuse (Lambert).
            double diff = std::max(0.0, dot(rec.normal, ls.direction));
            Color diffuse = m.diffuse * diff * (base * ls.radiance);

            // Specular (Blinn-Phong half vector).
            Vec3 half = unit_vector(ls.direction + viewDir);
            double spec = std::pow(std::max(0.0, dot(rec.normal, half)),
                                   m.shininess);
            Color specular = m.specular * spec * ls.radiance;

            result = result + diffuse + specular;
        }

        // Recursive mirror reflection.
        if (m.reflectivity > 0.0 && depth > 1) {
            Vec3 reflDir = reflect(unit_vector(r.dir), rec.normal);
            Ray reflRay(rec.point + rec.normal * 1e-4, reflDir);
            Color reflected = caster.trace(reflRay, depth - 1);
            result = (1.0 - m.reflectivity) * result
                   + m.reflectivity * reflected;
        }

        return result;
    }
};

// Debug shader: visualizes surface normals as RGB. Great for checking geometry
// and demonstrating runtime shader hot-swapping.
class NormalShader : public Shader {
public:
    Color shade(const Ray&, const HitRecord& rec,
                const Scene&, int, const RayCaster&) const override {
        return 0.5 * (rec.normal + Vec3(1, 1, 1));
    }
};

// Flat/unlit shader: just the material base color. Cheap and texture-aware.
class FlatShader : public Shader {
public:
    Color shade(const Ray&, const HitRecord& rec,
                const Scene&, int, const RayCaster&) const override {
        return rec.material.baseColor(rec.u, rec.v, rec.point);
    }
};

#endif // SHADER_H
