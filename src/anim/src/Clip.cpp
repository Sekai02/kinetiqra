#include <kinetiqra/anim/Clip.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace kinetiqra::anim {

namespace {

math::Quat as_quaternion(math::Vec4 value) {
    // glTF stores a quaternion as x, y, z, w; glm's constructor takes w first.
    return math::Quat{value.w, value.x, value.y, value.z};
}

math::Vec4 as_vector(math::Quat value) {
    return math::Vec4{value.x, value.y, value.z, value.w};
}

// The pair of keys surrounding a time, and how far between them it sits.
struct Span {
    std::size_t before{0};
    std::size_t after{0};
    float alpha{0.0F};
};

Span find_span(const std::vector<float>& times, float time) {
    Span span;

    // Held rather than extrapolated at both ends.
    if (time <= times.front()) {
        return span;
    }
    if (time >= times.back()) {
        span.before = times.size() - 1;
        span.after = span.before;
        return span;
    }

    const auto upper = std::upper_bound(times.begin(), times.end(), time);
    span.after = static_cast<std::size_t>(std::distance(times.begin(), upper));
    span.before = span.after - 1;

    const float start = times[span.before];
    const float end = times[span.after];
    const float length = end - start;

    // Two keys at the same time would divide by zero; the later one wins, which
    // is how a step in the middle of a clip is usually authored.
    span.alpha = length > 0.0F ? (time - start) / length : 1.0F;
    return span;
}

math::Vec4 interpolate_linear(math::Vec4 a, math::Vec4 b, float alpha, Path path) {
    if (path != Path::Rotation) {
        return a + (b - a) * alpha;
    }

    // glm::slerp already takes the short way and returns a unit quaternion.
    return as_vector(glm::normalize(glm::slerp(as_quaternion(a), as_quaternion(b), alpha)));
}

// The Hermite basis glTF specifies, with the tangents scaled by the length of
// the segment so that a clip resampled at a different rate keeps its shape.
math::Vec4 interpolate_cubic(math::Vec4 value_before, math::Vec4 out_tangent, math::Vec4 in_tangent,
                             math::Vec4 value_after, float alpha, float span_length, Path path) {
    const float t = alpha;
    const float t2 = t * t;
    const float t3 = t2 * t;

    const math::Vec4 result = (2.0F * t3 - 3.0F * t2 + 1.0F) * value_before +
                              span_length * (t3 - 2.0F * t2 + t) * out_tangent +
                              (-2.0F * t3 + 3.0F * t2) * value_after +
                              span_length * (t3 - t2) * in_tangent;

    if (path != Path::Rotation) {
        return result;
    }

    // A cubic through four quaternion components does not land on the unit
    // sphere, so the specification says to normalise the result.
    return as_vector(glm::normalize(as_quaternion(result)));
}

}  // namespace

math::Vec4 sample(const Sampler& sampler, Path path, float time) {
    if (!sampler.valid()) {
        return path == Path::Scale ? math::Vec4{1.0F, 1.0F, 1.0F, 1.0F}
                                   : math::Vec4{0.0F, 0.0F, 0.0F, 1.0F};
    }

    const Span span = find_span(sampler.times, time);
    const std::size_t stride = sampler.values_per_key();

    // With a cubic sampler the value of a key is the middle of its three.
    const std::size_t value_offset = sampler.interpolation == Interpolation::CubicSpline ? 1 : 0;

    const math::Vec4 before = sampler.values[span.before * stride + value_offset];

    if (span.before == span.after || sampler.interpolation == Interpolation::Step) {
        return before;
    }

    const math::Vec4 after = sampler.values[span.after * stride + value_offset];

    if (sampler.interpolation == Interpolation::Linear) {
        return interpolate_linear(before, after, span.alpha, path);
    }

    const math::Vec4 out_tangent = sampler.values[span.before * stride + 2];
    const math::Vec4 in_tangent = sampler.values[span.after * stride];
    const float length = sampler.times[span.after] - sampler.times[span.before];

    return interpolate_cubic(before, out_tangent, in_tangent, after, span.alpha, length, path);
}

void evaluate(const Clip& clip, float time, const scene::Scene& rest, scene::Pose& pose) {
    for (const Channel& channel : clip.channels) {
        if (channel.sampler >= clip.samplers.size() || !channel.target.valid()) {
            continue;
        }

        const scene::Node* node = rest.node(channel.target);
        if (node == nullptr) {
            continue;
        }

        // Seeded from what the node already holds, so a channel that only
        // rotates a joint leaves its position and scale as they were authored.
        const scene::Transform* existing = pose.find(channel.target);
        scene::Transform transform = existing != nullptr ? *existing : node->transform;

        const math::Vec4 value = sample(clip.samplers[channel.sampler], channel.path, time);

        switch (channel.path) {
            case Path::Translation:
                transform.translation = math::Vec3{value};
                break;
            case Path::Rotation:
                transform.rotation = as_quaternion(value);
                break;
            case Path::Scale:
                transform.scale = math::Vec3{value};
                break;
        }

        pose.set(channel.target, transform);
    }
}

}  // namespace kinetiqra::anim
