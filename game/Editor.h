#ifndef GAME_EDITOR_H
#define GAME_EDITOR_H

// ---------------------------------------------------------------------------
// Editor: the data-model controller behind the dev view (SDL-free)
// ---------------------------------------------------------------------------
// This class holds the editing *state* (active tool, brush, current selection)
// and all the operations that mutate a Map: placing brushes, picking objects
// under a ray, transforming the selection, duplicating and deleting. It knows
// nothing about SDL or drawing — editor_main.cpp owns the window, panels and
// input, and calls into here.
//
// Tools (Unreal-ish):
//   Select  - click an object to select it; transform it with the gizmo keys.
//   Place   - click the floor to drop the current brush (category + item).
//   Delete  - click an object to remove it.
//
// Whenever an operation changes the map, `dirty` is set so the host can rebuild
// the renderer scene (via World::instantiate) and refresh the viewport.
// ---------------------------------------------------------------------------

#include <raytracer/Ray.h>
#include <raytracer/Vec3.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "Assets.h"
#include "Map.h"
#include "World.h"   // sel::Kind

class Editor {
public:
    enum class Tool { Select, Place, Delete };
    enum class Category { Wall, Box, Sphere, Prop, Enemy, Weapon, PointLight, Count };

    Tool     tool        = Tool::Select;
    Category category     = Category::Wall;
    int      itemIndex    = 0;
    bool     snap         = true;
    double   placeHeight  = 0.0;

    int  selKind  = sel::None;
    int  selIndex = -1;

    bool dirty = false;   // map changed; host rebuilds the world

    explicit Editor(const AssetLibrary& assets) : assets_(&assets) {
        rebuildPalettes();
    }

    // ----- Brush / tool selection ------------------------------------------

    void setTool(Tool t) { tool = t; }

    void nextCategory(int dir) {
        int n = int(Category::Count);
        category = Category(((int(category) + dir) % n + n) % n);
        itemIndex = 0;
    }
    void nextItem(int dir) {
        const auto& names = palette();
        if (names.empty()) return;
        int n = int(names.size());
        itemIndex = ((itemIndex + dir) % n + n) % n;
    }
    void toggleSnap() { snap = !snap; }

