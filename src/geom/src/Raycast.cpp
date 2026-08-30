#include <kinetiqra/geom/Raycast.hpp>

#include <glm/geometric.hpp>

#include <limits>

namespace kinetiqra::geom {

std::optional<Hit> raycast(const EditMesh& mesh, const math::Ray& ray,
                           const std::vector<math::Vec3>* positions) {
    const auto position_of = [&](VertexId vertex) {
        if (positions != nullptr && vertex.index < positions->size()) {
            return (*positions)[vertex.index];
        }
        return mesh.position(vertex);
    };

    std::optional<Hit> nearest;
    float best = std::numeric_limits<float>::max();

    for (const FaceId face : mesh.faces()) {
        const std::vector<CornerId>* corners = mesh.face_corners(face);
        if (corners == nullptr || corners->size() < 3) {
            continue;
        }

        const math::Vec3 first = position_of(mesh.corner_vertex((*corners)[0]));

        for (std::size_t i = 2; i < corners->size(); ++i) {
            const math::Vec3 previous = position_of(mesh.corner_vertex((*corners)[i - 1]));
            const math::Vec3 current = position_of(mesh.corner_vertex((*corners)[i]));

            float distance = 0.0F;
            if (!math::intersect_triangle(ray, first, previous, current, distance)) {
                continue;
            }

            if (distance >= best) {
                continue;
            }

            best = distance;

            Hit hit;
            hit.face = face;
            hit.point = ray.at(distance);
            hit.distance = distance;

            // The nearest corner of the whole face, not of the triangle the fan
            // happened to land on, or a click near a corner of a quad could
            // pick a vertex on the other side of it.
            float closest = std::numeric_limits<float>::max();
            for (const CornerId corner : *corners) {
                const VertexId vertex = mesh.corner_vertex(corner);
                const float to_vertex = glm::distance(position_of(vertex), hit.point);
                if (to_vertex < closest) {
                    closest = to_vertex;
                    hit.vertex = vertex;
                }
            }

            nearest = hit;
        }
    }

    return nearest;
}

}  // namespace kinetiqra::geom
