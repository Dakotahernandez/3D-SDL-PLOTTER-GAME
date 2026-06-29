#ifndef GAME_EDITOR_UI_H
#define GAME_EDITOR_UI_H

// ---------------------------------------------------------------------------
// EditorUI: draws the editor chrome and reports which button was clicked.
// ---------------------------------------------------------------------------
// A minimal immediate-mode pattern: each frame we build the toolbar, palette,
// outliner and details panels. Buttons are drawn and, if the mouse clicked
// inside one this frame, the matching action is recorded in `clicked`. The host
// (editor_main) reads `clicked` after the draw call and performs the action.
// This guarantees the visible rectangles and the hit-testing always match.
// ---------------------------------------------------------------------------

#include <SDL2/SDL.h>

#include <string>
#include <vector>

#include "Editor.h"
#include "EditorCamera.h"
#include "Map.h"
#include "Overlay.h"

struct UIResult {
    enum class Action {
        None, ToolSelect, ToolPlace, ToolDelete,
        ToggleSnap, ToggleTopDown, Save, NewMap, Play,
        PickCategory, PickItem, PickOutliner,
        Duplicate, DeleteSel
    };
    Action action = Action::None;
    int    arg     = 0;   // category index / item index / outliner row
    int    arg2    = 0;
};

class EditorUI {
public:
    EditorUI(int winW, int winH) : winW_(winW), winH_(winH) {}

    int topH()   const { return 36; }
    int leftW()  const { return 220; }
    int rightW() const { return 240; }
    int botH()   const { return 96; }

    bool inPanels(int mx, int my) const {
        if (my < topH()) return true;
        if (my >= winH_ - botH()) return true;
        if (mx < leftW() && my >= topH()) return true;
        if (mx >= winW_ - rightW() && my >= topH()) return true;
        return false;
    }

    // Draw everything and return the action triggered by a click this frame.
    UIResult draw(SDL_Renderer* r, Editor& ed, const EditorCamera& cam,
                  const Map& map, int& outlinerScroll,
                  int mx, int my, bool clicked,
                  const std::string& mapPath, const std::string& status) {
        UIResult res;
        clicked_ = clicked; mx_ = mx; my_ = my; out_ = &res;

        drawToolbar(r, ed, cam, mapPath);
        drawLeft(r, ed, map, outlinerScroll);
        drawRight(r, ed, map);
        drawBottom(r, ed, cam, status);
        drawViewportGizmo(r, ed, cam, map);
        drawCrosshair(r, ed);

        return res;
    }

private:
    // ---- toolbar (top) ----
    void drawToolbar(SDL_Renderer* r, Editor& ed, const EditorCamera& cam,
                     const std::string& mapPath) {
        ui::fillRect(r, 0, 0, winW_, topH(), ui::theme::bar());
        ui::frameRect(r, 0, 0, winW_, topH(), ui::theme::line());

        int x = 8, y = 6, h = topH() - 12;
        auto tool = [&](const char* label, Editor::Tool t, UIResult::Action a) {
            int w = ui::textWidth(label, 2) + 16;
            ui::Rect b{x, y, w, h};
            if (ui::button(r, b, label, mx_, my_, ed.tool == t) && clicked_)
                out_->action = a;
            x += w + 6;
        };
        tool("SELECT", Editor::Tool::Select, UIResult::Action::ToolSelect);
        tool("PLACE",  Editor::Tool::Place,  UIResult::Action::ToolPlace);
        tool("DELETE", Editor::Tool::Delete, UIResult::Action::ToolDelete);

        x += 10;
        toggle(r, x, y, h, "GRID", ed.snap, UIResult::Action::ToggleSnap);
        toggle(r, x, y, h, "TOP",
               cam.mode == EditorCamera::Mode::TopDown, UIResult::Action::ToggleTopDown);

        // Right-aligned file actions.
        int rx = winW_ - 8;
        auto rbtn = [&](const char* label, UIResult::Action a, ui::Color hot) {
            int w = ui::textWidth(label, 2) + 16;
            rx -= w; ui::Rect b{rx, y, w, h};
            bool hov = b.contains(mx_, my_);
            ui::panel(r, b.x, b.y, b.w, b.h, hov ? hot : ui::theme::panelDark());
            int tw = ui::textWidth(label, 2);
            ui::text(r, b.x + (w - tw) / 2, b.y + 4, label, ui::theme::text(), 2);
            if (hov && clicked_) out_->action = a;
            rx -= 6;
        };
        rbtn("PLAY", UIResult::Action::Play, ui::theme::good());
        rbtn("SAVE", UIResult::Action::Save, ui::theme::accent());
        rbtn("NEW",  UIResult::Action::NewMap, ui::theme::bar());
    }

    void toggle(SDL_Renderer* r, int& x, int y, int h, const char* label,
                bool on, UIResult::Action a) {
        int w = ui::textWidth(label, 2) + 16;
        ui::Rect b{x, y, w, h};
        if (ui::button(r, b, label, mx_, my_, on) && clicked_) out_->action = a;
        x += w + 6;
    }

