#ifndef GAME_EDITOR_H
#define GAME_EDITOR_H

// ---------------------------------------------------------------------------
// In-game map editor (the "dev tools")
// ---------------------------------------------------------------------------
// The editor mutates a Map in place. The host loop is responsible for telling
// it when the map changed (so it can re-instantiate the World/Scene) via the
// `dirty` flag this class sets.
//
// Workflow once in edit mode (toggled by the host, conventionally F1):
//   * fly with WASD + Q/E (down/up), mouse looks around
//   * a "brush" decides what gets placed; cycle the category with [ and ],
//     and the specific asset within a category with , and .
//   * left click  -> place the current brush where the crosshair meets the
//                    floor (snapped to a grid when snapping is on)
//   * right click -> delete the nearest placed entity to the crosshair
//   * G toggles grid snapping, [ / ] / , / . pick the brush
//   * F5 saves the map, F9 reloads it from disk
//
// The brush palette is built from the AssetLibrary, so any material / prefab a
// designer adds to the .def files is immediately placeable here.
// ---------------------------------------------------------------------------

#include <raytracer/Ray.h>
#include <raytracer/Vec3.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Assets.h"
#include "Map.h"

class Editor {
public:
    enum class Category { Wall, Box, Sphere, Prop, Enemy, Weapon, PointLight, Count };

    bool dirty = false;       // set when the map changed; host clears it

    explicit Editor(const AssetLibrary& assets) : assets_(&assets) {
        rebuildPalettes();
    }

    // --- Brush selection ----------------------------------------------------

    void nextCategory(int dir) {
        int n = static_cast<int>(Category::Count);
        category_ = static_cast<Category>(((static_cast<int>(category_) + dir) % n + n) % n);
        itemIndex_ = 0;
    }

    void nextItem(int dir) {
        const auto& names = currentPalette();
        if (names.empty()) return;
        int n = static_cast<int>(names.size());
        itemIndex_ = ((itemIndex_ + dir) % n + n) % n;
    }

    void toggleSnap() { snap_ = !snap_; }
    bool snapping() const { return snap_; }

    // Height the next placed entity sits at (raise/lower with the wheel/keys).
    void adjustHeight(double d) {
        placeHeight_ += d;
        if (placeHeight_ < 0.0) placeHeight_ = 0.0;
    }

    Category category() const { return category_; }
    std::string categoryName() const {
        switch (category_) {
            case Category::Wall:       return "wall";
            case Category::Box:        return "box";
            case Category::Sphere:     return "sphere";
            case Category::Prop:       return "prop";
            case Category::Enemy:      return "enemy";
            case Category::Weapon:     return "weapon";
            case Category::PointLight: return "light";
            default:                   return "?";
        }
    }
    std::string itemName() const {
        const auto& names = currentPalette();
        if (names.empty()) return "(none)";
        return names[std::min<size_t>(itemIndex_, names.size() - 1)];
    }

    // --- Editing actions ----------------------------------------------------

    // Place the current brush where `aim` meets the floor plane (y = 0).
    void place(const Ray& aim, Map& map) {
        Vec3 p;
        if (!floorHit(aim, p)) return;
        if (snap_) { p.x = std::round(p.x); p.z = std::round(p.z); }

        const std::string item = itemName();
        switch (category_) {
            case Category::Wall: {
                // A unit-footprint wall column spanning floor..ceiling.
                double h = map.ceilingEnabled ? map.ceilingHeight : 3.0;
                Vec3 lo(p.x - 0.5, 0.0, p.z - 0.5);
                Vec3 hi(p.x + 0.5, h,   p.z + 0.5);
                map.walls.push_back({lo, hi, item});
                break;
            }
            case Category::Box:
                map.boxes.push_back({Vec3(p.x, placeHeight_ + 0.5, p.z), 1.0, item});
                break;
            case Category::Sphere:
                map.spheres.push_back({Vec3(p.x, placeHeight_ + 0.5, p.z), 0.5, item});
                break;
            case Category::Prop:
                map.props.push_back({item, Vec3(p.x, placeHeight_ + 0.5, p.z)});
                break;
            case Category::Enemy:
                map.enemies.push_back({item, Vec3(p.x, placeHeight_ + 1.0, p.z)});
                break;
            case Category::Weapon:
                map.weapons.push_back({item, Vec3(p.x, placeHeight_ + 0.5, p.z)});
                break;
            case Category::PointLight:
                map.pointLights.push_back({
                    Vec3(p.x, std::max(0.5, placeHeight_ + 2.0), p.z),
                    Color(1.0, 0.95, 0.85), 1.8, 0.02});
                break;
            default: break;
        }
        dirty = true;
    }

