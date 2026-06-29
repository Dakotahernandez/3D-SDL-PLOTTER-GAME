#ifndef GAME_LEVEL_BROWSER_H
#define GAME_LEVEL_BROWSER_H

// ---------------------------------------------------------------------------
// LevelBrowser: discover the .map files under a directory for the menu.
// ---------------------------------------------------------------------------

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

struct LevelEntry {
    std::string name;   // display name (file stem)
    std::string path;   // full path to the .map file
};

inline std::vector<LevelEntry> scanLevels(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<LevelEntry> out;
    std::error_code ec;
    if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".map") {
                out.push_back({entry.path().stem().string(),
                               entry.path().string()});
            }
        }
    }
    std::sort(out.begin(), out.end(),
              [](const LevelEntry& a, const LevelEntry& b) {
                  return a.name < b.name;
              });
    return out;
}

#endif // GAME_LEVEL_BROWSER_H