    // ---- left: palette + outliner ----
    void drawLeft(SDL_Renderer* r, Editor& ed, const Map& map, int& scroll) {
        int x = 0, y = topH(), w = leftW(), h = winH_ - topH() - botH();
        ui::fillRect(r, x, y, w, h, ui::theme::panel());
        ui::frameRect(r, x, y, w, h, ui::theme::line());

        int cy = y + 8;
        ui::text(r, x + 10, cy, "PALETTE", ui::theme::accent(), 2);
        cy += 20;
        ui::text(r, x + 10, cy, "CAT: " + ed.categoryName(), ui::theme::textDim(), 2);
        cy += 18;

        // Category buttons (compact grid).
        const char* cats[] = {"WALL","BOX","SPH","PROP","ENMY","WEAP","LITE"};
        for (int i = 0; i < 7; ++i) {
            int bw = (w - 24) / 2;
            int bx = x + 10 + (i % 2) * (bw + 4);
            int by = cy + (i / 2) * 22;
            ui::Rect b{bx, by, bw, 20};
            bool on = int(ed.category) == i;
            if (ui::button(r, b, cats[i], mx_, my_, on) && clicked_) {
                out_->action = UIResult::Action::PickCategory;
                out_->arg = i;
            }
        }
        cy += 4 * 22 + 8;

        ui::text(r, x + 10, cy, "ITEM:", ui::theme::textDim(), 2);
        cy += 18;
        const auto& items = ed.palette();
        int maxItemRows = 6;
        int start = 0;
        // keep the current item visible
        if (ed.itemIndex >= maxItemRows) start = ed.itemIndex - maxItemRows + 1;
        for (int i = 0; i < maxItemRows && start + i < int(items.size()); ++i) {
            int idx = start + i;
            ui::Rect b{x + 10, cy, w - 20, 18};
            bool on = idx == ed.itemIndex;
            if (ui::button(r, b, items[idx], mx_, my_, on) && clicked_) {
                out_->action = UIResult::Action::PickItem;
                out_->arg = idx;
            }
            cy += 20;
        }

        // Outliner header.
        cy += 8;
        ui::fillRect(r, x + 4, cy, w - 8, 1, ui::theme::line());
        cy += 6;
        ui::text(r, x + 10, cy, "OUTLINER", ui::theme::accent(), 2);
        cy += 20;

        auto rows = ed.outliner(map);
        int avail = (y + h) - cy - 6;
        int rowH = 16;
        int visible = avail / rowH;
        if (visible < 1) visible = 1;
        int maxScroll = std::max(0, int(rows.size()) - visible);
        if (scroll < 0) scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;

        for (int i = 0; i < visible && scroll + i < int(rows.size()); ++i) {
            const auto& row = rows[scroll + i];
            bool on = ed.isSelected(row.kind, row.index);
            int ry = cy + i * rowH;
            if (on) ui::fillRect(r, x + 6, ry - 1, w - 12, rowH, ui::theme::accent());
            ui::Rect b{x + 6, ry - 1, w - 12, rowH};
            if (b.contains(mx_, my_) && clicked_) {
                out_->action = UIResult::Action::PickOutliner;
                out_->arg = row.kind; out_->arg2 = row.index;
            }
            ui::text(r, x + 10, ry, row.label, on ? ui::rgb(15,18,24) : ui::theme::text(), 2);
        }
        if (maxScroll > 0) {
            std::string s = std::to_string(scroll + 1) + "/" + std::to_string(int(rows.size()));
            ui::text(r, x + w - ui::textWidth(s, 1) - 8, y + 8, s, ui::theme::textDim(), 1);
        }
    }

    // ---- right: details of the selection ----
    void drawRight(SDL_Renderer* r, Editor& ed, const Map& map) {
        int w = rightW(), x = winW_ - w, y = topH(), h = winH_ - topH() - botH();
        ui::fillRect(r, x, y, w, h, ui::theme::panel());
        ui::frameRect(r, x, y, w, h, ui::theme::line());

        int cy = y + 8;
        ui::text(r, x + 10, cy, "DETAILS", ui::theme::accent(), 2);
        cy += 22;

        if (!ed.hasSelection()) {
            ui::text(r, x + 10, cy, "NOTHING", ui::theme::textDim(), 2);
            cy += 16;
            ui::text(r, x + 10, cy, "SELECTED", ui::theme::textDim(), 2);
            cy += 24;
            ui::text(r, x + 10, cy, "CLICK AN", ui::theme::textDim(), 1);
            cy += 10;
            ui::text(r, x + 10, cy, "OBJECT WITH", ui::theme::textDim(), 1);
            cy += 10;
            ui::text(r, x + 10, cy, "THE SELECT TOOL", ui::theme::textDim(), 1);
            return;
        }

        ui::text(r, x + 10, cy, ed.selectionLabel(map), ui::theme::sel(), 2);
        cy += 22;
        Vec3 p = ed.selectionPosition(map);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "X %.2f", p.x);
        ui::text(r, x + 10, cy, buf, ui::theme::text(), 2); cy += 16;
        std::snprintf(buf, sizeof(buf), "Y %.2f", p.y);
        ui::text(r, x + 10, cy, buf, ui::theme::text(), 2); cy += 16;
        std::snprintf(buf, sizeof(buf), "Z %.2f", p.z);
        ui::text(r, x + 10, cy, buf, ui::theme::text(), 2); cy += 24;

