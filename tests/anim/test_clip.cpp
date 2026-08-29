#include <kinetiqra/anim/Clip.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

using kinetiqra::anim::Channel;
using kinetiqra::anim::Clip;
using kinetiqra::anim::evaluate;
using kinetiqra::anim::Interpolation;
using kinetiqra::anim::Path;
using kinetiqra::anim::sample;
using kinetiqra::anim::Sampler;
using kinetiqra::math::Quat;
using kinetiqra::math::Vec3;
using kinetiqra::math::Vec4;
using kinetiqra::scene::NodeId;
using kinetiqra::scene::Pose;
using kinetiqra::scene::Scene;

namespace {

Sampler linear_positions() {
    Sampler sampler;
    sampler.interpolation = Interpolation::Linear;
    sampler.times = {0.0F, 1.0F, 2.0F};
    sampler.values = {
        Vec4{0.0F, 0.0F, 0.0F, 0.0F},
        Vec4{10.0F, 0.0F, 0.0F, 0.0F},
        Vec4{10.0F, 20.0F, 0.0F, 0.0F},
    };
    return sampler;
}

Quat quaternion_of(Vec4 value) {
    return Quat{value.w, value.x, value.y, value.z};
}

}  // namespace

TEST_CASE("sampling lands exactly on a keyframe at its own time") {
    const Sampler sampler = linear_positions();

    CHECK(sample(sampler, Path::Translation, 0.0F).x == doctest::Approx(0.0F));
    CHECK(sample(sampler, Path::Translation, 1.0F).x == doctest::Approx(10.0F));
    CHECK(sample(sampler, Path::Translation, 2.0F).y == doctest::Approx(20.0F));
}

TEST_CASE("linear interpolation halfway is the midpoint") {
    const Sampler sampler = linear_positions();

    CHECK(sample(sampler, Path::Translation, 0.5F).x == doctest::Approx(5.0F));
    CHECK(sample(sampler, Path::Translation, 1.5F).y == doctest::Approx(10.0F));
    CHECK(sample(sampler, Path::Translation, 1.5F).x == doctest::Approx(10.0F));
}

TEST_CASE("time outside the clip holds the nearest key rather than extrapolating") {
    const Sampler sampler = linear_positions();

    // Extrapolating here would fling the target off in both directions, which
    // is the classic way an animation explodes at its edges.
    CHECK(sample(sampler, Path::Translation, -100.0F).x == doctest::Approx(0.0F));
    CHECK(sample(sampler, Path::Translation, 100.0F).x == doctest::Approx(10.0F));
    CHECK(sample(sampler, Path::Translation, 100.0F).y == doctest::Approx(20.0F));
}

TEST_CASE("step holds the earlier value until the next key") {
    Sampler sampler = linear_positions();
    sampler.interpolation = Interpolation::Step;

    CHECK(sample(sampler, Path::Translation, 0.0F).x == doctest::Approx(0.0F));
    CHECK(sample(sampler, Path::Translation, 0.99F).x == doctest::Approx(0.0F));
    CHECK(sample(sampler, Path::Translation, 1.0F).x == doctest::Approx(10.0F));
    CHECK(sample(sampler, Path::Translation, 1.99F).x == doctest::Approx(10.0F));
}

TEST_CASE("a rotation interpolates along the arc, not through the chord") {
    Sampler sampler;
    sampler.interpolation = Interpolation::Linear;
    sampler.times = {0.0F, 1.0F};

    const Quat start = glm::angleAxis(0.0F, Vec3{0.0F, 1.0F, 0.0F});
    const Quat end = glm::angleAxis(glm::radians(90.0F), Vec3{0.0F, 1.0F, 0.0F});
    sampler.values = {Vec4{start.x, start.y, start.z, start.w}, Vec4{end.x, end.y, end.z, end.w}};

    const Quat middle = quaternion_of(sample(sampler, Path::Rotation, 0.5F));

    // Halfway along a quarter turn is an eighth of a turn, and the result is
    // still a unit quaternion. Interpolating the components straight would
    // shorten it and slow the turn through the middle.
    CHECK(glm::length(middle) == doctest::Approx(1.0F));

    const Vec3 turned = middle * Vec3{1.0F, 0.0F, 0.0F};
    CHECK(turned.x == doctest::Approx(std::cos(glm::radians(45.0F))).epsilon(1e-4));
    CHECK(turned.z == doctest::Approx(-std::sin(glm::radians(45.0F))).epsilon(1e-4));
}

