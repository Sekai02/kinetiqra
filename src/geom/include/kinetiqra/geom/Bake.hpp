#pragma once

#include <kinetiqra/geom/EditMesh.hpp>

#include <cstdint>
#include <vector>

namespace kinetiqra::geom {

// An EditMesh flattened into what a GPU can draw.
//
// Vertices are interleaved as position, normal, uv, which is eight floats each.
struct BakedMesh {
    // position, normal, uv.
    static constexpr std::size_t kStaticFloatsPerVertex = 8;

    // The same, plus four joint indices and four weights.
    static constexpr std::size_t kSkinnedFloatsPerVertex = 16;

    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;

    // Which of the two layouts the vertices are in. The renderer needs it to
    // describe the vertex array and to pick the shader.
    bool skinned{false};

    [[nodiscard]] std::size_t floats_per_vertex() const {
        return skinned ? kSkinnedFloatsPerVertex : kStaticFloatsPerVertex;
    }

    [[nodiscard]] std::size_t vertex_count() const { return vertices.size() / floats_per_vertex(); }

    [[nodiscard]] std::size_t triangle_count() const { return indices.size() / 3; }
};

// Flattens the mesh, splitting a vertex once per distinct combination of its
// corner attributes and merging the corners that agree.
//
// This is the one place where the editable representation turns into the GPU
// one. It goes in this direction only: the split exists to satisfy hardware
// that has a single attribute per vertex, and the editable mesh is left alone,
// still holding one vertex where the model has one vertex. See
// docs/INVARIANTS.md.
//
// Faces are triangulated as a fan, which is correct for the convex faces the
// editor produces today.
[[nodiscard]] BakedMesh bake(const EditMesh& mesh);

}  // namespace kinetiqra::geom
