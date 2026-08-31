#pragma once

#include <kinetiqra/geom/EditMesh.hpp>

#include <cstdint>
#include <vector>

namespace kinetiqra::geom {

// A run of triangles painted by one material.
//
// The indices of a baked mesh are ordered so that every material owns one
// unbroken stretch of them, which is what lets a mesh with several materials be
// drawn as several calls into a single buffer rather than as several buffers.
struct Section {
    std::uint32_t material{0};
    std::size_t first_index{0};
    std::size_t index_count{0};
};

// An EditMesh flattened into what a GPU can draw.
//
// Vertices are interleaved as position, normal, uv, tangent, which is twelve
// floats each.
struct BakedMesh {
    // position, normal, uv, tangent.
    static constexpr std::size_t kStaticFloatsPerVertex = 12;

    // The same, plus four joint indices and four weights.
    static constexpr std::size_t kSkinnedFloatsPerVertex = 20;

    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;

    // One per material used, in the order the materials are numbered. A mesh
    // that names no material gets a single section covering everything, so a
    // caller can always just walk this list.
    std::vector<Section> sections;

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
//
// Faces are visited grouped by material rather than in slot order, so that the
// indices of each material come out in one run. That is the only thing about
// the output that depends on which material a face carries; the vertices
// themselves are shared across materials wherever their attributes agree.
[[nodiscard]] BakedMesh bake(const EditMesh& mesh);

}  // namespace kinetiqra::geom
