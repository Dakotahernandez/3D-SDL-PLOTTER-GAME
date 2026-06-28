#ifndef GAME_WORLD_H
#define GAME_WORLD_H

// ---------------------------------------------------------------------------
// World: turns a (data-only) Map into live renderer geometry + game state
// ---------------------------------------------------------------------------
// instantiate() is the bridge between the editor's data model (Map) and the
// running game: it rebuilds the renderer's Scene from scratch and returns the
// runtime state the gameplay loop needs (collision walls, shootable enemies,
// spawn pose). Call it whenever the map changes so edits show up immediately.
// ---------------------------------------------------------------------------

#include <raytracer/RayTracer.h>
#include <raytracer/Sphere.h>
#include <raytracer/Plane.h>
#include <raytracer/Box.h>
#include <raytracer/Light.h>

#include <memory>
#include <string>
#include <vector>

#include "Assets.h"
#include "Map.h"
#include "Player.h"   // for WallAABB

// A live, shootable enemy instance backed by a sphere in the scene.
struct LiveEnemy {
    std::shared_ptr<Sphere> sphere;
    std::string type;
    double      speed = 1.5;
    int         hp    = 1;
    bool        alive = true;
};

// Runtime state produced from a Map.
struct World {
    std::vector<WallAABB>  walls;     // XZ collision footprints
    std::vector<LiveEnemy> enemies;   // moving / shootable targets
    Vec3   spawn    = Vec3(0, 1.2, 6);
    double spawnYaw = 0.0;
    int    enemyCount = 0;            // total enemies at spawn (for HUD)
};

inline World instantiate(const Map& map, const AssetLibrary& assets,
                         Renderer& tracer) {
    World world;
    world.spawn    = map.spawn;
    world.spawnYaw = map.spawnYaw;

    tracer.scene.clear();
    tracer.scene.clearLights();
    tracer.scene.ambient = map.ambient;

    auto pushAABB = [&](const Vec3& a, const Vec3& b) {
        world.walls.push_back(WallAABB{
            std::fmin(a.x, b.x), std::fmax(a.x, b.x),
            std::fmin(a.z, b.z), std::fmax(a.z, b.z)});
    };

    // Floor / ceiling.
    if (map.floorEnabled) {
        tracer.scene.add(std::make_shared<Plane>(
            Vec3(0, 0, 0), Vec3(0, 1, 0), assets.material(map.floorMaterial)));
    }
    if (map.ceilingEnabled) {
        tracer.scene.add(std::make_shared<Plane>(
            Vec3(0, map.ceilingHeight, 0), Vec3(0, -1, 0),
            assets.material(map.ceilingMaterial)));
    }

    // Walls (boxes by min/max) — solid, so they collide.
    for (const auto& w : map.walls) {
        tracer.scene.add(std::make_shared<Box>(w.min, w.max,
                                               assets.material(w.material)));
        pushAABB(w.min, w.max);
    }

    // Free-standing cubes — solid.
    for (const auto& b : map.boxes) {
        Vec3 h(b.size * 0.5, b.size * 0.5, b.size * 0.5);
        Vec3 lo = b.center - h, hi = b.center + h;
        tracer.scene.add(std::make_shared<Box>(lo, hi, assets.material(b.material)));
        pushAABB(lo, hi);
    }

    // Decorative spheres — no collision.
    for (const auto& s : map.spheres) {
        tracer.scene.add(std::make_shared<Sphere>(
            s.center, s.radius, assets.material(s.material)));
    }

    // Props from prefab designs.
    for (const auto& p : map.props) {
        PropDef def;
        auto it = assets.props.find(p.type);
        if (it != assets.props.end()) def = it->second;
        Material mat = assets.material(def.material);
        if (def.shape == "sphere") {
            tracer.scene.add(std::make_shared<Sphere>(p.pos, def.size * 0.5, mat));
        } else {
            Vec3 h(def.size * 0.5, def.size * 0.5, def.size * 0.5);
            Vec3 lo = p.pos - h, hi = p.pos + h;
            tracer.scene.add(std::make_shared<Box>(lo, hi, mat));
            pushAABB(lo, hi);
        }
    }

    // Weapons render as a small floating box (a placeholder pickup model).
    for (const auto& w : map.weapons) {
        WeaponDef def;
        auto it = assets.weapons.find(w.type);
        if (it != assets.weapons.end()) def = it->second;
        Vec3 h(def.size * 0.5, def.size * 0.5, def.size * 0.5);
        tracer.scene.add(std::make_shared<Box>(
            w.pos - h, w.pos + h, assets.material(def.material)));
    }

    // Enemies from prefab designs — shootable spheres.
    for (const auto& e : map.enemies) {
        EnemyDef def;
        auto it = assets.enemies.find(e.type);
        if (it != assets.enemies.end()) def = it->second;
        auto sphere = std::make_shared<Sphere>(
            e.pos, def.radius, assets.material(def.material));
        tracer.scene.add(sphere);
        world.enemies.push_back(LiveEnemy{sphere, e.type, def.speed, def.hp, true});
    }
    world.enemyCount = static_cast<int>(world.enemies.size());

    // Lights.
    for (const auto& l : map.pointLights) {
        tracer.scene.addLight(std::make_shared<PointLight>(
            l.pos, l.color, l.intensity, l.atten));
    }
    for (const auto& l : map.dirLights) {
        tracer.scene.addLight(std::make_shared<DirectionalLight>(
            l.dir, l.color, l.intensity));
    }

    return world;
}

#endif // GAME_WORLD_H
