#include <kinetiqra/scene/Raycast.hpp>

#include <glm/geometric.hpp>

#include <limits>

namespace kinetiqra::scene {

std::optional<SceneHit> raycast(const Scene& scene, const math::Ray& ray, const Pose* pose) {
    std::optional<SceneHit> nearest;
    float best = std::numeric_limits<float>::max();

    for (const NodeId id : scene.nodes_in_order()) {
        const Node* node = scene.node(id);
        if (node == nullptr || !node->mesh.valid()) {
            continue;
        }

        const geom::EditMesh* mesh = scene.mesh(node->mesh);
        if (mesh == nullptr) {
            continue;
        }

        // Everything is tested in world space, against where the vertices
        // actually are rather than where the mesh stores them. For a skinned
        // mesh those are not the same place: the joints carry the geometry, and
        // a mesh bound in centimetres sits a hundred times too large until they
        // have. Picking what is drawn is the whole point.
        const std::vector<math::Vec3> positions = scene.world_positions(id, pose);

        const std::optional<geom::Hit> hit = geom::raycast(*mesh, ray, &positions);
        if (!hit.has_value() || hit->distance >= best) {
            continue;
        }

        best = hit->distance;

        SceneHit found;
        found.node = id;
        found.face = hit->face;
        found.vertex = hit->vertex;
        found.point = hit->point;
        found.distance = hit->distance;
        nearest = found;
    }

    return nearest;
}

}  // namespace kinetiqra::scene
