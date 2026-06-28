#include "RayTracer.h"
#include "Light.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

static const double kInfinity = std::numeric_limits<double>::infinity();

// Clamp a linear color [0,1] into a packed 32-bit ARGB pixel (gamma 2.0).
static uint32_t toARGB(const Color& color) {
    auto channel = [](double x) -> int {
        x = std::sqrt(std::max(0.0, std::min(1.0, x)));
        return static_cast<int>(std::min(255.0, x * 255.0 + 0.5));
    };
    int r = channel(color.x);
    int g = channel(color.y);
    int b = channel(color.z);
    return (255u << 24) | (r << 16) | (g << 8) | b;
}

// Default background: vertical gradient from a warm horizon to sky blue.
static Color defaultBackground(const Ray& r) {
    Vec3 unitDir = unit_vector(r.dir);
    double t = 0.5 * (unitDir.y + 1.0);
    return (1.0 - t) * Color(1.0, 1.0, 1.0) + t * Color(0.5, 0.7, 1.0);
}

RayTracer::RayTracer(int w, int h)
    : camera(std::make_shared<PerspectiveCamera>()),
      shader(std::make_shared<PhongShader>()),
      background(&defaultBackground),
      width_(w), height_(h) {}

void RayTracer::setSize(int w, int h) {
    width_ = w;
    height_ = h;
}

void RayTracer::setLightPosition(const Vec3& pos) {
    for (auto& l : scene.lights) {
        if (auto p = std::dynamic_pointer_cast<PointLight>(l)) {
            p->position = pos;
            return;
        }
    }
}

Color RayTracer::trace(const Ray& r, int depth) const {
    if (depth <= 0) return Color(0, 0, 0);

    HitRecord rec;
    if (scene.hit(r, 0.001, kInfinity, rec)) {
        return shader->shade(r, rec, scene, depth, *this);
    }
    return background ? background(r) : Color(0, 0, 0);
}

void RayTracer::renderBand(uint32_t* pixels, int y0, int y1) const {
    const int s = std::max(1, samples);
    const double invS = 1.0 / double(s);

    for (int j = y0; j < y1; ++j) {
        for (int i = 0; i < width_; ++i) {
            Color color(0, 0, 0);
            for (int sy = 0; sy < s; ++sy) {
                for (int sx = 0; sx < s; ++sx) {
                    double offX = (s > 1) ? (sx + 0.5) * invS : 0.5;
                    double offY = (s > 1) ? (sy + 0.5) * invS : 0.5;
                    double u = (i + offX) / double(width_ - 1);
                    double v = (j + offY) / double(height_ - 1);
                    Ray r = camera->generateRay(u, v);
                    color = color + trace(r, maxDepth);
                }
            }
            color = color * (invS * invS);

            // The viewport's v grows upward; SDL buffers are top-left origin,
            // so flip vertically when writing.
            int row = (height_ - 1 - j);
            pixels[row * width_ + i] = toARGB(color);
        }
    }
}

void RayTracer::render(uint32_t* pixels) {
    if (!camera || !shader) return;

    camera->prepare(double(width_) / double(height_));

    unsigned hw = std::thread::hardware_concurrency();
    int numThreads = std::max(1u, hw);
    if (numThreads > height_) numThreads = height_;

    std::vector<std::thread> pool;
    pool.reserve(numThreads);
    int rowsPer = (height_ + numThreads - 1) / numThreads;

    for (int t = 0; t < numThreads; ++t) {
        int y0 = t * rowsPer;
        int y1 = std::min(height_, y0 + rowsPer);
        if (y0 >= y1) break;
        pool.emplace_back(&RayTracer::renderBand, this, pixels, y0, y1);
    }
    for (auto& th : pool) th.join();
}