TEST_CASE("a rotation takes the short way round between hemispheres") {
    Sampler sampler;
    sampler.interpolation = Interpolation::Linear;
    sampler.times = {0.0F, 1.0F};

    const Quat start = glm::angleAxis(glm::radians(10.0F), Vec3{0.0F, 1.0F, 0.0F});
    Quat end = glm::angleAxis(glm::radians(30.0F), Vec3{0.0F, 1.0F, 0.0F});

    // The same rotation with every component negated, which is what an exporter
    // may well write. Taken at face value it would spin nearly all the way
    // round instead of twenty degrees.
    end = Quat{-end.w, -end.x, -end.y, -end.z};

    sampler.values = {Vec4{start.x, start.y, start.z, start.w}, Vec4{end.x, end.y, end.z, end.w}};

    const Quat middle = quaternion_of(sample(sampler, Path::Rotation, 0.5F));
    const Vec3 turned = middle * Vec3{1.0F, 0.0F, 0.0F};
    const float angle = std::atan2(-turned.z, turned.x);

    CHECK(glm::degrees(angle) == doctest::Approx(20.0F).epsilon(1e-3));
}

TEST_CASE("a cubic segment follows its tangents") {
    Sampler sampler;
    sampler.interpolation = Interpolation::CubicSpline;
    sampler.times = {0.0F, 1.0F};

    // Three values per key: in-tangent, value, out-tangent. Flat tangents make
    // the segment ease in and out, so halfway is the midpoint of the values
    // even though the motion around it is not linear.
    sampler.values = {
        Vec4{0.0F, 0.0F, 0.0F, 0.0F}, Vec4{0.0F, 0.0F, 0.0F, 0.0F},
        Vec4{0.0F, 0.0F, 0.0F, 0.0F},  // key 0
        Vec4{0.0F, 0.0F, 0.0F, 0.0F}, Vec4{10.0F, 0.0F, 0.0F, 0.0F},
        Vec4{0.0F, 0.0F, 0.0F, 0.0F},  // key 1
    };

    CHECK(sample(sampler, Path::Translation, 0.0F).x == doctest::Approx(0.0F));
    CHECK(sample(sampler, Path::Translation, 1.0F).x == doctest::Approx(10.0F));
    CHECK(sample(sampler, Path::Translation, 0.5F).x == doctest::Approx(5.0F));

    // Flat tangents mean it leaves the first key slowly, so a quarter of the
    // way through it has covered less than a quarter of the distance.
    CHECK(sample(sampler, Path::Translation, 0.25F).x < 2.5F);
    CHECK(sample(sampler, Path::Translation, 0.75F).x > 7.5F);
}

TEST_CASE("a cubic segment with matching tangents is a straight line") {
    Sampler sampler;
    sampler.interpolation = Interpolation::CubicSpline;
    sampler.times = {0.0F, 1.0F};

    // Tangents equal to the slope of the segment turn the Hermite curve into
    // the straight line between the two values, which is a known result and a
    // good check that the basis and the tangent scaling are right.
    const Vec4 slope{10.0F, 0.0F, 0.0F, 0.0F};
    sampler.values = {
        slope, Vec4{0.0F, 0.0F, 0.0F, 0.0F}, slope, slope, Vec4{10.0F, 0.0F, 0.0F, 0.0F}, slope,
    };

    CHECK(sample(sampler, Path::Translation, 0.25F).x == doctest::Approx(2.5F));
    CHECK(sample(sampler, Path::Translation, 0.5F).x == doctest::Approx(5.0F));
    CHECK(sample(sampler, Path::Translation, 0.75F).x == doctest::Approx(7.5F));
}

TEST_CASE("a sampler with a single key returns it at any time") {
    Sampler sampler;
    sampler.times = {3.0F};
    sampler.values = {Vec4{7.0F, 0.0F, 0.0F, 0.0F}};

    CHECK(sample(sampler, Path::Translation, 0.0F).x == doctest::Approx(7.0F));
    CHECK(sample(sampler, Path::Translation, 3.0F).x == doctest::Approx(7.0F));
    CHECK(sample(sampler, Path::Translation, 99.0F).x == doctest::Approx(7.0F));
}

