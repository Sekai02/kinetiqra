#include <kinetiqra/math/OrbitCamera.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace kinetiqra::math {

void OrbitCamera::orbit(float delta_yaw, float delta_pitch) {
    yaw_ += delta_yaw;
    pitch_ = std::clamp(pitch_ + delta_pitch, kMinPitch, kMaxPitch);
}

void OrbitCamera::pan(float delta_x, float delta_y, Vec2 viewport_size) {
    if (viewport_size.y <= 0.0F) {
        return;
    }

    // One viewport height covers this much world space at the target's depth,
    // which is what makes a drag track the point under the cursor.
    const float extent = 2.0F * distance_ * std::tan(field_of_view_ * 0.5F);
    const float per_pixel = extent / viewport_size.y;

    target_ -= right() * (delta_x * per_pixel);
    target_ += up() * (delta_y * per_pixel);
}

void OrbitCamera::dolly(float delta) {
    const float factor = std::pow(0.9F, delta);
    distance_ = std::clamp(distance_ * factor, kMinDistance, kMaxDistance);
}

void OrbitCamera::frame(Vec3 target, float distance) {
    target_ = target;
    distance_ = std::clamp(distance, kMinDistance, kMaxDistance);
}

Vec3 OrbitCamera::forward() const {
    const float cos_pitch = std::cos(pitch_);

    // Points from the camera towards the target.
    return Vec3{-cos_pitch * std::sin(yaw_), -std::sin(pitch_), -cos_pitch * std::cos(yaw_)};
}

Vec3 OrbitCamera::right() const {
    return glm::normalize(glm::cross(forward(), kWorldUp));
}

Vec3 OrbitCamera::up() const {
    return glm::cross(right(), forward());
}

Vec3 OrbitCamera::position() const {
    return target_ - forward() * distance_;
}

Mat4 OrbitCamera::view() const {
    return glm::lookAt(position(), target_, kWorldUp);
}

Mat4 OrbitCamera::projection(float aspect) const {
    return glm::perspective(field_of_view_, aspect, near_plane_, far_plane_);
}

Mat4 OrbitCamera::view_projection(float aspect) const {
    return projection(aspect) * view();
}

Ray OrbitCamera::ray_through(Vec2 pixel, Vec2 viewport_size) const {
    Ray ray;
    ray.origin = position();
    ray.direction = forward();

    if (viewport_size.x <= 0.0F || viewport_size.y <= 0.0F) {
        return ray;
    }

    // From pixels to the cube the projection maps the frustum onto, which runs
    // from minus one to one with y pointing up rather than down.
    const float x = ((2.0F * pixel.x) / viewport_size.x) - 1.0F;
    const float y = 1.0F - ((2.0F * pixel.y) / viewport_size.y);

    const float aspect = viewport_size.x / viewport_size.y;

    // The half extents of the near plane, which is the same relationship pan()
    // uses to turn pixels into world space.
    const float half_height = std::tan(field_of_view_ * 0.5F);
    const float half_width = half_height * aspect;

    const Vec3 direction = forward() + (right() * (x * half_width)) + (up() * (y * half_height));

    ray.direction = glm::normalize(direction);
    return ray;
}

}  // namespace kinetiqra::math
