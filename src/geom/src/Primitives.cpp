#include <kinetiqra/geom/Primitives.hpp>

#include <array>

namespace kinetiqra::geom {

namespace {

// Right-handed, Y-up: +X right, +Y up, +Z towards the viewer. Corners are wound
// counter-clockwise seen from outside, so the front face is the visible one.
constexpr std::array<std::array<int, 4>, 6> kFaceVertices = {{
    {{1, 5, 7, 3}},  // +X
    {{4, 0, 2, 6}},  // -X
    {{2, 3, 7, 6}},  // +Y
    {{4, 5, 1, 0}},  // -Y
    {{5, 4, 6, 7}},  // +Z
    {{0, 1, 3, 2}},  // -Z
}};

constexpr std::array<math::Vec3, 6> kFaceNormals = {{
    {1.0F, 0.0F, 0.0F},
    {-1.0F, 0.0F, 0.0F},
    {0.0F, 1.0F, 0.0F},
    {0.0F, -1.0F, 0.0F},
    {0.0F, 0.0F, 1.0F},
    {0.0F, 0.0F, -1.0F},
}};

// Each face gets the whole unit square, so every face is its own UV island and
// the box has a seam along every edge.
constexpr std::array<math::Vec2, 4> kFaceUvs = {{
    {0.0F, 0.0F},
    {1.0F, 0.0F},
    {1.0F, 1.0F},
    {0.0F, 1.0F},
}};

}  // namespace

EditMesh make_box(math::Vec3 size) {
    EditMesh mesh;

    const math::Vec3 half = size * 0.5F;

    std::array<VertexId, 8> vertices{};
    for (int i = 0; i < 8; ++i) {
        // The bit pattern of the index picks the sign on each axis, so vertex 0
        // is the most negative corner and vertex 7 the most positive.
        const math::Vec3 position{
            ((i & 1) != 0) ? half.x : -half.x,
            ((i & 2) != 0) ? half.y : -half.y,
            ((i & 4) != 0) ? half.z : -half.z,
        };
        vertices[static_cast<std::size_t>(i)] = mesh.add_vertex(position);
    }

    for (std::size_t face = 0; face < kFaceVertices.size(); ++face) {
        std::vector<VertexId> face_vertices;
        face_vertices.reserve(4);
        for (const int index : kFaceVertices[face]) {
            face_vertices.push_back(vertices[static_cast<std::size_t>(index)]);
        }

        std::vector<CornerId> corners;
        mesh.add_face(face_vertices, &corners);

        for (std::size_t corner = 0; corner < corners.size(); ++corner) {
            mesh.set_normal(corners[corner], kFaceNormals[face]);
            mesh.set_uv(corners[corner], kFaceUvs[corner]);
        }
    }

    return mesh;
}

}  // namespace kinetiqra::geom