    std::string toolName() const {
        switch (tool) {
            case Tool::Select: return "SELECT";
            case Tool::Place:  return "PLACE";
            case Tool::Delete: return "DELETE";
        }
        return "?";
    }
    std::string categoryName() const {
        switch (category) {
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
        const auto& names = palette();
        if (names.empty()) return "(none)";
        return names[std::min<size_t>(itemIndex, names.size() - 1)];
    }
    const std::vector<std::string>& palette() const {
        switch (category) {
            case Category::Wall:
            case Category::Box:
            case Category::Sphere: return materialNames_;
            case Category::Prop:   return propNames_;
            case Category::Enemy:  return enemyNames_;
            case Category::Weapon: return weaponNames_;
            default:               return emptyNames_;
        }
    }

    // ----- Placing ----------------------------------------------------------

    void place(const Ray& aim, Map& map) {
        Vec3 p;
        if (!floorHit(aim, p)) return;
        if (snap) { p.x = std::round(p.x); p.z = std::round(p.z); }
        const std::string item = itemName();

        switch (category) {
            case Category::Wall: {
                double h = map.ceilingEnabled ? map.ceilingHeight : 3.0;
                map.walls.push_back({Vec3(p.x - 0.5, 0.0, p.z - 0.5),
                                     Vec3(p.x + 0.5, h,   p.z + 0.5), item});
                select(sel::Wall, int(map.walls.size()) - 1);
                break;
            }
            case Category::Box:
                map.boxes.push_back({Vec3(p.x, placeHeight + 0.5, p.z), 1.0, item});
                select(sel::Box, int(map.boxes.size()) - 1);
                break;
            case Category::Sphere:
                map.spheres.push_back({Vec3(p.x, placeHeight + 0.5, p.z), 0.5, item});
                select(sel::Sphere, int(map.spheres.size()) - 1);
                break;
            case Category::Prop:
                map.props.push_back({item, Vec3(p.x, placeHeight + 0.5, p.z)});
                select(sel::Prop, int(map.props.size()) - 1);
                break;
            case Category::Enemy:
                map.enemies.push_back({item, Vec3(p.x, placeHeight + 1.0, p.z)});
                select(sel::Enemy, int(map.enemies.size()) - 1);
                break;
            case Category::Weapon:
                map.weapons.push_back({item, Vec3(p.x, placeHeight + 0.5, p.z)});
                select(sel::Weapon, int(map.weapons.size()) - 1);
                break;
            case Category::PointLight:
                map.pointLights.push_back({Vec3(p.x, std::max(0.5, placeHeight + 2.0), p.z),
                                           Color(1.0, 0.95, 0.85), 1.8, 0.02});
                select(sel::Light, int(map.pointLights.size()) - 1);
                break;
            default: break;
        }
        dirty = true;
    }

    // ----- Selection / picking ---------------------------------------------

    void select(int kind, int index) { selKind = kind; selIndex = index; }
    void clearSelection() { selKind = sel::None; selIndex = -1; }
    bool hasSelection() const { return selKind != sel::None && selIndex >= 0; }

    // Pick the nearest object under `aim` and select it (Select tool).
    void selectAt(const Ray& aim, const Map& map) {
        int kind, idx;
        if (pickEntry(aim, map, kind, idx)) select(kind, idx);
        else clearSelection();
    }

    // Delete the nearest object under `aim` (Delete tool).
    void deleteAt(const Ray& aim, Map& map) {
        int kind, idx;
        if (pickEntry(aim, map, kind, idx)) eraseEntry(map, kind, idx);
    }

    void deleteSelected(Map& map) {
        if (hasSelection()) eraseEntry(map, selKind, selIndex);
    }

    void duplicateSelected(Map& map) {
        if (!hasSelection()) return;
        const Vec3 off(1.0, 0.0, 1.0);
        switch (selKind) {
            case sel::Wall: {
                auto e = map.walls[selIndex]; e.min += off; e.max += off;
                map.walls.push_back(e); select(sel::Wall, int(map.walls.size()) - 1); break; }
            case sel::Box: {
                auto e = map.boxes[selIndex]; e.center += off;
                map.boxes.push_back(e); select(sel::Box, int(map.boxes.size()) - 1); break; }
            case sel::Sphere: {
                auto e = map.spheres[selIndex]; e.center += off;
                map.spheres.push_back(e); select(sel::Sphere, int(map.spheres.size()) - 1); break; }
            case sel::Prop: {
                auto e = map.props[selIndex]; e.pos += off;
                map.props.push_back(e); select(sel::Prop, int(map.props.size()) - 1); break; }
            case sel::Enemy: {
                auto e = map.enemies[selIndex]; e.pos += off;
                map.enemies.push_back(e); select(sel::Enemy, int(map.enemies.size()) - 1); break; }
            case sel::Weapon: {
                auto e = map.weapons[selIndex]; e.pos += off;
                map.weapons.push_back(e); select(sel::Weapon, int(map.weapons.size()) - 1); break; }
            case sel::Light: {
                auto e = map.pointLights[selIndex]; e.pos += off;
                map.pointLights.push_back(e); select(sel::Light, int(map.pointLights.size()) - 1); break; }
            default: return;
        }
        dirty = true;
    }

    // ----- Transforms on the selection -------------------------------------

    void moveSelected(const Vec3& d, Map& map) {
        if (!hasSelection()) return;
        switch (selKind) {
            case sel::Wall:   map.walls[selIndex].min += d; map.walls[selIndex].max += d; break;
            case sel::Box:    map.boxes[selIndex].center += d; break;
            case sel::Sphere: map.spheres[selIndex].center += d; break;
            case sel::Prop:   map.props[selIndex].pos += d; break;
            case sel::Enemy:  map.enemies[selIndex].pos += d; break;
            case sel::Weapon: map.weapons[selIndex].pos += d; break;
            case sel::Light:  map.pointLights[selIndex].pos += d; break;
            default: return;
        }
        dirty = true;
    }

    // Multiply the selection's size (box.size / sphere.radius / wall footprint).
    void scaleSelected(double factor, Map& map) {
        if (!hasSelection()) return;
        switch (selKind) {
            case sel::Box:    map.boxes[selIndex].size = clampSize(map.boxes[selIndex].size * factor); break;
            case sel::Sphere: map.spheres[selIndex].radius = clampSize(map.spheres[selIndex].radius * factor); break;
            case sel::Wall: {
                auto& w = map.walls[selIndex];
                Vec3 c = 0.5 * (w.min + w.max);
                Vec3 half = 0.5 * (w.max - w.min);
                double fx = clampSize(half.x * factor) , fz = clampSize(half.z * factor);
                w.min = Vec3(c.x - fx, w.min.y, c.z - fz);
                w.max = Vec3(c.x + fx, w.max.y, c.z + fz);
                break;
            }
            default: return;   // prop/enemy/weapon size comes from the prefab
        }
        dirty = true;
    }

    // ----- Queries for the UI ----------------------------------------------

    bool isSelected(int kind, int index) const {
        return selKind == kind && selIndex == index;
    }

    // World center + a bounding radius of the selection (for the viewport
    // marker). Returns false if nothing is selected.
    bool selectionBounds(const Map& map, Vec3& center, double& radius) const {
        if (!hasSelection()) return false;
        return entryBounds(map, selKind, selIndex, center, radius);
    }

    std::string selectionLabel(const Map& map) const {
        if (!hasSelection()) return "(nothing selected)";
        switch (selKind) {
            case sel::Wall:   return "wall  " + map.walls[selIndex].material;
            case sel::Box:    return "box  " + map.boxes[selIndex].material;
            case sel::Sphere: return "sphere  " + map.spheres[selIndex].material;
            case sel::Prop:   return "prop  " + map.props[selIndex].type;
            case sel::Enemy:  return "enemy  " + map.enemies[selIndex].type;
            case sel::Weapon: return "weapon  " + map.weapons[selIndex].type;
            case sel::Light:  return "point light";
            default:          return "?";
        }
    }
    Vec3 selectionPosition(const Map& map) const {
        Vec3 c; double r;
        if (entryBounds(map, selKind, selIndex, c, r)) return c;
        return Vec3();
    }

    // Flattened list of every placed entry for the Outliner panel.
    struct OutlinerRow { int kind; int index; std::string label; };
    std::vector<OutlinerRow> outliner(const Map& map) const {
        std::vector<OutlinerRow> rows;
        for (size_t i = 0; i < map.walls.size(); ++i)
            rows.push_back({sel::Wall, int(i), "wall " + map.walls[i].material});
        for (size_t i = 0; i < map.boxes.size(); ++i)
            rows.push_back({sel::Box, int(i), "box " + map.boxes[i].material});
        for (size_t i = 0; i < map.spheres.size(); ++i)
            rows.push_back({sel::Sphere, int(i), "sphere " + map.spheres[i].material});
        for (size_t i = 0; i < map.props.size(); ++i)
            rows.push_back({sel::Prop, int(i), "prop " + map.props[i].type});
        for (size_t i = 0; i < map.enemies.size(); ++i)
            rows.push_back({sel::Enemy, int(i), "enemy " + map.enemies[i].type});
        for (size_t i = 0; i < map.weapons.size(); ++i)
            rows.push_back({sel::Weapon, int(i), "weapon " + map.weapons[i].type});
        for (size_t i = 0; i < map.pointLights.size(); ++i)
            rows.push_back({sel::Light, int(i), "light"});
        return rows;
    }

    void rebuildPalettes() {
        materialNames_ = assets_->materialNames();
        propNames_     = assets_->propNames();
        enemyNames_    = assets_->enemyNames();
        weaponNames_   = assets_->weaponNames();
        if (materialNames_.empty()) materialNames_.push_back("wall");
    }

private:
    static double clampSize(double v) { return std::max(0.1, std::min(20.0, v)); }

    static bool floorHit(const Ray& aim, Vec3& out) {
        if (std::fabs(aim.dir.y) < 1e-6) return false;
        double t = -aim.orig.y / aim.dir.y;
        if (t <= 0.0) return false;
        out = aim.at(t);
        if (std::fabs(out.x) > 64.0 || std::fabs(out.z) > 64.0) return false;
        return true;
    }

    // World center + bounding radius for any entry kind/index.
    bool entryBounds(const Map& map, int kind, int index,
                     Vec3& center, double& radius) const {
        switch (kind) {
            case sel::Wall: {
                const auto& w = map.walls[index];
                center = 0.5 * (w.min + w.max);
                Vec3 h = 0.5 * (w.max - w.min);
                radius = std::max(h.x, std::max(h.y, h.z));
                return true;
            }
            case sel::Box:
                center = map.boxes[index].center;
                radius = map.boxes[index].size * 0.75; return true;
            case sel::Sphere:
                center = map.spheres[index].center;
                radius = map.spheres[index].radius * 1.1; return true;
            case sel::Prop: {
                center = map.props[index].pos;
                double sz = 1.0;
                auto it = assets_->props.find(map.props[index].type);
                if (it != assets_->props.end()) sz = it->second.size;
                radius = sz * 0.75; return true;
            }
            case sel::Enemy: {
                center = map.enemies[index].pos;
                double r = 0.45;
                auto it = assets_->enemies.find(map.enemies[index].type);
                if (it != assets_->enemies.end()) r = it->second.radius;
                radius = r * 1.3; return true;
            }
            case sel::Weapon: {
                center = map.weapons[index].pos;
                double sz = 0.4;
                auto it = assets_->weapons.find(map.weapons[index].type);
                if (it != assets_->weapons.end()) sz = it->second.size;
                radius = sz; return true;
            }
            case sel::Light:
                center = map.pointLights[index].pos; radius = 0.5; return true;
            default: return false;
        }
    }

    // Closest entry hit by `aim`, tested as a bounding sphere per entry.
    bool pickEntry(const Ray& aim, const Map& map, int& outKind, int& outIndex) const {
        double bestT = std::numeric_limits<double>::infinity();
        bool found = false;
        auto tryKind = [&](int kind, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                Vec3 c; double r;
                if (!entryBounds(map, kind, int(i), c, r)) continue;
                double t;
                if (raySphere(aim, c, std::max(r, 0.25), t) && t < bestT) {
                    bestT = t; outKind = kind; outIndex = int(i); found = true;
                }
            }
        };
        tryKind(sel::Wall,   map.walls.size());
        tryKind(sel::Box,    map.boxes.size());
        tryKind(sel::Sphere, map.spheres.size());
        tryKind(sel::Prop,   map.props.size());
        tryKind(sel::Enemy,  map.enemies.size());
        tryKind(sel::Weapon, map.weapons.size());
        tryKind(sel::Light,  map.pointLights.size());
        return found;
    }

