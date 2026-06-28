#ifndef GAME_MAP_H
#define GAME_MAP_H

// ---------------------------------------------------------------------------
// Map: a hand-editable, savable description of a level
// ---------------------------------------------------------------------------
// A Map is pure data (no SDL, no renderer). It is loaded from a `.map` text
// file, edited in memory by the in-game editor, written back out with save(),
// and turned into live renderer geometry by instantiate().
//
// .map grammar (one statement per line, '#' starts a comment):
//   spawn   <x> <y> <z> <yawDegrees>
//   ambient <value>
//   floor   <material> | none
//   ceiling <height> <material> | none
//   light point <x> <y> <z> <r> <g> <b> <intensity> <atten>
//   light dir   <dx> <dy> <dz> <r> <g> <b> <intensity>
//   wall    <minx> <miny> <minz> <maxx> <maxy> <maxz> <material>
//   box     <cx> <cy> <cz> <size> <material>
//   sphere  <cx> <cy> <cz> <radius> <material>
//   enemy   <type> <x> <y> <z>
//   weapon  <type> <x> <y> <z>
//   prop    <type> <x> <y> <z>
// ---------------------------------------------------------------------------

#include <raytracer/Vec3.h>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct WallEntry   { Vec3 min, max; std::string material; };
struct BoxEntry    { Vec3 center; double size; std::string material; };
struct SphereEntry { Vec3 center; double radius; std::string material; };
struct EntityEntry { std::string type; Vec3 pos; };   // enemy / weapon / prop
struct PointLightEntry { Vec3 pos; Color color; double intensity, atten; };
struct DirLightEntry   { Vec3 dir; Color color; double intensity; };

struct Map {
    // --- Globals ---
    Vec3   spawn    = Vec3(0.0, 1.2, 6.0);
    double spawnYaw = 0.0;                 // radians
    double ambient  = 0.12;

    bool        floorEnabled   = true;
    std::string floorMaterial  = "floor";
    bool        ceilingEnabled = true;
    double      ceilingHeight  = 3.0;
    std::string ceilingMaterial = "ceiling";

    // --- Placed content ---
    std::vector<WallEntry>       walls;
    std::vector<BoxEntry>        boxes;
    std::vector<SphereEntry>     spheres;
    std::vector<EntityEntry>     enemies;
    std::vector<EntityEntry>     weapons;
    std::vector<EntityEntry>     props;
    std::vector<PointLightEntry> pointLights;
    std::vector<DirLightEntry>   dirLights;

    // --- IO -----------------------------------------------------------------

    bool load(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[map] cannot open " << path << "\n";
            return false;
        }
        *this = Map();   // reset to defaults, then overlay file contents
        // Defaults assume a closed room; a fresh file may redefine everything.
        walls.clear(); boxes.clear(); spheres.clear();
        enemies.clear(); weapons.clear(); props.clear();
        pointLights.clear(); dirLights.clear();

