#include <kinetiqra/geom/Tangents.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <vector>

namespace kinetiqra::geom {

namespace {

// Below this the two UV edges of a triangle are parallel, so the texture has no
// area there and no direction can be recovered from it.
constexpr float kDegenerateUv = 1e-12F;

}  // namespace

void compute_tangents(EditMesh& mesh) {
    const auto* positions = mesh.attributes().find<math::Vec3>(kPosition, Domain::Vertex);
    const auto* normals = mesh.attributes().find<math::Vec3>(kNormal, Domain::Corner);
    const auto* uvs = mesh.attributes().find<math::Vec2>(kUv, Domain::Corner);

    if (positions == nullptr || normals == nullptr || uvs == nullptr) {
        return;
    }

    auto* tangents = mesh.attributes().add<math::Vec4>(kTangent, Domain::Corner,
                                                       math::Vec4{1.0F, 0.0F, 0.0F, 1.0F});
    if (tangents == nullptr) {
        return;
    }

    // Accumulated per vertex rather than per corner, which is what makes the
    // frame turn smoothly across a surface instead of faceting at every
    // triangle.
    //
    // It also matters for a reason that is not about looks. The bake merges
    // corners whose attributes agree bit for bit, and a per-triangle tangent
    // differs slightly between neighbours, so almost every corner would become
    // its own vertex. On a model of four thousand vertices that came out as
    // twenty two thousand.
    const std::size_t vertex_slots = positions->size();
    std::vector<math::Vec3> along_u(vertex_slots, math::Vec3{0.0F, 0.0F, 0.0F});
    std::vector<math::Vec3> along_v(vertex_slots, math::Vec3{0.0F, 0.0F, 0.0F});

    const auto position_of = [&](CornerId corner) {
        return mesh.position(mesh.corner_vertex(corner));
    };

    const auto uv_of = [&](CornerId corner) {
        return corner.index < uvs->size() ? (*uvs)[corner.index] : math::Vec2{0.0F, 0.0F};
    };

    for (const FaceId face : mesh.faces()) {
        const std::vector<CornerId>* corners = mesh.face_corners(face);
        if (corners == nullptr || corners->size() < 3) {
            continue;
        }

        // Fanned the same way the bake fans it, so the frame matches the
        // triangles that are actually drawn.
        for (std::size_t i = 2; i < corners->size(); ++i) {
            const CornerId a = (*corners)[0];
            const CornerId b = (*corners)[i - 1];
            const CornerId c = (*corners)[i];

            const math::Vec3 edge_one = position_of(b) - position_of(a);
            const math::Vec3 edge_two = position_of(c) - position_of(a);

            const math::Vec2 uv_one = uv_of(b) - uv_of(a);
            const math::Vec2 uv_two = uv_of(c) - uv_of(a);

            const float determinant = (uv_one.x * uv_two.y) - (uv_two.x * uv_one.y);
            if (std::abs(determinant) < kDegenerateUv) {
                continue;
            }

            const float inverse = 1.0F / determinant;

            // Where U runs, and where V runs, expressed in the space the
            // positions live in.
            const math::Vec3 u = ((edge_one * uv_two.y) - (edge_two * uv_one.y)) * inverse;
            const math::Vec3 v = ((edge_two * uv_one.x) - (edge_one * uv_two.x)) * inverse;

            for (const CornerId corner : {a, b, c}) {
                const VertexId vertex = mesh.corner_vertex(corner);
                if (vertex.index < along_u.size()) {
                    along_u[vertex.index] += u;
                    along_v[vertex.index] += v;
                }
            }
        }
    }

    // Written back onto the corners, because that is where a tangent lives: two
    // corners of one vertex across a hard edge have different normals, and the
    // frame each of them needs is square to its own.
    for (const FaceId face : mesh.faces()) {
        const std::vector<CornerId>* corners = mesh.face_corners(face);
        if (corners == nullptr) {
            continue;
        }

        for (const CornerId id : *corners) {
            const std::size_t corner = id.index;
            const std::size_t vertex = mesh.corner_vertex(id).index;

            if (corner >= tangents->size() || vertex >= along_u.size()) {
                continue;
            }

            const math::Vec3 accumulated = along_u[vertex];
            if (glm::length(accumulated) <= 0.0F) {
                continue;
            }

            const math::Vec3 normal =
                corner < normals->size() ? (*normals)[corner] : math::Vec3{0.0F, 1.0F, 0.0F};

            // Gram-Schmidt: the part of the tangent that lies in the surface.
            // The accumulated direction is only approximately perpendicular to
            // the normal, and the shader needs a frame that is square.
            const math::Vec3 tangent = accumulated - (normal * glm::dot(normal, accumulated));
            if (glm::length(tangent) <= 0.0F) {
                continue;
            }

            const math::Vec3 unit = glm::normalize(tangent);

            // Which way the third axis goes. A mirrored UV island has its V
            // running backwards, and without this it would be lit as though the
            // light came from the other side.
            const float handedness =
                glm::dot(glm::cross(normal, unit), along_v[vertex]) < 0.0F ? -1.0F : 1.0F;

            (*tangents)[corner] = math::Vec4{unit, handedness};
        }
    }
}

}  // namespace kinetiqra::geom