TEST_CASE("an empty or malformed sampler yields a harmless value") {
    const Sampler empty;
    CHECK_FALSE(empty.valid());

    // Never read out of bounds, and never collapse a model by returning a zero
    // scale for a channel that has nothing to say.
    CHECK(sample(empty, Path::Scale, 1.0F).x == doctest::Approx(1.0F));
    CHECK(sample(empty, Path::Translation, 1.0F).x == doctest::Approx(0.0F));

    Sampler mismatched;
    mismatched.times = {0.0F, 1.0F};
    mismatched.values = {Vec4{1.0F, 0.0F, 0.0F, 0.0F}};
    CHECK_FALSE(mismatched.valid());
    CHECK(sample(mismatched, Path::Translation, 0.5F).x == doctest::Approx(0.0F));
}

TEST_CASE("evaluating fills only the nodes the clip drives") {
    Scene scene;
    const NodeId driven = scene.add_node("driven");
    const NodeId untouched = scene.add_node("untouched");
    scene.node(untouched)->transform.translation = Vec3{5.0F, 0.0F, 0.0F};

    Clip clip;
    clip.samplers.push_back(linear_positions());
    clip.channels.push_back(Channel{driven, Path::Translation, 0});
    clip.duration = 2.0F;

    Pose pose;
    evaluate(clip, 1.0F, scene, pose);

    CHECK(pose.size() == 1);
    REQUIRE(pose.find(driven) != nullptr);
    CHECK(pose.find(driven)->translation.x == doctest::Approx(10.0F));
    CHECK(pose.find(untouched) == nullptr);
}

TEST_CASE("a channel leaves the components it does not drive alone") {
    Scene scene;
    const NodeId node = scene.add_node("node");
    scene.node(node)->transform.translation = Vec3{1.0F, 2.0F, 3.0F};
    scene.node(node)->transform.scale = Vec3{4.0F, 4.0F, 4.0F};

    Sampler rotations;
    rotations.times = {0.0F};
    const Quat turn = glm::angleAxis(glm::radians(90.0F), Vec3{0.0F, 1.0F, 0.0F});
    rotations.values = {Vec4{turn.x, turn.y, turn.z, turn.w}};

    Clip clip;
    clip.samplers.push_back(rotations);
    clip.channels.push_back(Channel{node, Path::Rotation, 0});

    Pose pose;
    evaluate(clip, 0.0F, scene, pose);

    const auto* posed = pose.find(node);
    REQUIRE(posed != nullptr);

    // Only the rotation was driven, so the authored position and scale survive.
    CHECK(posed->translation.x == doctest::Approx(1.0F));
    CHECK(posed->scale.x == doctest::Approx(4.0F));
    CHECK(glm::length(posed->rotation) == doctest::Approx(1.0F));
}

TEST_CASE("a channel pointing at a node that is gone is skipped") {
    Scene scene;
    const NodeId node = scene.add_node("node");

    Clip clip;
    clip.samplers.push_back(linear_positions());
    clip.channels.push_back(Channel{node, Path::Translation, 0});
    clip.channels.push_back(Channel{NodeId{}, Path::Translation, 0});
    clip.channels.push_back(Channel{node, Path::Translation, 99});

    Pose pose;
    evaluate(clip, 0.5F, scene, pose);

    // The valid channel is applied and the two broken ones are ignored rather
    // than read out of bounds.
    CHECK(pose.size() == 1);
}

TEST_CASE("evaluating twice at the same time gives the same pose") {
    Scene scene;
    const NodeId node = scene.add_node("node");

    Clip clip;
    clip.samplers.push_back(linear_positions());
    clip.channels.push_back(Channel{node, Path::Translation, 0});

    Pose first;
    Pose second;
    evaluate(clip, 0.75F, scene, first);
    evaluate(clip, 0.75F, scene, second);

    CHECK(first.find(node)->translation.x == doctest::Approx(second.find(node)->translation.x));
}
