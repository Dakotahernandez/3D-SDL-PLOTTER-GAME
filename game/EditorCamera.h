#ifndef GAME_EDITOR_CAMERA_H
#define GAME_EDITOR_CAMERA_H

// ---------------------------------------------------------------------------
// EditorCamera: the dev-view viewport camera
// ---------------------------------------------------------------------------
// Two navigation modes, switchable at runtime:
//   * Fly    : an Unreal-style free camera. Hold RMB to look, WASD + QE to fly.
//   * TopDown: an orthographic plan view looking straight down, for laying out
//              floor plans. Pan with WASD, zoom with the wheel.
//
// Besides driving the renderer, the camera provides the two operations an
// editor needs from any viewport:
//   * pickRay(px, py)   : a world-space ray through a window pixel (for
//                         clicking objects / the floor under the cursor).
//   * project(world)    : world point -> window pixel (+ visibility), used to
//                         draw selection markers and labels over the 3D view.
// ---------------------------------------------------------------------------

#include <raytracer/Camera.h>
#include <raytracer/Ray.h>
#include <raytracer/Vec3.h>

#include <cmath>
#include <memory>

class EditorCamera {
public:
    enum class Mode { Fly, TopDown };

    Mode   mode    = Mode::Fly;
    Vec3   position = Vec3(0, 4, 12);
    double yaw      = 0.0;     // radians; 0 looks down -Z
    double pitch    = -0.35;   // slight downward tilt
    double vfov     = 70.0;    // perspective field of view
    double orthoH   = 24.0;    // top-down view height in world units
    double flySpeed = 8.0;

    // Window/render aspect, refreshed each frame by the host.
    void setAspect(double a) { aspect_ = a; }

    Vec3 forward() const {
        double cp = std::cos(pitch);
        return Vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp);
    }
    Vec3 forwardFlat() const { return Vec3(std::sin(yaw), 0, -std::cos(yaw)); }
    Vec3 right() const { return unit_vector(cross(forwardFlat(), Vec3(0, 1, 0))); }

    void addLook(double dxPixels, double dyPixels, double sens = 0.0025) {
        yaw   += dxPixels * sens;
        pitch -= dyPixels * sens;
        const double lim = 1.5533;
        if (pitch >  lim) pitch =  lim;
        if (pitch < -lim) pitch = -lim;
    }

    // Build / refresh a renderer camera matching the current mode.
    std::shared_ptr<Camera> makeRenderCamera() const {
        if (mode == Mode::TopDown) {
            Vec3 from = Vec3(position.x, 40.0, position.z);
            Vec3 at   = Vec3(position.x, 0.0, position.z);
            auto cam = std::make_shared<OrthographicCamera>(
                from, at, Vec3(0, 0, -1), orthoH);
            cam->prepare(aspect_);
            return cam;
        }
        Vec3 at = position + forward();
        auto cam = std::make_shared<PerspectiveCamera>(
            position, at, Vec3(0, 1, 0), vfov);
        cam->prepare(aspect_);
        return cam;
    }

    // World-space ray through a window pixel (origin at top-left of window).
    Ray pickRay(int px, int py, int winW, int winH) const {
        double s = double(px) / double(winW);
        double t = 1.0 - double(py) / double(winH);   // flip: cam t grows up
        auto cam = makeRenderCamera();
        return cam->generateRay(s, t);
    }

    // Project a world point to a window pixel. Returns false if behind the
    // camera (perspective) so callers can skip drawing it.
    bool project(const Vec3& world, int winW, int winH,
                 int& outX, int& outY) const {
        Vec3 u, v, fwd;
        basis(u, v, fwd);
        Vec3 rel = world - eye();

        double s, tcoord;
        if (mode == Mode::TopDown) {
            double halfH = orthoH * 0.5;
            double halfW = halfH * aspect_;
            s = 0.5 + dot(rel, u) / (2.0 * halfW);
            tcoord = 0.5 + dot(rel, v) / (2.0 * halfH);
        } else {
            double z = dot(rel, fwd);           // forward distance
            if (z <= 1e-4) return false;        // behind / on the camera
            double halfH = std::tan(vfov * M_PI / 180.0 * 0.5);
            double halfW = halfH * aspect_;
            s = 0.5 + (dot(rel, u) / z) / (2.0 * halfW);
            tcoord = 0.5 + (dot(rel, v) / z) / (2.0 * halfH);
        }
        outX = int(s * winW);
        outY = int((1.0 - tcoord) * winH);
        return true;
    }

private:
    Vec3 eye() const {
        if (mode == Mode::TopDown) return Vec3(position.x, 40.0, position.z);
        return position;
    }
    // Camera right (u), up (v) and forward (fwd) basis vectors.
    void basis(Vec3& u, Vec3& v, Vec3& fwd) const {
        if (mode == Mode::TopDown) {
            fwd = Vec3(0, -1, 0);
            u   = Vec3(1, 0, 0);
            v   = Vec3(0, 0, -1);
            return;
        }
        fwd = forward();
        Vec3 w = -fwd;                       // backward
        u = unit_vector(cross(Vec3(0, 1, 0), w));
        v = cross(w, u);
    }

    double aspect_ = 16.0 / 9.0;
};

#endif // GAME_EDITOR_CAMERA_H
