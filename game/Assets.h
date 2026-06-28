#ifndef GAME_ASSETS_H
#define GAME_ASSETS_H

// ---------------------------------------------------------------------------
// Data-driven asset library
// ---------------------------------------------------------------------------
// Everything an artist / designer needs to "drop in" new content lives in two
// plain-text definition files that are loaded at runtime (no recompile):
//
//   assets/materials.def  -> named surface looks (colors, textures, finishes)
//   assets/prefabs.def     -> named enemy / weapon / prop "designs"
//
// Maps then reference those names. Edit a .def file, relaunch (or hit reload in
// the editor) and the new content appears. See the files themselves for the
// documented grammar; a short summary is in each loader below.
// ---------------------------------------------------------------------------

#include <raytracer/Material.h>
#include <raytracer/Texture.h>
#include <raytracer/Vec3.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ----- Design records (the "drop-in" content) ------------------------------

struct EnemyDef {
    std::string material = "enemy";
    double      radius   = 0.45;
    double      speed    = 1.5;   // world units / second toward the player
    int         hp       = 1;
};

struct WeaponDef {
    std::string material = "weapon";
    double      size     = 0.3;
    int         damage   = 1;
    std::string label    = "weapon";
};

struct PropDef {
    std::string shape    = "box";   // "box" or "sphere"
    std::string material = "wall";
    double      size     = 1.0;
};

// ----- The library ----------------------------------------------------------

class AssetLibrary {
public:
    std::map<std::string, Material>  materials;
    std::map<std::string, EnemyDef>  enemies;
    std::map<std::string, WeaponDef> weapons;
    std::map<std::string, PropDef>   props;

    // Directory used to resolve relative texture paths in materials.def.
    std::string baseDir;

    // Look up a material by name, falling back to a visible default so a typo
    // in a map never crashes the game.
    Material material(const std::string& name) const {
        auto it = materials.find(name);
        if (it != materials.end()) return it->second;
        Material m = Materials::glossy(Color(1.0, 0.0, 1.0));   // magenta
        return m;
    }

    bool hasMaterial(const std::string& n) const {
        return materials.count(n) != 0;
    }

    // Ordered name lists, handy for the editor's cycling palette.
    std::vector<std::string> materialNames() const { return keys(materials); }
    std::vector<std::string> enemyNames()    const { return keys(enemies); }
    std::vector<std::string> weaponNames()   const { return keys(weapons); }
    std::vector<std::string> propNames()     const { return keys(props); }

    // --- Loading ------------------------------------------------------------

    // materials.def grammar (one statement per line, '#' starts a comment):
    //   material <name> color    <r> <g> <b> <finish>
    //   material <name> checker  <r1> <g1> <b1> <r2> <g2> <b2> <tile> <finish>
    //   material <name> gradient <r1> <g1> <b1> <r2> <g2> <b2> <y0> <y1> <finish>
    //   material <name> image    <file> <scale> <finish>
    // <finish> is one of: matte | glossy | mirror
    // Image files may be PNG, JPG, BMP, GIF, TGA or binary PPM (P6).
    bool loadMaterials(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[assets] cannot open materials: " << path << "\n";
            return false;
        }
        std::string line;
        int lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            auto tok = tokenize(line);
            if (tok.empty() || tok[0] != "material") continue;
            if (tok.size() < 3) { warn(path, lineNo, "incomplete material"); continue; }

            const std::string& name = tok[1];
            const std::string& kind = tok[2];
            Material mat;

