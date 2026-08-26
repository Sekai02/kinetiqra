#include <kinetiqra/math/OrbitCamera.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>

using kinetiqra::math::OrbitCamera;
using kinetiqra::math::Vec2;
using kinetiqra::math::Vec3;
using kinetiqra::math::Vec4;

namespace {

constexpr float kEpsilon = 1e-4F;

bool near_equal(float a, float b, float epsilon = kEpsilon) {
    return std::fabs(a - b) <= epsilon;
}

bool near_equal(Vec3 a, Vec3 b, float epsilon = kEpsilon) {
    return near_equal(a.x, b.x, epsilon) && near_equal(a.y, b.y, epsilon) &&
           near_equal(a.z, b.z, epsilon);
}

}  // namespace

TEST_CASE("orbit rotates around the target without moving it") {
    OrbitCamera camera;
    const Vec3 target = camera.target();
    const float distance = camera.distance();

    camera.orbit(0.4F, 0.1F);

    CHECK(near_equal(camera.target(), target));
    CHECK(near_equal(camera.distance(), distance));
    CHECK(near_equal(glm::length(camera.position() - target), distance));
}

TEST_CASE("pitch is clamped short of the poles") {
    OrbitCamera camera;

    SUBCASE("looking straight down") {
        camera.orbit(0.0F, 100.0F);
        CHECK(near_equal(camera.pitch(), OrbitCamera::kMaxPitch));
    }

    SUBCASE("looking straight up") {
        camera.orbit(0.0F, -100.0F);
        CHECK(near_equal(camera.pitch(), OrbitCamera::kMinPitch));
    }

    // The basis stays usable at the limit, which is the reason for the clamp.
    CHECK(near_equal(glm::length(camera.right()), 1.0F));
    CHECK(near_equal(glm::length(camera.up()), 1.0F));
}

TEST_CASE("dolly is proportional and clamped at both ends") {
    OrbitCamera camera;

    SUBCASE("a step scales the distance rather than offsetting it") {
        camera.frame(Vec3{0.0F}, 10.0F);
        camera.dolly(1.0F);
        const float from_ten = camera.distance();

        camera.frame(Vec3{0.0F}, 100.0F);
        camera.dolly(1.0F);
        const float from_hundred = camera.distance();

        CHECK(near_equal(from_hundred / from_ten, 10.0F, 1e-3F));
    }

    SUBCASE("approaching stops at the minimum") {
        camera.frame(Vec3{0.0F}, 1.0F);
        for (int i = 0; i < 500; ++i) {
            camera.dolly(1.0F);
        }
        CHECK(near_equal(camera.distance(), OrbitCamera::kMinDistance));
    }

    SUBCASE("retreating stops at the maximum") {
        camera.frame(Vec3{0.0F}, 1.0F);
        for (int i = 0; i < 500; ++i) {
            camera.dolly(-1.0F);
        }
        CHECK(near_equal(camera.distance(), OrbitCamera::kMaxDistance));
    }
}

TEST_CASE("the view matrix places the target down the negative z axis") {
    OrbitCamera camera;
    camera.frame(Vec3{2.0F, -1.0F, 3.0F}, 7.0F);
    camera.orbit(0.9F, -0.3F);

    const Vec4 target_in_view = camera.view() * Vec4{camera.target(), 1.0F};

    CHECK(near_equal(target_in_view.x, 0.0F));
    CHECK(near_equal(target_in_view.y, 0.0F));
    CHECK(near_equal(target_in_view.z, -camera.distance()));
}

TEST_CASE("pan moves the target across the view plane, not the world axes") {
    OrbitCamera camera;
    camera.orbit(0.6F, 0.0F);  // so that no view axis lines up with a world axis

    const Vec3 before = camera.target();
    const Vec3 right = camera.right();
    const Vec3 up = camera.up();

    camera.pan(30.0F, 0.0F, Vec2{800.0F, 600.0F});

    const Vec3 movement = camera.target() - before;

    CHECK(glm::length(movement) > 0.0F);
    CHECK(near_equal(glm::dot(movement, camera.forward()), 0.0F));
    CHECK(glm::dot(movement, right) < 0.0F);  // dragging right pulls the scene right
    CHECK(near_equal(glm::dot(movement, up), 0.0F));
}

TEST_CASE("pan scales with distance so the drag tracks the cursor") {
    OrbitCamera near_camera;
    OrbitCamera far_camera;
    near_camera.frame(Vec3{0.0F}, 2.0F);
    far_camera.frame(Vec3{0.0F}, 20.0F);

    const Vec2 viewport{800.0F, 600.0F};
    near_camera.pan(50.0F, 0.0F, viewport);
    far_camera.pan(50.0F, 0.0F, viewport);

    const float near_shift = glm::length(near_camera.target());
    const float far_shift = glm::length(far_camera.target());

    CHECK(near_equal(far_shift / near_shift, 10.0F, 1e-3F));
}

TEST_CASE("pan is ignored for a degenerate viewport") {
    OrbitCamera camera;
    const Vec3 before = camera.target();

    camera.pan(10.0F, 10.0F, Vec2{0.0F, 0.0F});

    CHECK(near_equal(camera.target(), before));
}

TEST_CASE("the projection is finite and right handed") {
    OrbitCamera camera;
    const auto projection = camera.projection(16.0F / 9.0F);

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            CHECK(std::isfinite(projection[column][row]));
        }
    }

    // A right-handed projection flips the sign of z on its way to clip space.
    CHECK(projection[2][3] < 0.0F);

    const Vec4 in_front = projection * Vec4{0.0F, 0.0F, -camera.distance(), 1.0F};
    CHECK(in_front.w > 0.0F);
}
