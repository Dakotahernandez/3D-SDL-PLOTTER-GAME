#ifndef MATERIAL_H
#define MATERIAL_H

#include "Vec3.h"
#include "Texture.h"

#include <memory>

// Surface description used by the Phong-style shader.
//   albedo       base (diffuse) color, also used to TINT a texture
//   diffuse      diffuse coefficient (kd)
//   specular     specular coefficient (ks)
//   shininess    Phong specular exponent (larger = tighter highlight)
//   reflectivity 0 = matte, 1 = perfect mirror
//   texture      optional surface texture; when set it replaces the flat
//                albedo (modulated by `albedo` so albedo doubles as a tint)
struct Material {
    Color  albedo;
    double diffuse;
    double specular;
    double shininess;
    double reflectivity;
    std::shared_ptr<Texture> texture;   // optional; null = flat albedo

    Material()
        : albedo(0.8, 0.8, 0.8), diffuse(0.9), specular(0.3),
          shininess(32.0), reflectivity(0.0) {}

    Material(const Color& a, double kd, double ks, double shin, double refl)
        : albedo(a), diffuse(kd), specular(ks),
          shininess(shin), reflectivity(refl) {}

    // Effective base color at a surface point: texture (tinted) or flat albedo.
    Color baseColor(double u, double v, const Vec3& p) const {
        if (texture) return albedo * texture->sample(u, v, p);
        return albedo;
    }
};

// A few ready-made materials for convenience.
namespace Materials {
    inline Material matte(const Color& c) {
        return Material(c, 0.9, 0.1, 8.0, 0.0);
    }
    inline Material glossy(const Color& c) {
        return Material(c, 0.7, 0.6, 64.0, 0.15);
    }
    inline Material mirror(const Color& c) {
        return Material(c, 0.2, 0.8, 256.0, 0.8);
    }
}

#endif // MATERIAL_H
