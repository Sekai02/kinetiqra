#pragma once

#include <kinetiqra/math/Types.hpp>
#include <kinetiqra/scene/Pose.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace kinetiqra::anim {

enum class Interpolation {
    Step,
    Linear,
    CubicSpline,
};

// Which part of a node's transform a channel drives. They are addressed
// separately because that is how glTF stores them and how they are authored:
// an animator moves a joint's rotation without disturbing its scale.
enum class Path {
    Translation,
    Rotation,
    Scale,
};

// Keyframes and how to read between them.
//
// Values are kept as Vec4 so one container serves translations, scales and
// rotations alike; a translation uses xyz and a rotation all four. A cubic
// sampler stores three values per key, the in-tangent, the value and the
// out-tangent, in that order, which is the layout glTF uses.
struct Sampler {
    std::vector<float> times;
    std::vector<math::Vec4> values;
    Interpolation interpolation{Interpolation::Linear};

    [[nodiscard]] std::size_t key_count() const { return times.size(); }

    [[nodiscard]] std::size_t values_per_key() const {
        return interpolation == Interpolation::CubicSpline ? 3 : 1;
    }

    [[nodiscard]] bool valid() const {
        return !times.empty() && values.size() == times.size() * values_per_key();
    }
};

struct Channel {
    scene::NodeId target;
    Path path{Path::Translation};
    std::size_t sampler{0};
};

struct Clip {
    std::string name;
    std::vector<Sampler> samplers;
    std::vector<Channel> channels;
    float duration{0.0F};
};

// The value of a sampler at a moment.
//
// Before the first key and after the last, the nearest key is held rather than
// extrapolated, which is what the specification asks for and what stops a clip
// flinging a limb off at its edges.
//
// Rotations are interpolated by slerp, taking the short way round when
// consecutive quaternions sit in opposite hemispheres. Interpolating the four
// components straight would slow the turn through the middle of every arc,
// which reads as a stutter rather than as a rotation.
[[nodiscard]] math::Vec4 sample(const Sampler& sampler, Path path, float time);

// Fills the pose with the nodes this clip drives, leaving the rest to the
// scene. `rest` supplies the components a clip does not touch, so a channel
// that only rotates a joint keeps that joint's authored position.
void evaluate(const Clip& clip, float time, const scene::Scene& rest, scene::Pose& pose);

}  // namespace kinetiqra::anim
