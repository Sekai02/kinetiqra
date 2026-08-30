#pragma once

#include <kinetiqra/math/Ray.hpp>
#include <kinetiqra/math/Types.hpp>

namespace kinetiqra::math {

// A camera that orbits a point in space, which is the interaction model every
// modelling tool uses: the subject stays put and the viewer moves around it.
//
// This type is deliberately free of GL, GLFW and ImGui. It holds no resources
// and touches no device, so it can be reasoned about and tested on its own;
// mapping input events onto it belongs to the editor.
class OrbitCamera {
public:
    // Angles are radians. Pitch is measured from the horizon, positive upwards,
    // and is clamped just short of the poles because the view basis is degenerate
    // when the forward axis is parallel to world up.
    static constexpr float kMinPitch = -1.5533430F;  // just under -89 degrees
    static constexpr float kMaxPitch = 1.5533430F;   // just under +89 degrees
    static constexpr float kMinDistance = 0.05F;
    static constexpr float kMaxDistance = 10000.0F;

    // Rotates around the target. Pitch is clamped, yaw is free to wrap.
    void orbit(float delta_yaw, float delta_pitch);

    // Moves the target in the plane the viewer is facing. The offsets are in
    // pixels and are scaled by distance, so panning feels the same whether the
    // camera is close to the subject or far from it.
    void pan(float delta_x, float delta_y, Vec2 viewport_size);

    // Moves towards or away from the target. The step is proportional to the
    // current distance, so approaching a subject slows down rather than
    // overshooting it, and the result is clamped at both ends.
    void dolly(float delta);

    void frame(Vec3 target, float distance);

    [[nodiscard]] Vec3 position() const;
    [[nodiscard]] Vec3 forward() const;
    [[nodiscard]] Vec3 right() const;
    [[nodiscard]] Vec3 up() const;

    [[nodiscard]] Mat4 view() const;
    [[nodiscard]] Mat4 projection(float aspect) const;
    [[nodiscard]] Mat4 view_projection(float aspect) const;

    // The ray under a pixel, measured from the top left corner the way a window
    // system reports the pointer.
    //
    // This is the inverse of the projection this class already builds, which is
    // why it lives here, and it keeps the unprojection free of any window
    // system, so picking can be reasoned about without one.
    [[nodiscard]] Ray ray_through(Vec2 pixel, Vec2 viewport_size) const;

    [[nodiscard]] Vec3 target() const { return target_; }

    [[nodiscard]] float distance() const { return distance_; }

    [[nodiscard]] float yaw() const { return yaw_; }

    [[nodiscard]] float pitch() const { return pitch_; }

    void set_field_of_view(float radians) { field_of_view_ = radians; }

    [[nodiscard]] float field_of_view() const { return field_of_view_; }

    [[nodiscard]] float near_plane() const { return near_plane_; }

    [[nodiscard]] float far_plane() const { return far_plane_; }

private:
    Vec3 target_{0.0F, 0.0F, 0.0F};
    float distance_{6.0F};
    float yaw_{0.7853982F};    // 45 degrees, so neither axis is edge on
    float pitch_{0.4886922F};  // 28 degrees, looking slightly down at the floor

    float field_of_view_{0.7853982F};  // 45 degrees vertical
    float near_plane_{0.01F};
    float far_plane_{1000.0F};
};

}  // namespace kinetiqra::math