        std::string line;
        while (std::getline(in, line)) {
            auto t = tok(line);
            if (t.empty()) continue;
            const std::string& cmd = t[0];

            if (cmd == "spawn" && t.size() >= 5) {
                spawn = vec(t, 1);
                spawnYaw = num(t[4]) * 3.14159265358979323846 / 180.0;
            } else if (cmd == "ambient" && t.size() >= 2) {
                ambient = num(t[1]);
            } else if (cmd == "floor" && t.size() >= 2) {
                if (t[1] == "none") { floorEnabled = false; }
                else { floorEnabled = true; floorMaterial = t[1]; }
            } else if (cmd == "ceiling") {
                if (t.size() >= 2 && t[1] == "none") {
                    ceilingEnabled = false;
                } else if (t.size() >= 3) {
                    ceilingEnabled = true;
                    ceilingHeight = num(t[1]);
                    ceilingMaterial = t[2];
                }
            } else if (cmd == "light" && t.size() >= 2 && t[1] == "point" && t.size() >= 11) {
                pointLights.push_back({vec(t, 2), color(t, 5), num(t[8]), num(t[9])});
            } else if (cmd == "light" && t.size() >= 2 && t[1] == "dir" && t.size() >= 10) {
                dirLights.push_back({vec(t, 2), color(t, 5), num(t[8])});
            } else if (cmd == "wall" && t.size() >= 8) {
                walls.push_back({vec(t, 1), vec(t, 4), t[7]});
            } else if (cmd == "box" && t.size() >= 6) {
                boxes.push_back({vec(t, 1), num(t[4]), t[5]});
            } else if (cmd == "sphere" && t.size() >= 6) {
                spheres.push_back({vec(t, 1), num(t[4]), t[5]});
            } else if (cmd == "enemy" && t.size() >= 5) {
                enemies.push_back({t[1], vec(t, 2)});
            } else if (cmd == "weapon" && t.size() >= 5) {
                weapons.push_back({t[1], vec(t, 2)});
            } else if (cmd == "prop" && t.size() >= 5) {
                props.push_back({t[1], vec(t, 2)});
            }
        }
        return true;
    }

    bool save(const std::string& path) const {
        std::ofstream out(path);
        if (!out) {
            std::cerr << "[map] cannot write " << path << "\n";
            return false;
        }
        out << std::fixed << std::setprecision(3);
        out << "# Saved by the in-game editor. Edit by hand or in-game.\n\n";

        out << "spawn " << v3(spawn) << " "
            << (spawnYaw * 180.0 / 3.14159265358979323846) << "\n";
        out << "ambient " << ambient << "\n";
        if (floorEnabled)   out << "floor " << floorMaterial << "\n";
        else                out << "floor none\n";
        if (ceilingEnabled) out << "ceiling " << ceilingHeight << " " << ceilingMaterial << "\n";
        else                out << "ceiling none\n";
        out << "\n";

        for (const auto& l : pointLights)
            out << "light point " << v3(l.pos) << " " << v3(l.color) << " "
                << l.intensity << " " << l.atten << "\n";
        for (const auto& l : dirLights)
            out << "light dir " << v3(l.dir) << " " << v3(l.color) << " "
                << l.intensity << "\n";
        if (!pointLights.empty() || !dirLights.empty()) out << "\n";

        for (const auto& w : walls)
            out << "wall " << v3(w.min) << " " << v3(w.max) << " " << w.material << "\n";
        for (const auto& b : boxes)
            out << "box " << v3(b.center) << " " << b.size << " " << b.material << "\n";
        for (const auto& s : spheres)
            out << "sphere " << v3(s.center) << " " << s.radius << " " << s.material << "\n";
        if (!walls.empty() || !boxes.empty() || !spheres.empty()) out << "\n";

        for (const auto& e : enemies)  out << "enemy "  << e.type << " " << v3(e.pos) << "\n";
        for (const auto& e : weapons)  out << "weapon " << e.type << " " << v3(e.pos) << "\n";
        for (const auto& e : props)    out << "prop "   << e.type << " " << v3(e.pos) << "\n";

        std::cout << "[map] saved " << path << "\n";
        return true;
    }

private:
    static std::vector<std::string> tok(const std::string& line) {
        std::vector<std::string> out;
        std::istringstream ss(line);
        std::string w;
        while (ss >> w) {
            if (!w.empty() && w[0] == '#') break;
            out.push_back(w);
        }
        return out;
    }
    static double num(const std::string& s) { return std::strtod(s.c_str(), nullptr); }
    static Vec3 vec(const std::vector<std::string>& t, size_t i) {
        return Vec3(num(t[i]), num(t[i + 1]), num(t[i + 2]));
    }
    static Color color(const std::vector<std::string>& t, size_t i) {
        return Color(num(t[i]), num(t[i + 1]), num(t[i + 2]));
    }
    static std::string v3(const Vec3& v) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3)
           << v.x << " " << v.y << " " << v.z;
        return ss.str();
    }
};

#endif // GAME_MAP_H