    static bool raySphere(const Ray& r, const Vec3& center, double radius, double& tHit) {
        Vec3 oc = r.orig - center;
        double a = dot(r.dir, r.dir);
        double b = 2.0 * dot(oc, r.dir);
        double c = dot(oc, oc) - radius * radius;
        double disc = b * b - 4 * a * c;
        if (disc < 0) return false;
        double t = (-b - std::sqrt(disc)) / (2 * a);
        if (t < 0.001) t = (-b + std::sqrt(disc)) / (2 * a);
        if (t < 0.001) return false;
        tHit = t;
        return true;
    }

    void eraseEntry(Map& map, int kind, int index) {
        switch (kind) {
            case sel::Wall:   map.walls.erase(map.walls.begin() + index); break;
            case sel::Box:    map.boxes.erase(map.boxes.begin() + index); break;
            case sel::Sphere: map.spheres.erase(map.spheres.begin() + index); break;
            case sel::Prop:   map.props.erase(map.props.begin() + index); break;
            case sel::Enemy:  map.enemies.erase(map.enemies.begin() + index); break;
            case sel::Weapon: map.weapons.erase(map.weapons.begin() + index); break;
            case sel::Light:  map.pointLights.erase(map.pointLights.begin() + index); break;
            default: return;
        }
        clearSelection();
        dirty = true;
    }

    const AssetLibrary* assets_;
    std::vector<std::string> materialNames_, propNames_, enemyNames_, weaponNames_, emptyNames_;
};

#endif // GAME_EDITOR_H
