#ifndef TEXTURE_H
#define TEXTURE_H

#include "Vec3.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// Decode a compressed image (PNG, JPG, BMP, ...) into tightly-packed RGB bytes.
// Implemented in src/ImageIO.cpp via the vendored stb_image; declared here so
// the header stays lightweight. Returns false if the file cannot be decoded
// (e.g. an unsupported format), letting callers fall back to the PPM reader.
namespace rt_imageio {
bool loadImageRGB(const std::string& path, int& width, int& height,
                  std::vector<uint8_t>& rgbOut);
}

// ---------------------------------------------------------------------------
// Texture strategies
// ---------------------------------------------------------------------------
// A Texture maps a surface coordinate (u, v) and/or a world point to a color.
// Materials hold an optional Texture; when present, shaders sample it instead
// of the material's flat albedo. Like every other behavior in this engine,
// textures are polymorphic and hot-swappable at runtime.
//
// The `scale` member lets a single texture tile across large surfaces: the
// incoming coordinates are multiplied by `scale` before wrapping, so a value
// of 2.0 repeats the pattern twice as often.
class Texture {
public:
    virtual ~Texture() {}

    // Sample the texture. `u`/`v` are surface coordinates (their meaning is
    // shape-defined: [0,1] for spheres, world units for planes/boxes) and `p`
    // is the world-space hit point for solid/procedural textures.
    virtual Color sample(double u, double v, const Vec3& p) const = 0;
};

// A constant color (useful as a default / placeholder).
class SolidColor : public Texture {
public:
    Color color;
    explicit SolidColor(const Color& c) : color(c) {}
    Color sample(double, double, const Vec3&) const override { return color; }
};

// Procedural checkerboard in UV space. No image file required.
class CheckerTexture : public Texture {
public:
    Color  a;
    Color  b;
    double scale;   // checks per unit of (u, v)

    CheckerTexture(const Color& c1, const Color& c2, double s = 1.0)
        : a(c1), b(c2), scale(s) {}

    Color sample(double u, double v, const Vec3&) const override {
        int iu = static_cast<int>(std::floor(u * scale));
        int iv = static_cast<int>(std::floor(v * scale));
        return ((iu + iv) & 1) ? b : a;
    }
};

// Vertical gradient driven by world height (handy for skies / accent walls).
class GradientTexture : public Texture {
public:
    Color  bottom;
    Color  top;
    double yLow;
    double yHigh;

    GradientTexture(const Color& lo, const Color& hi,
                    double y0 = 0.0, double y1 = 3.0)
        : bottom(lo), top(hi), yLow(y0), yHigh(y1) {}

    Color sample(double, double, const Vec3& p) const override {
        double t = (p.y - yLow) / (yHigh - yLow);
        t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        return (1.0 - t) * bottom + t * top;
    }
};

// Image texture loaded from a file. PNG / JPG / BMP / GIF / TGA / PSD are
// decoded through the vendored stb_image; binary PPM (P6) is handled by a
// built-in reader so the library still works even without stb. Coordinates
// tile (wrap) so a small texture can cover a large wall.
class ImageTexture : public Texture {
public:
    double scale = 1.0;   // texture repeats per unit of (u, v)

    ImageTexture() = default;

    bool valid() const { return width_ > 0 && height_ > 0; }
    int  width()  const { return width_; }
    int  height() const { return height_; }

    // Load any supported image (PNG, JPG, BMP, PPM, ...). Returns nullptr on
    // failure so the caller can fall back to a flat color. The material's
    // albedo is applied as a tint when sampling.
    static std::shared_ptr<ImageTexture> load(const std::string& path,
                                              double scale = 1.0) {
        auto tex = std::make_shared<ImageTexture>();
        tex->scale = scale;
        int w = 0, h = 0;
        std::vector<uint8_t> px;
        if (rt_imageio::loadImageRGB(path, w, h, px) && w > 0 && h > 0) {
            tex->width_  = w;
            tex->height_ = h;
            tex->pixels_ = std::move(px);
            return tex;
        }
        // stb could not decode it (or is unavailable): try raw PPM.
        if (tex->readPPM(path)) return tex;
        return nullptr;
    }

    // Back-compat alias: load a binary PPM (P6) specifically.
    static std::shared_ptr<ImageTexture> loadPPM(const std::string& path,
                                                 double scale = 1.0) {
        auto tex = std::make_shared<ImageTexture>();
        tex->scale = scale;
        if (tex->readPPM(path)) return tex;
        return nullptr;
    }

    Color sample(double u, double v, const Vec3&) const override {
        if (!valid()) return Color(1, 0, 1);   // magenta = "missing texture"

        // Wrap into [0,1) after applying the tiling scale.
        double su = u * scale;
        double sv = v * scale;
        su -= std::floor(su);
        sv -= std::floor(sv);

        // Nearest-texel lookup (v flipped so images appear right-side up).
        int x = static_cast<int>(su * width_);
        int y = static_cast<int>((1.0 - sv) * height_);
        if (x < 0) x = 0; else if (x >= width_) x = width_ - 1;
        if (y < 0) y = 0; else if (y >= height_) y = height_ - 1;

        size_t idx = (size_t(y) * width_ + x) * 3;
        const double inv = 1.0 / 255.0;
        return Color(pixels_[idx + 0] * inv,
                     pixels_[idx + 1] * inv,
                     pixels_[idx + 2] * inv);
    }

private:
    bool readPPM(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        std::string magic;
        in >> magic;
        if (magic != "P6") return false;

        int maxval = 0;
        if (!readHeaderInt(in, width_) ||
            !readHeaderInt(in, height_) ||
            !readHeaderInt(in, maxval)) {
            return false;
        }
        if (width_ <= 0 || height_ <= 0 || maxval != 255) return false;

        in.get();   // consume the single whitespace after maxval
        pixels_.resize(size_t(width_) * height_ * 3);
        in.read(reinterpret_cast<char*>(pixels_.data()),
                std::streamsize(pixels_.size()));
        return bool(in);
    }

    // Read an integer from the PPM header, skipping '#' comment lines.
    static bool readHeaderInt(std::istream& in, int& out) {
        for (;;) {
            int c = in.peek();
            if (c == EOF) return false;
            if (std::isspace(c)) { in.get(); continue; }
            if (c == '#') { std::string skip; std::getline(in, skip); continue; }
            break;
        }
        return bool(in >> out);
    }

    int width_  = 0;
    int height_ = 0;
    std::vector<uint8_t> pixels_;
};

#endif // TEXTURE_H
