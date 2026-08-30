#pragma once

#include <kinetiqra/math/Types.hpp>

namespace kinetiqra::math {

// A half line: where it starts and which way it goes.
//
// The direction is expected to be normalised, so the distance returned by an
// intersection is a length in world units rather than a multiple of whatever
// the direction happened to be scaled by.
struct Ray {
    Vec3 origin{0.0F, 0.0F, 0.0F};
    Vec3 direction{0.0F, 0.0F, -1.0F};

    [[nodiscard]] Vec3 at(float distance) const { return origin + direction * distance; }
};

// Where the ray meets the triangle, if it does.
//
// Returns false for a miss, for a triangle edge on to the ray, and for one
// behind the origin, which is what stops a click selecting something that is
// behind the camera.
//
// Both faces are hit. A modelling tool has to let you pick the inside of a
// surface, and the winding of an imported mesh is not something the user chose.
[[nodiscard]] bool intersect_triangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& distance);

}  // namespace kinetiqra::math
