#include <kinetiqra/math/OrbitCamera.hpp>
#include <kinetiqra/math/Ray.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

using kinetiqra::math::intersect_triangle;
using kinetiqra::math::OrbitCamera;
using kinetiqra::math::Ray;
using kinetiqra::math::Vec2;
using kinetiqra::math::Vec3;

namespace {

// A triangle in the z = 0 plane, wound counter clockwise seen from +Z.
constexpr Vec3 kA{-1.0F, -1.0F, 0.0F};
constexpr Vec3 kB{1.0F, -1.0F, 0.0F};
constexpr Vec3 kC{0.0F, 1.0F, 0.0F};

}  // namespace

TEST_CASE("a ray through the middle of a triangle hits it") {
    const Ray ray{Vec3{0.0F, 0.0F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}};

    float distance = 0.0F;
    REQUIRE(intersect_triangle(ray, kA, kB, kC, distance));
    CHECK(distance == doctest::Approx(5.0F));
    CHECK(ray.at(distance).z == doctest::Approx(0.0F));
}

TEST_CASE("a ray beside a triangle misses it") {
    const Ray ray{Vec3{5.0F, 5.0F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}};

    float distance = 0.0F;
    CHECK_FALSE(intersect_triangle(ray, kA, kB, kC, distance));
}

TEST_CASE("a triangle behind the ray is not hit") {
    // Pointing away from it: what is behind the camera must not be selectable.
    const Ray ray{Vec3{0.0F, 0.0F, 5.0F}, Vec3{0.0F, 0.0F, 1.0F}};

    float distance = 0.0F;
    CHECK_FALSE(intersect_triangle(ray, kA, kB, kC, distance));
}

TEST_CASE("a ray edge on to a triangle misses rather than dividing by zero") {
    const Ray ray{Vec3{0.0F, 0.0F, 0.0F}, Vec3{1.0F, 0.0F, 0.0F}};

    float distance = 0.0F;
    CHECK_FALSE(intersect_triangle(ray, kA, kB, kC, distance));
}

TEST_CASE("a triangle is hit from behind as well as from in front") {
    // A modelling tool has to let the inside of a surface be picked, and the
    // winding of an imported mesh was not the user's decision.
    const Ray ray{Vec3{0.0F, 0.0F, -5.0F}, Vec3{0.0F, 0.0F, 1.0F}};

    float distance = 0.0F;
    REQUIRE(intersect_triangle(ray, kA, kB, kC, distance));
    CHECK(distance == doctest::Approx(5.0F));
}

TEST_CASE("the ray through the centre of the screen points at what the camera looks at") {
    OrbitCamera camera;
    camera.frame(Vec3{1.0F, 2.0F, 3.0F}, 10.0F);

    const Vec2 size{800.0F, 600.0F};
    const Ray ray = camera.ray_through(Vec2{400.0F, 300.0F}, size);

    CHECK(ray.origin.x == doctest::Approx(camera.position().x));
    CHECK(ray.direction.x == doctest::Approx(camera.forward().x));
    CHECK(ray.direction.y == doctest::Approx(camera.forward().y));
    CHECK(ray.direction.z == doctest::Approx(camera.forward().z));

    // Which is to say it passes through the point being orbited.
    const float distance = camera.distance();
    CHECK(glm::distance(ray.at(distance), camera.target()) == doctest::Approx(0.0F));
}

TEST_CASE("the ray direction is a unit vector wherever it is taken") {
    const OrbitCamera camera;
    const Vec2 size{1024.0F, 768.0F};

    for (const Vec2 pixel : {Vec2{0.0F, 0.0F}, Vec2{1024.0F, 0.0F}, Vec2{0.0F, 768.0F},
                             Vec2{1024.0F, 768.0F}, Vec2{123.0F, 456.0F}}) {
        const Ray ray = camera.ray_through(pixel, size);
        CHECK(glm::length(ray.direction) == doctest::Approx(1.0F));
    }
}

TEST_CASE("the top of the screen is above the bottom of it") {
    const OrbitCamera camera;
    const Vec2 size{800.0F, 600.0F};

    const Ray top = camera.ray_through(Vec2{400.0F, 0.0F}, size);
    const Ray bottom = camera.ray_through(Vec2{400.0F, 600.0F}, size);

    // Pixels are counted downwards and the world is not, so getting this
    // backwards would invert every click.
    CHECK(glm::dot(top.direction, camera.up()) > glm::dot(bottom.direction, camera.up()));
}

TEST_CASE("the right of the screen is to the right") {
    const OrbitCamera camera;
    const Vec2 size{800.0F, 600.0F};

    const Ray left = camera.ray_through(Vec2{0.0F, 300.0F}, size);
    const Ray right = camera.ray_through(Vec2{800.0F, 300.0F}, size);

    CHECK(glm::dot(right.direction, camera.right()) > glm::dot(left.direction, camera.right()));
}

TEST_CASE("a viewport with no area gives the forward ray rather than a division by zero") {
    const OrbitCamera camera;

    const Ray ray = camera.ray_through(Vec2{0.0F, 0.0F}, Vec2{0.0F, 0.0F});

    CHECK(ray.direction.x == doctest::Approx(camera.forward().x));
    CHECK(ray.direction.z == doctest::Approx(camera.forward().z));
}
