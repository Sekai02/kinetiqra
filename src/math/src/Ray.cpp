#include <kinetiqra/math/Ray.hpp>

#include <glm/geometric.hpp>

#include <cmath>

namespace kinetiqra::math {

namespace {

// Below this the ray is parallel to the plane of the triangle, and the division
// that follows would be meaningless rather than merely imprecise.
constexpr float kParallelEpsilon = 1e-8F;

}  // namespace

bool intersect_triangle(const Ray& ray, Vec3 a, Vec3 b, Vec3 c, float& distance) {
    // Moller-Trumbore: solve for the barycentric coordinates and the distance at
    // once, without building the plane first.
    const Vec3 edge_one = b - a;
    const Vec3 edge_two = c - a;

    const Vec3 across = glm::cross(ray.direction, edge_two);
    const float determinant = glm::dot(edge_one, across);

    if (std::abs(determinant) < kParallelEpsilon) {
        return false;
    }

    const float inverse = 1.0F / determinant;
    const Vec3 to_origin = ray.origin - a;

    const float u = glm::dot(to_origin, across) * inverse;
    if (u < 0.0F || u > 1.0F) {
        return false;
    }

    const Vec3 along = glm::cross(to_origin, edge_one);
    const float v = glm::dot(ray.direction, along) * inverse;
    if (v < 0.0F || u + v > 1.0F) {
        return false;
    }

    const float hit = glm::dot(edge_two, along) * inverse;
    if (hit < 0.0F) {
        return false;
    }

    distance = hit;
    return true;
}

}  // namespace kinetiqra::math
