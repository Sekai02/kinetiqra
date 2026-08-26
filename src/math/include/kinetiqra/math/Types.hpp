#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kinetiqra::math {

// The engine works in glTF's conventions throughout: right-handed, Y-up, and
// one unit is one metre. Nothing outside `io` converts between conventions, so
// that there is exactly one place where the conversion can be wrong.
//
// See docs/INVARIANTS.md.

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

inline constexpr Vec3 kWorldUp{0.0F, 1.0F, 0.0F};
inline constexpr Vec3 kWorldRight{1.0F, 0.0F, 0.0F};

// Towards the viewer, which is the positive Z of a right-handed system.
inline constexpr Vec3 kWorldForward{0.0F, 0.0F, 1.0F};

}  // namespace kinetiqra::math
