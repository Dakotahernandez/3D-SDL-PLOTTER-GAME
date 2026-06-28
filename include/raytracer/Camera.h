#ifndef CAMERA_H
#define CAMERA_H

#include "Vec3.h"
#include "Ray.h"
#include <cmath>

// Abstract camera. Swap implementations on the Renderer to change projection
// (perspective, orthographic, fisheye, ...) without touching render code.
class Camera {
public:
    virtual ~Camera() {}

    // Called once per frame with the output aspect ratio so the camera can
    // cache any derived basis vectors.
    virtual void prepare(double aspect) { (void)aspect; }

    // Generate a primary ray for normalized screen coordinates (s, t),
    // each in [0, 1] with the origin at the bottom-left of the image.
    virtual Ray generateRay(double s, double t) const = 0;
};

// Standard pinhole perspective camera defined by a look-from / look-at pair.
class PerspectiveCamera : public Camera {
public:
    Vec3   lookFrom;
    Vec3   lookAt;
    Vec3   up;
    double vfov;   // vertical field of view in degrees

    PerspectiveCamera(const Vec3& from = Vec3(0, 1, 1),
                      const Vec3& at = Vec3(0, 0, -5),
                      const Vec3& vup = Vec3(0, 1, 0),
                      double verticalFov = 60.0)
        : lookFrom(from), lookAt(at), up(vup), vfov(verticalFov) {
        prepare(1.0);
    }

    void prepare(double aspect) override {
        double theta = vfov * M_PI / 180.0;
        double h = std::tan(theta * 0.5);
        double viewportHeight = 2.0 * h;
        double viewportWidth = aspect * viewportHeight;

        Vec3 w = unit_vector(lookFrom - lookAt);   // backward
        Vec3 u = unit_vector(cross(up, w));        // right
        Vec3 v = cross(w, u);                      // true up

        origin_ = lookFrom;
        horizontal_ = viewportWidth * u;
        vertical_ = viewportHeight * v;
        lowerLeft_ = origin_ - horizontal_ * 0.5 - vertical_ * 0.5 - w;
    }

    Ray generateRay(double s, double t) const override {
        Vec3 dir = lowerLeft_ + s * horizontal_ + t * vertical_ - origin_;
        return Ray(origin_, dir);
    }

private:
    Vec3 origin_;
    Vec3 horizontal_;
    Vec3 vertical_;
    Vec3 lowerLeft_;
};

// Orthographic camera: parallel rays, useful for CAD-style / 2.5D views.
class OrthographicCamera : public Camera {
public:
    Vec3   lookFrom;
    Vec3   lookAt;
    Vec3   up;
    double height;   // world-space height of the view volume

    OrthographicCamera(const Vec3& from = Vec3(0, 1, 1),
                       const Vec3& at = Vec3(0, 0, -5),
                       const Vec3& vup = Vec3(0, 1, 0),
                       double viewHeight = 4.0)
        : lookFrom(from), lookAt(at), up(vup), height(viewHeight) {
        prepare(1.0);
    }

    void prepare(double aspect) override {
        double viewportHeight = height;
        double viewportWidth = aspect * viewportHeight;

        forward_ = unit_vector(lookAt - lookFrom);
        Vec3 w = -forward_;
        right_ = unit_vector(cross(up, w));
        trueUp_ = cross(w, right_);

        horizontal_ = viewportWidth * right_;
        vertical_ = viewportHeight * trueUp_;
        lowerLeft_ = lookFrom - horizontal_ * 0.5 - vertical_ * 0.5;
    }

    Ray generateRay(double s, double t) const override {
        Vec3 origin = lowerLeft_ + s * horizontal_ + t * vertical_;
        return Ray(origin, forward_);
    }

private:
    Vec3 forward_;
    Vec3 right_;
    Vec3 trueUp_;
    Vec3 horizontal_;
    Vec3 vertical_;
    Vec3 lowerLeft_;
};

#endif // CAMERA_H