    // Delete the placed entity (of any kind) nearest to where the crosshair
    // meets the floor.
    void deleteNearest(const Ray& aim, Map& map) {
        Vec3 p;
        if (!floorHit(aim, p)) return;

        double best = 4.0;   // only delete within 2 units (squared = 4)
        int kind = -1;       // which list
        size_t idx = 0;

        auto consider = [&](int k, size_t i, const Vec3& pos) {
            double dx = pos.x - p.x, dz = pos.z - p.z;
            double d2 = dx * dx + dz * dz;
            if (d2 < best) { best = d2; kind = k; idx = i; }
        };

        for (size_t i = 0; i < map.walls.size(); ++i)
            consider(0, i, 0.5 * (map.walls[i].min + map.walls[i].max));
        for (size_t i = 0; i < map.boxes.size(); ++i)   consider(1, i, map.boxes[i].center);
        for (size_t i = 0; i < map.spheres.size(); ++i) consider(2, i, map.spheres[i].center);
        for (size_t i = 0; i < map.props.size(); ++i)   consider(3, i, map.props[i].pos);
        for (size_t i = 0; i < map.enemies.size(); ++i) consider(4, i, map.enemies[i].pos);
        for (size_t i = 0; i < map.weapons.size(); ++i) consider(5, i, map.weapons[i].pos);
        for (size_t i = 0; i < map.pointLights.size(); ++i) consider(6, i, map.pointLights[i].pos);

        switch (kind) {
            case 0: map.walls.erase(map.walls.begin() + idx); break;
            case 1: map.boxes.erase(map.boxes.begin() + idx); break;
            case 2: map.spheres.erase(map.spheres.begin() + idx); break;
            case 3: map.props.erase(map.props.begin() + idx); break;
            case 4: map.enemies.erase(map.enemies.begin() + idx); break;
            case 5: map.weapons.erase(map.weapons.begin() + idx); break;
            case 6: map.pointLights.erase(map.pointLights.begin() + idx); break;
            default: return;   // nothing close enough
        }
        dirty = true;
    }

    // Refresh palettes after assets are (re)loaded.
    void rebuildPalettes() {
        materialNames_ = assets_->materialNames();
        propNames_     = assets_->propNames();
        enemyNames_    = assets_->enemyNames();
        weaponNames_   = assets_->weaponNames();
        if (materialNames_.empty()) materialNames_.push_back("wall");
    }

private:
    const std::vector<std::string>& currentPalette() const {
        switch (category_) {
            case Category::Wall:
            case Category::Box:
            case Category::Sphere: return materialNames_;
            case Category::Prop:   return propNames_;
            case Category::Enemy:  return enemyNames_;
            case Category::Weapon: return weaponNames_;
            default:               return emptyNames_;
        }
    }

    // Intersect the aim ray with the floor plane y = 0.
    static bool floorHit(const Ray& aim, Vec3& out) {
        if (std::fabs(aim.dir.y) < 1e-6) return false;
        double t = -aim.orig.y / aim.dir.y;
        if (t <= 0.0) return false;
        out = aim.at(t);
        // Keep placement within a sane radius of the origin.
        if (std::fabs(out.x) > 64.0 || std::fabs(out.z) > 64.0) return false;
        return true;
    }

    const AssetLibrary* assets_;
    Category category_ = Category::Wall;
    int      itemIndex_ = 0;
    bool     snap_ = true;
    double   placeHeight_ = 0.0;

    std::vector<std::string> materialNames_;
    std::vector<std::string> propNames_;
    std::vector<std::string> enemyNames_;
    std::vector<std::string> weaponNames_;
    std::vector<std::string> emptyNames_;
};

#endif // GAME_EDITOR_H
