#ifndef SCENE_H
#define SCENE_H

#include "Hittable.h"
#include "Light.h"
#include <memory>
#include <vector>
#include <limits>

// Holds all scene geometry plus a collection of (polymorphic) lights.
// Everything is stored by shared_ptr so geometry and lights can be added,
// removed, or swapped at runtime.
class Scene {
public:
    std::vector<std::shared_ptr<Hittable>> objects;
    std::vector<std::shared_ptr<Light>>    lights;

    double ambient = 0.12;   // global ambient term

    void add(const std::shared_ptr<Hittable>& obj) { objects.push_back(obj); }
    void addLight(const std::shared_ptr<Light>& l) { lights.push_back(l); }
    void clear() { objects.clear(); }
    void clearLights() { lights.clear(); }

    // Closest-hit query across every object in the scene.
    bool hit(const Ray& r, double t_min, double t_max, HitRecord& rec) const {
        HitRecord temp;
        bool hitAnything = false;
        double closest = t_max;
        for (const auto& obj : objects) {
            if (obj->hit(r, t_min, closest, temp)) {
                hitAnything = true;
                closest = temp.t;
                rec = temp;
            }
        }
        return hitAnything;
    }

    // Cheap occlusion test for shadow rays (stops at the first blocker).
    bool occluded(const Ray& r, double t_min, double t_max) const {
        HitRecord temp;
        for (const auto& obj : objects) {
            if (obj->hit(r, t_min, t_max, temp)) return true;
        }
        return false;
    }
};

#endif // SCENE_H