        // Action buttons.
        ui::Rect dup{x + 10, cy, w - 20, 22};
        if (ui::button(r, dup, "DUPLICATE", mx_, my_) && clicked_)
            out_->action = UIResult::Action::Duplicate;
        cy += 26;
        ui::Rect del{x + 10, cy, w - 20, 22};
        if (ui::button(r, del, "DELETE", mx_, my_) && clicked_)
            out_->action = UIResult::Action::DeleteSel;
        cy += 30;

        ui::text(r, x + 10, cy, "MOVE: ARROWS", ui::theme::textDim(), 1); cy += 10;
        ui::text(r, x + 10, cy, "UP/DN: PGUP/PGDN", ui::theme::textDim(), 1); cy += 10;
        ui::text(r, x + 10, cy, "SCALE: - / =", ui::theme::textDim(), 1);
    }

    // ---- bottom: status + hints ----
    void drawBottom(SDL_Renderer* r, Editor& ed, const EditorCamera& cam,
                    const std::string& status) {
        int y = winH_ - botH();
        ui::fillRect(r, 0, y, winW_, botH(), ui::theme::panelDark());
        ui::frameRect(r, 0, y, winW_, botH(), ui::theme::line());

        ui::text(r, 12, y + 8, "TOOL: " + ed.toolName(), ui::theme::good(), 2);
        std::string brush = "BRUSH: " + ed.categoryName() + " / " + ed.itemName();
        ui::text(r, 12, y + 30, brush, ui::theme::text(), 2);
        std::string view = cam.mode == EditorCamera::Mode::TopDown ? "VIEW: TOP-DOWN" : "VIEW: FLY";
        ui::text(r, 12, y + 52, view, ui::theme::textDim(), 2);

        ui::text(r, 12, y + 74, status, ui::theme::accentHot(), 1);

        // Right side controls cheat-sheet.
        int hx = winW_ - 560;
        ui::text(r, hx, y + 8,  "RMB LOOK   WASD/QE FLY   T TOPDOWN", ui::theme::textDim(), 1);
        ui::text(r, hx, y + 20, "1 SELECT  2 PLACE  3 DELETE  G GRID", ui::theme::textDim(), 1);
        ui::text(r, hx, y + 32, "[ ] CATEGORY   , . ITEM   WHEEL ITEM", ui::theme::textDim(), 1);
        ui::text(r, hx, y + 44, "CTRL+D DUP   DEL REMOVE   CTRL+S SAVE", ui::theme::textDim(), 1);
        ui::text(r, hx, y + 56, "P PLAY   ESC QUIT", ui::theme::textDim(), 1);
    }

    // Marker over the selected object + a compass.
    void drawViewportGizmo(SDL_Renderer* r, Editor& ed, const EditorCamera& cam,
                           const Map& map) {
        Vec3 c; double rad;
        if (ed.selectionBounds(map, c, rad)) {
            int sx, sy;
            if (cam.project(c, winW_, winH_, sx, sy) && !inPanels(sx, sy)) {
                ui::setColor(r, ui::theme::sel());
                int s = 10;
                SDL_RenderDrawLine(r, sx - s, sy, sx + s, sy);
                SDL_RenderDrawLine(r, sx, sy - s, sx, sy + s);
                SDL_Rect box{sx - s, sy - s, 2 * s, 2 * s};
                SDL_RenderDrawRect(r, &box);
            }
        }
        // Tiny axis compass bottom-left of the viewport area.
        int ox = leftW() + 28, oy = winH_ - botH() - 28;
        Vec3 origin(0, 0, 0);
        int cxp, cyp;
        if (cam.project(origin, winW_, winH_, cxp, cyp)) { /* keep simple */ }
        ui::text(r, ox - 18, oy - 6, "+X", ui::theme::textDim(), 1);
    }

    void drawCrosshair(SDL_Renderer* r, Editor& ed) {
        if (ed.tool == Editor::Tool::Select) return;   // free cursor for select
        int cx = winW_ / 2, cy = winH_ / 2, a = 8, g = 3;
        ui::setColor(r, ed.tool == Editor::Tool::Delete
                     ? ui::rgb(240, 90, 90) : ui::theme::good());
        SDL_RenderDrawLine(r, cx - a, cy, cx - g, cy);
        SDL_RenderDrawLine(r, cx + g, cy, cx + a, cy);
        SDL_RenderDrawLine(r, cx, cy - a, cx, cy - g);
        SDL_RenderDrawLine(r, cx, cy + g, cx, cy + a);
    }

    int  winW_, winH_;
    bool clicked_ = false;
    int  mx_ = 0, my_ = 0;
    UIResult* out_ = nullptr;
};

#endif // GAME_EDITOR_UI_H
