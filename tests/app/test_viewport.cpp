#include <kinetiqra/app/Viewport.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>

using kinetiqra::app::CameraInput;
using kinetiqra::app::Viewport;
using kinetiqra::math::Vec2;
using kinetiqra::math::Vec3;

namespace {

constexpr Vec2 kRenderSize{1280.0F, 800.0F};

CameraInput drag_left(float dx, float dy, bool over_world) {
    CameraInput input;
    input.delta = Vec2{dx, dy};
    input.left = true;
    input.over_world = over_world;
    return input;
}

bool moved(const Viewport& before, const Viewport& after) {
    return glm::length(after.camera().position() - before.camera().position()) > 1e-5F;
}

}  // namespace

TEST_CASE("a drag beginning over a panel never takes control") {
    Viewport viewport;
    const Viewport untouched;

    viewport.update(drag_left(20.0F, 0.0F, false), kRenderSize);

    CHECK_FALSE(viewport.dragging());
    CHECK_FALSE(moved(untouched, viewport));
}

TEST_CASE("a drag beginning over the world survives crossing a panel") {
    Viewport viewport;

    viewport.update(drag_left(5.0F, 0.0F, true), kRenderSize);
    REQUIRE(viewport.dragging());

    const float yaw_over_world = viewport.camera().yaw();

    // The pointer moves onto a panel without the button being released.
    viewport.update(drag_left(5.0F, 0.0F, false), kRenderSize);

    CHECK(viewport.dragging());
    CHECK(viewport.camera().yaw() != doctest::Approx(yaw_over_world));
}

TEST_CASE("releasing the button ends the drag") {
    Viewport viewport;

    viewport.update(drag_left(5.0F, 0.0F, true), kRenderSize);
    REQUIRE(viewport.dragging());

    viewport.update(CameraInput{}, kRenderSize);
    CHECK_FALSE(viewport.dragging());

    // And the next drag has to start over the world again.
    viewport.update(drag_left(5.0F, 0.0F, false), kRenderSize);
    CHECK_FALSE(viewport.dragging());
}

TEST_CASE("shift turns the left drag into a pan") {
    Viewport orbiting;
    Viewport panning;

    CameraInput input = drag_left(25.0F, 0.0F, true);
    orbiting.update(input, kRenderSize);

    input.shift = true;
    panning.update(input, kRenderSize);

    // Orbiting swings the camera around a fixed target; panning carries the
    // target with it. That is the difference worth asserting.
    CHECK(glm::length(orbiting.camera().target()) == doctest::Approx(0.0F));
    CHECK(glm::length(panning.camera().target()) > 0.0F);
    CHECK(orbiting.camera().yaw() != doctest::Approx(panning.camera().yaw()));
}

TEST_CASE("the middle button always pans") {
    Viewport viewport;

    CameraInput input;
    input.delta = Vec2{25.0F, 0.0F};
    input.middle = true;
    input.over_world = true;

    viewport.update(input, kRenderSize);

    CHECK(viewport.dragging());
    CHECK(glm::length(viewport.camera().target()) > 0.0F);
}

TEST_CASE("the wheel is judged by where the pointer is now") {
    Viewport over_panel;
    Viewport over_world;

    CameraInput input;
    input.wheel = 3.0F;

    input.over_world = false;
    over_panel.update(input, kRenderSize);

    input.over_world = true;
    over_world.update(input, kRenderSize);

    CHECK(over_panel.camera().distance() == doctest::Approx(Viewport{}.camera().distance()));
    CHECK(over_world.camera().distance() < over_panel.camera().distance());
}

TEST_CASE("a drag does not carry the wheel with it over a panel") {
    Viewport viewport;

    // Dragging keeps control, but scrolling while over a panel belongs to the
    // panel, so the two are decided separately.
    viewport.update(drag_left(5.0F, 0.0F, true), kRenderSize);
    const float distance = viewport.camera().distance();

    CameraInput input = drag_left(5.0F, 0.0F, false);
    input.wheel = 3.0F;
    viewport.update(input, kRenderSize);

    CHECK(viewport.dragging());
    CHECK(viewport.camera().distance() == doctest::Approx(distance));
}
