#ifndef GAME_OVERLAY_H
#define GAME_OVERLAY_H

// ---------------------------------------------------------------------------
// Overlay: a tiny immediate-mode 2D UI layer drawn on top of the ray-traced
// frame with SDL_Renderer. SDL has no built-in text, so this provides a small
// 5x7 bitmap font plus rectangle / panel / button helpers. It is deliberately
// minimal — just enough to build an Unreal-style editor chrome (toolbars,
// outliner, details panel) and a level-select menu.
// ---------------------------------------------------------------------------

#include <SDL2/SDL.h>

#include <array>
#include <cctype>
#include <string>
#include <unordered_map>

namespace ui {

struct Color { Uint8 r, g, b, a; };

inline Color rgb(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return {r, g, b, a}; }

// A handful of named theme colors (dark, Unreal-ish).
namespace theme {
    inline Color panel()     { return rgb(28, 30, 36, 235); }
    inline Color panelDark() { return rgb(20, 21, 26, 240); }
    inline Color bar()       { return rgb(38, 41, 50, 245); }
    inline Color line()      { return rgb(60, 64, 76); }
    inline Color text()      { return rgb(220, 224, 230); }
    inline Color textDim()   { return rgb(150, 156, 166); }
    inline Color accent()    { return rgb(64, 156, 255); }
    inline Color accentHot() { return rgb(96, 184, 255); }
    inline Color good()      { return rgb(90, 200, 120); }
    inline Color warn()      { return rgb(240, 180, 70); }
    inline Color sel()       { return rgb(255, 170, 60); }
}

// ----- 5x7 bitmap font ------------------------------------------------------
// Each glyph is 7 rows; the low 5 bits of each byte are the pixels (MSB-left).
// Authored from readable row strings the first time the table is needed.

inline const std::unordered_map<char, std::array<Uint8, 7>>& fontTable() {
    static const std::unordered_map<char, std::array<Uint8, 7>> table = [] {
        auto G = [](const char* r0, const char* r1, const char* r2,
                    const char* r3, const char* r4, const char* r5,
                    const char* r6) {
            auto row = [](const char* s) -> Uint8 {
                Uint8 b = 0;
                for (int i = 0; i < 5; ++i)
                    if (s[i] && s[i] != ' ') b |= (1 << (4 - i));
                return b;
            };
            return std::array<Uint8, 7>{row(r0), row(r1), row(r2), row(r3),
                                        row(r4), row(r5), row(r6)};
        };
        std::unordered_map<char, std::array<Uint8, 7>> t;
        t[' '] = G("     ", "     ", "     ", "     ", "     ", "     ", "     ");
        t['A'] = G(" XX  ", "X  X ", "X  X ", "XXXX ", "X  X ", "X  X ", "X  X ");
        t['B'] = G("XXX  ", "X  X ", "XXX  ", "X  X ", "X  X ", "X  X ", "XXX  ");
        t['C'] = G(" XXX ", "X    ", "X    ", "X    ", "X    ", "X    ", " XXX ");
        t['D'] = G("XXX  ", "X  X ", "X  X ", "X  X ", "X  X ", "X  X ", "XXX  ");
        t['E'] = G("XXXX ", "X    ", "XXX  ", "X    ", "X    ", "X    ", "XXXX ");
        t['F'] = G("XXXX ", "X    ", "XXX  ", "X    ", "X    ", "X    ", "X    ");
        t['G'] = G(" XXX ", "X    ", "X    ", "X XX ", "X  X ", "X  X ", " XXX ");
        t['H'] = G("X  X ", "X  X ", "XXXX ", "X  X ", "X  X ", "X  X ", "X  X ");
        t['I'] = G("XXX  ", " X   ", " X   ", " X   ", " X   ", " X   ", "XXX  ");
        t['J'] = G("  XX ", "   X ", "   X ", "   X ", "X  X ", "X  X ", " XX  ");
        t['K'] = G("X  X ", "X X  ", "XX   ", "XX   ", "X X  ", "X X  ", "X  X ");
        t['L'] = G("X    ", "X    ", "X    ", "X    ", "X    ", "X    ", "XXXX ");
        t['M'] = G("X  X ", "XXXX ", "XXXX ", "X  X ", "X  X ", "X  X ", "X  X ");
        t['N'] = G("X  X ", "XX X ", "XX X ", "X XX ", "X XX ", "X  X ", "X  X ");
        t['O'] = G(" XX  ", "X  X ", "X  X ", "X  X ", "X  X ", "X  X ", " XX  ");
        t['P'] = G("XXX  ", "X  X ", "X  X ", "XXX  ", "X    ", "X    ", "X    ");
        t['Q'] = G(" XX  ", "X  X ", "X  X ", "X  X ", "X XX ", "X X  ", " XXX ");
        t['R'] = G("XXX  ", "X  X ", "X  X ", "XXX  ", "X X  ", "X  X ", "X  X ");
        t['S'] = G(" XXX ", "X    ", "X    ", " XX  ", "   X ", "   X ", "XXX  ");
        t['T'] = G("XXXXX", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ");
        t['U'] = G("X  X ", "X  X ", "X  X ", "X  X ", "X  X ", "X  X ", " XX  ");
        t['V'] = G("X  X ", "X  X ", "X  X ", "X  X ", "X  X ", " XX  ", " XX  ");
        t['W'] = G("X  X ", "X  X ", "X  X ", "X  X ", "XXXX ", "XXXX ", "X  X ");
        t['X'] = G("X  X ", "X  X ", " XX  ", " XX  ", " XX  ", "X  X ", "X  X ");
        t['Y'] = G("X  X ", "X  X ", " XX  ", "  X  ", "  X  ", "  X  ", "  X  ");
        t['Z'] = G("XXXX ", "   X ", "  X  ", " X   ", "X    ", "X    ", "XXXX ");
        t['0'] = G(" XX  ", "X  X ", "X XX ", "XX X ", "X  X ", "X  X ", " XX  ");
        t['1'] = G("  X  ", " XX  ", "  X  ", "  X  ", "  X  ", "  X  ", " XXX ");
        t['2'] = G(" XX  ", "X  X ", "   X ", "  X  ", " X   ", "X    ", "XXXX ");
        t['3'] = G("XXX  ", "   X ", "   X ", " XX  ", "   X ", "   X ", "XXX  ");
        t['4'] = G("  XX ", " X X ", "X  X ", "XXXX ", "   X ", "   X ", "   X ");
        t['5'] = G("XXXX ", "X    ", "XXX  ", "   X ", "   X ", "X  X ", " XX  ");
        t['6'] = G(" XX  ", "X    ", "X    ", "XXX  ", "X  X ", "X  X ", " XX  ");
        t['7'] = G("XXXX ", "   X ", "  X  ", "  X  ", " X   ", " X   ", " X   ");
        t['8'] = G(" XX  ", "X  X ", "X  X ", " XX  ", "X  X ", "X  X ", " XX  ");
        t['9'] = G(" XX  ", "X  X ", "X  X ", " XXX ", "   X ", "   X ", " XX  ");
        t['.'] = G("     ", "     ", "     ", "     ", "     ", " XX  ", " XX  ");
        t[','] = G("     ", "     ", "     ", "     ", " XX  ", " XX  ", " X   ");
        t[':'] = G("     ", " XX  ", " XX  ", "     ", " XX  ", " XX  ", "     ");
        t[';'] = G("     ", " XX  ", " XX  ", "     ", " XX  ", " XX  ", " X   ");
        t['!'] = G("  X  ", "  X  ", "  X  ", "  X  ", "  X  ", "     ", "  X  ");
        t['?'] = G(" XX  ", "X  X ", "   X ", "  X  ", " X   ", "     ", " X   ");
        t['-'] = G("     ", "     ", "     ", "XXXX ", "     ", "     ", "     ");
        t['+'] = G("     ", "  X  ", "  X  ", "XXXXX", "  X  ", "  X  ", "     ");
        t['='] = G("     ", "     ", "XXXX ", "     ", "XXXX ", "     ", "     ");
        t['_'] = G("     ", "     ", "     ", "     ", "     ", "     ", "XXXXX");
        t['/'] = G("   X ", "   X ", "  X  ", " X   ", " X   ", "X    ", "X    ");
        t['\\']= G("X    ", "X    ", " X   ", " X   ", "  X  ", "   X ", "   X ");
        t['*'] = G("     ", "X X X", " XXX ", "XXXXX", " XXX ", "X X X", "     ");
        t['('] = G("  X  ", " X   ", " X   ", " X   ", " X   ", " X   ", "  X  ");
        t[')'] = G(" X   ", "  X  ", "  X  ", "  X  ", "  X  ", "  X  ", " X   ");
        t['['] = G(" XXX ", " X   ", " X   ", " X   ", " X   ", " X   ", " XXX ");
        t[']'] = G(" XXX ", "   X ", "   X ", "   X ", "   X ", "   X ", " XXX ");
        t['<'] = G("   X ", "  X  ", " X   ", "X    ", " X   ", "  X  ", "   X ");
        t['>'] = G("X    ", " X   ", "  X  ", "   X ", "  X  ", " X   ", "X    ");
        t['%'] = G("X   X", "   X ", "  X  ", " X   ", "X    ", "X   X", "    X");
        t['#'] = G(" X X ", "XXXXX", " X X ", " X X ", "XXXXX", " X X ", "     ");
        t['\''] = G("  X  ", "  X  ", " X   ", "     ", "     ", "     ", "     ");
        t['"'] = G(" X X ", " X X ", " X X ", "     ", "     ", "     ", "     ");
        return t;
    }();
    return table;
}

// ----- Drawing primitives ---------------------------------------------------

inline void setColor(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

inline void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
    setColor(r, c);
    SDL_Rect rc{x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}

inline void frameRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
    setColor(r, c);
    SDL_Rect rc{x, y, w, h};
    SDL_RenderDrawRect(r, &rc);
}

// A filled panel with a 1px border.
inline void panel(SDL_Renderer* r, int x, int y, int w, int h,
                  Color fill = theme::panel(), Color border = theme::line()) {
    fillRect(r, x, y, w, h, fill);
    frameRect(r, x, y, w, h, border);
}

inline int charWidth(int scale)  { return 5 * scale + scale; }   // glyph + gap
inline int lineHeight(int scale) { return 7 * scale + scale; }

inline int textWidth(const std::string& s, int scale) {
    return int(s.size()) * charWidth(scale);
}

// Draw a single line of text. Lowercase is folded to uppercase (the font is
// uppercase-only). Unknown glyphs render as blanks.
inline void text(SDL_Renderer* r, int x, int y, const std::string& s,
                 Color color, int scale = 2) {
    const auto& font = fontTable();
    setColor(r, color);
    int cx = x;
    for (char ch : s) {
        char up = char(std::toupper((unsigned char)ch));
        auto it = font.find(up);
        if (it != font.end()) {
            const auto& g = it->second;
            for (int row = 0; row < 7; ++row) {
                Uint8 bits = g[row];
                for (int col = 0; col < 5; ++col) {
                    if (bits & (1 << (4 - col))) {
                        SDL_Rect px{cx + col * scale, y + row * scale,
                                    scale, scale};
                        SDL_RenderFillRect(r, &px);
                    }
                }
            }
        }
        cx += charWidth(scale);
    }
}

// A simple clickable button. Returns true if the cursor is inside it (hover);
// the caller decides what a click does. `hot` highlights the active/selected
// state. Bounds are returned so hit-testing can reuse them.
struct Rect { int x, y, w, h; bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h; } };

inline bool button(SDL_Renderer* r, Rect b, const std::string& label,
                   int mx, int my, bool hot = false) {
    bool hover = b.contains(mx, my);
    Color fill = hot ? theme::accent()
                     : (hover ? theme::bar() : theme::panelDark());
    panel(r, b.x, b.y, b.w, b.h, fill, theme::line());
    int scale = 2;
    int tw = textWidth(label, scale);
    int tx = b.x + (b.w - tw) / 2;
    int ty = b.y + (b.h - lineHeight(scale)) / 2 + 1;
    Color tc = hot ? rgb(15, 18, 24) : theme::text();
    text(r, tx, ty, label, tc, scale);
    return hover;
}

} // namespace ui

#endif // GAME_OVERLAY_H
