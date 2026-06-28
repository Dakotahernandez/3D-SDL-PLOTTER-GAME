#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <raytracer/Vec3.h>
#include <raytracer/Ray.h>
#include <raytracer/Camera.h>
#include <cmath>
#include <vector>

// First-person player / camera controller.
//
// The ray tracer's PerspectiveCamera is driven by a look-from / look-at pair.
// An FPS instead wants to think in terms of a position plus yaw (turn) and
// pitch (look up/down) angles, so this class owns those and projects them onto
// the camera every frame via applyToCamera().
//
// `position` is the eye position in world space.
//   yaw   : rotation around the +Y axis, radians. 0 looks down -Z.
//   pitch : look up (+) / down (-), radians, clamped to avoid gimbal flip.
class Player {
public:
    Vec3   position;
    double yaw   = 0.0;            // radians
    double pitch = 0.0;            // radians

    double moveSpeed        = 4.5; // world units / second
    double mouseSensitivity = 0.0022;
    double radius           = 0.3; // collision radius in the XZ plane

    explicit Player(const Vec3& start = Vec3(0, 0, 0),
                    double startYaw = 0.0, double startPitch = 0.0)
        : position(start), yaw(startYaw), pitch(startPitch) {}

    // Full view direction including pitch (used to aim the camera and shots).
    Vec3 forward() const {
        double cp = std::cos(pitch);
        return Vec3(std::sin(yaw) * cp,
                    std::sin(pitch),
                    -std::cos(yaw) * cp);
    }

    // Horizontal heading only (used for walking so looking up/down does not
    // slow you down or lift you off the ground).
    Vec3 forwardFlat() const {
        return Vec3(std::sin(yaw), 0.0, -std::cos(yaw));
    }

    // Unit vector pointing to the player's right, on the ground plane.
    Vec3 right() const {
        return unit_vector(cross(forwardFlat(), Vec3(0, 1, 0)));
    }

    // Apply a relative mouse motion (in pixels) to the look angles.
    void addLook(double dxPixels, double dyPixels) {
        yaw   += dxPixels * mouseSensitivity;
        pitch -= dyPixels * mouseSensitivity;
        const double limit = 1.55334;          // ~89 degrees
        if (pitch >  limit) pitch =  limit;
        if (pitch < -limit) pitch = -limit;
    }

    // Push the current pose into a perspective camera.
    void applyToCamera(PerspectiveCamera& cam) const {
        cam.lookFrom = position;
        cam.lookAt   = position + forward();
        cam.up       = Vec3(0, 1, 0);
    }

    // A ray fired straight down the center of view (for shooting / picking).
    Ray aimRay() const {
        return Ray(position, forward());
    }
};

// Axis-aligned wall footprint used for cheap XZ collision. The player is
// treated as a circle of `radius`; movement is resolved per-axis so you slide
// along walls instead of sticking to them.
struct WallAABB {
    double minX, maxX, minZ, maxZ;
};

// Resolve a desired displacement against the level walls. Movement is split
// into independent X and Z steps so a blocked axis does not cancel the other.
inline void moveWithCollision(Player& player, const Vec3& delta,
                              const std::vector<WallAABB>& walls) {
    auto collides = [&](double x, double z) {
        for (const auto& w : walls) {
            double nearestX = std::fmax(w.minX, std::fmin(x, w.maxX));
            double nearestZ = std::fmax(w.minZ, std::fmin(z, w.maxZ));
            double dx = x - nearestX;
            double dz = z - nearestZ;
            if (dx * dx + dz * dz < player.radius * player.radius) return true;
        }
        return false;
    };

    double newX = player.position.x + delta.x;
    if (!collides(newX, player.position.z)) player.position.x = newX;

    double newZ = player.position.z + delta.z;
    if (!collides(player.position.x, newZ)) player.position.z = newZ;
}

#endif // GAME_PLAYER_H
