#include <kinetiqra/geom/Extrude.hpp>

#include <glm/geometric.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace kinetiqra::geom {

namespace {

// An edge, named by its two vertices with the smaller first so that the same
// edge seen from either of its faces is the same key.
std::uint64_t edge_key(VertexId a, VertexId b) {
    const std::uint32_t low = a.index < b.index ? a.index : b.index;
    const std::uint32_t high = a.index < b.index ? b.index : a.index;
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

// Newell's method, which gives the normal of a polygon of any number of sides
// and does not care whether it is perfectly flat.
math::Vec3 face_normal(const EditMesh& mesh, const std::vector<CornerId>& corners) {
    math::Vec3 normal{0.0F, 0.0F, 0.0F};

    for (std::size_t i = 0; i < corners.size(); ++i) {
        const math::Vec3 current = mesh.position(mesh.corner_vertex(corners[i]));
        const math::Vec3 next =
            mesh.position(mesh.corner_vertex(corners[(i + 1) % corners.size()]));

        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }

    return glm::length(normal) > 0.0F ? glm::normalize(normal) : math::Vec3{0.0F, 1.0F, 0.0F};
}

math::Vec3 face_centre(const EditMesh& mesh, const std::vector<CornerId>& corners) {
    math::Vec3 total{0.0F, 0.0F, 0.0F};
    for (const CornerId corner : corners) {
        total += mesh.position(mesh.corner_vertex(corner));
    }
    return total / static_cast<float>(corners.size());
}

}  // namespace

ExtrudeResult extrude(EditMesh& mesh, const std::vector<FaceId>& faces) {
    ExtrudeResult result;

    // The selection as it stood, read before anything is created, since adding
    // faces invalidates nothing but reading as we go would be harder to follow.
    std::vector<FaceId> selected;
    std::vector<std::vector<CornerId>> loops;

    for (const FaceId face : faces) {
        const std::vector<CornerId>* corners = mesh.face_corners(face);
        if (corners == nullptr || corners->size() < 3) {
            continue;
        }
        selected.push_back(face);
        loops.push_back(*corners);
    }

    if (selected.empty()) {
        return result;
    }

    // An edge used once by the selection is on its boundary; one used twice is
    // between two selected faces and is not a place a wall belongs.
    std::unordered_map<std::uint64_t, int> edge_uses;
    for (const std::vector<CornerId>& loop : loops) {
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const VertexId a = mesh.corner_vertex(loop[i]);
            const VertexId b = mesh.corner_vertex(loop[(i + 1) % loop.size()]);
            ++edge_uses[edge_key(a, b)];
        }
    }

    const auto* joints = mesh.attributes().find<math::Vec4>(kJoints, Domain::Vertex);
    const auto* weights = mesh.attributes().find<math::Vec4>(kWeights, Domain::Vertex);
    const bool skinned = joints != nullptr && weights != nullptr;

    // Every vertex the selection touches is duplicated once, however many of
    // the selected faces share it, or the patch would tear along its inside.
    std::unordered_map<std::uint32_t, VertexId> duplicates;

    const auto duplicate = [&](VertexId original) {
        if (const auto found = duplicates.find(original.index); found != duplicates.end()) {
            return found->second;
        }

        const VertexId copy = mesh.add_vertex(mesh.position(original));

        // The new vertex stands where the old one does, so it belongs to the
        // same joints. Without this an extrusion on a rigged mesh would leave
        // the new geometry behind when the skeleton moved.
        if (skinned && original.index < joints->size() && original.index < weights->size()) {
            mesh.set_skinning(copy, (*joints)[original.index], (*weights)[original.index]);
        }

        duplicates.emplace(original.index, copy);
        result.vertices.push_back(copy);
        return copy;
    };

    const auto* normals = mesh.attributes().find<math::Vec3>(kNormal, Domain::Corner);
    const auto* uvs = mesh.attributes().find<math::Vec2>(kUv, Domain::Corner);

    for (std::size_t index = 0; index < selected.size(); ++index) {
        const std::vector<CornerId>& loop = loops[index];

        const math::Vec3 normal = face_normal(mesh, loop);
        const math::Vec3 centre = face_centre(mesh, loop);

        // The cap: the same face, built from the duplicates, so it keeps the
        // winding and therefore the direction it faces.
        std::vector<VertexId> cap;
        cap.reserve(loop.size());
        for (const CornerId corner : loop) {
            cap.push_back(duplicate(mesh.corner_vertex(corner)));
        }

        std::vector<CornerId> cap_corners;
        result.caps.push_back(mesh.add_face(cap, &cap_corners));

        // Carried over rather than recomputed, so a hard edge or a UV island
        // survives being extruded.
        for (std::size_t corner = 0; corner < cap_corners.size(); ++corner) {
            const CornerId source = loop[corner];

            if (normals != nullptr && source.index < normals->size()) {
                mesh.set_normal(cap_corners[corner], (*normals)[source.index]);
            }
            if (uvs != nullptr && source.index < uvs->size()) {
                mesh.set_uv(cap_corners[corner], (*uvs)[source.index]);
            }
        }

        for (std::size_t i = 0; i < loop.size(); ++i) {
            const VertexId a = mesh.corner_vertex(loop[i]);
            const VertexId b = mesh.corner_vertex(loop[(i + 1) % loop.size()]);

            if (edge_uses[edge_key(a, b)] != 1) {
                continue;
            }

            const math::Vec3 from = mesh.position(a);
            const math::Vec3 to = mesh.position(b);

            // A wall raised along this edge would face this way. The cross
            // product is computed rather than measured because at this moment
            // the wall is flat against the surface and has no area to measure.
            math::Vec3 outward = glm::cross(to - from, normal);
            const bool away = glm::dot(outward, ((from + to) * 0.5F) - centre) > 0.0F;
            if (!away) {
                outward = -outward;
            }

            const VertexId top_a = duplicate(a);
            const VertexId top_b = duplicate(b);

            // Wound so that the wall faces outward once it has been moved.
            const std::vector<VertexId> wall = away ? std::vector<VertexId>{a, b, top_b, top_a}
                                                    : std::vector<VertexId>{b, a, top_a, top_b};

            std::vector<CornerId> wall_corners;
            result.walls.push_back(mesh.add_face(wall, &wall_corners));

            const math::Vec3 wall_normal = glm::length(outward) > 0.0F
                                               ? glm::normalize(outward)
                                               : math::Vec3{0.0F, 1.0F, 0.0F};

            for (const CornerId corner : wall_corners) {
                mesh.set_normal(corner, wall_normal);

                // Nothing sensible to say yet: unwrapping a new wall is a
                // decision, and inventing coordinates would be worse than
                // leaving them at the origin where they are obviously unset.
                mesh.set_uv(corner, math::Vec2{0.0F, 0.0F});
            }
        }
    }

    // Last, so that everything above read the faces as they were.
    for (const FaceId face : selected) {
        mesh.remove_face(face);
    }

    return result;
}

}  // namespace kinetiqra::geom