            if (kind == "color" && tok.size() >= 7) {
                Color c = color3(tok, 3);
                mat = finish(tok[6]);
                mat.albedo = c;
            } else if (kind == "checker" && tok.size() >= 11) {
                Color c1 = color3(tok, 3);
                Color c2 = color3(tok, 6);
                double tile = num(tok[9]);
                mat = finish(tok[10]);
                mat.albedo = Color(1, 1, 1);   // texture provides the color
                mat.texture = std::make_shared<CheckerTexture>(c1, c2, tile);
            } else if (kind == "gradient" && tok.size() >= 12) {
                Color c1 = color3(tok, 3);
                Color c2 = color3(tok, 6);
                double y0 = num(tok[9]);
                double y1 = num(tok[10]);
                mat = finish(tok[11]);
                mat.albedo = Color(1, 1, 1);
                mat.texture = std::make_shared<GradientTexture>(c1, c2, y0, y1);
            } else if (kind == "image" && tok.size() >= 6) {
                std::string file  = resolve(tok[3]);
                double      scale = num(tok[4]);
                mat = finish(tok[5]);
                mat.albedo = Color(1, 1, 1);
                auto img = ImageTexture::load(file, scale);
                if (img) {
                    mat.texture = img;
                } else {
                    std::cerr << "[assets] missing/undecodable image '" << file
                              << "' for material '" << name << "'\n";
                    mat.albedo = Color(1.0, 0.0, 1.0);
                }
            } else {
                warn(path, lineNo, "unrecognized material statement");
                continue;
            }
            materials[name] = mat;
        }
        return true;
    }

    // prefabs.def grammar (keyword/value pairs after the type + name):
    //   enemy  <name> material <m> radius <r> speed <s> hp <n>
    //   weapon <name> material <m> size <s> damage <n> label <text>
    //   prop   <name> shape <box|sphere> material <m> size <s>
    // Any omitted key keeps the default from the *Def struct above.
    bool loadPrefabs(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[assets] cannot open prefabs: " << path << "\n";
            return false;
        }
        std::string line;
        while (std::getline(in, line)) {
            auto tok = tokenize(line);
            if (tok.size() < 2) continue;

            if (tok[0] == "enemy") {
                EnemyDef d;
                forEachKV(tok, 2, [&](const std::string& k, const std::string& v) {
                    if (k == "material") d.material = v;
                    else if (k == "radius") d.radius = num(v);
                    else if (k == "speed") d.speed = num(v);
                    else if (k == "hp") d.hp = int(num(v));
                });
                enemies[tok[1]] = d;
            } else if (tok[0] == "weapon") {
                WeaponDef d;
                forEachKV(tok, 2, [&](const std::string& k, const std::string& v) {
                    if (k == "material") d.material = v;
                    else if (k == "size") d.size = num(v);
                    else if (k == "damage") d.damage = int(num(v));
                    else if (k == "label") d.label = v;
                });
                weapons[tok[1]] = d;
            } else if (tok[0] == "prop") {
                PropDef d;
                forEachKV(tok, 2, [&](const std::string& k, const std::string& v) {
                    if (k == "shape") d.shape = v;
                    else if (k == "material") d.material = v;
                    else if (k == "size") d.size = num(v);
                });
                props[tok[1]] = d;
            }
        }
        return true;
    }

private:
    // Apply a named finish preset (coefficients only; albedo set by caller).
    static Material finish(const std::string& name) {
        if (name == "mirror") return Materials::mirror(Color(1, 1, 1));
        if (name == "matte")  return Materials::matte(Color(1, 1, 1));
        return Materials::glossy(Color(1, 1, 1));   // default / "glossy"
    }

    std::string resolve(const std::string& rel) const {
        if (rel.empty() || rel[0] == '/') return rel;
        if (baseDir.empty()) return rel;
        return baseDir + "/" + rel;
    }

    static double num(const std::string& s) {
        return std::strtod(s.c_str(), nullptr);
    }

    static Color color3(const std::vector<std::string>& t, size_t i) {
        return Color(num(t[i]), num(t[i + 1]), num(t[i + 2]));
    }

    static std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> out;
        std::istringstream ss(line);
        std::string w;
        while (ss >> w) {
            if (!w.empty() && w[0] == '#') break;   // rest of line is a comment
            out.push_back(w);
        }
        return out;
    }

    // Walk keyword/value pairs starting at index `start`.
    template <typename Fn>
    static void forEachKV(const std::vector<std::string>& t, size_t start, Fn fn) {
        for (size_t i = start; i + 1 < t.size(); i += 2) {
            fn(t[i], t[i + 1]);
        }
    }

    template <typename Map>
    static std::vector<std::string> keys(const Map& m) {
        std::vector<std::string> out;
        out.reserve(m.size());
        for (const auto& kv : m) out.push_back(kv.first);
        return out;
    }

    static void warn(const std::string& f, int line, const std::string& msg) {
        std::cerr << "[assets] " << f << ":" << line << ": " << msg << "\n";
    }
};

#endif // GAME_ASSETS_H
