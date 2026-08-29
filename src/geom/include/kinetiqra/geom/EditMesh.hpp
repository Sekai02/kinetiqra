#pragma once

#include <kinetiqra/core/Arena.hpp>
#include <kinetiqra/geom/AttributeSet.hpp>
#include <kinetiqra/math/Types.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace kinetiqra::geom {

namespace tags {
struct Vertex {};

struct Corner {};

struct Face {};
}  // namespace tags

using VertexId = core::Handle<tags::Vertex>;
using CornerId = core::Handle<tags::Corner>;
using FaceId = core::Handle<tags::Face>;

// The names the renderer and the importers agree on. They are ordinary channels
// like any other; nothing in the mesh treats them specially.
inline constexpr const char* kPosition = "position";  // Vec3, Vertex
inline constexpr const char* kNormal = "normal";      // Vec3, Corner
inline constexpr const char* kUv = "uv";              // Vec2, Corner

// Skinning, and note the domain: these are on the vertex, not the corner.
//
// A normal belongs to a corner because two faces meeting at a vertex may
// disagree about which way the surface points. How much a joint owns a vertex
// is not like that: it is a property of the point in space, and every corner
// sitting on it has to agree, or the mesh would tear when the joint moves.
//
// Four joints per vertex, which is what glTF's JOINTS_0 and WEIGHTS_0 carry and
// what the vertex shader blends.
inline constexpr const char* kJoints = "joints";    // Vec4 of indices, Vertex
inline constexpr const char* kWeights = "weights";  // Vec4, Vertex

// A mesh in the form the editor works on, as opposed to the form the GPU wants.
//
// Elements are addressed by generation-checked handles rather than by index or
// pointer, and attributes live on whichever domain they belong to, with UVs and
// normals on corners so that a seam or a hard edge can exist at all. Splitting
// vertices for the GPU is the bake's job, and this mesh never does it.
class EditMesh {
public:
    EditMesh();

    VertexId add_vertex(math::Vec3 position);

    // Adds a face over the given vertices, creating one corner per vertex. The
    // corners are returned so that per-corner attributes can be written without
    // looking them up again, which is the common case when building geometry.
    FaceId add_face(const std::vector<VertexId>& vertices,
                    std::vector<CornerId>* created_corners = nullptr);

    // Removes the vertex. Faces still referring to it are left alone and become
    // invalid, which `validate` reports; repairing them is a decision for the
    // caller rather than something to do silently here.
    bool remove_vertex(VertexId id);

    [[nodiscard]] bool contains(VertexId id) const { return vertices_.contains(id); }

    [[nodiscard]] bool contains(CornerId id) const { return corners_.contains(id); }

    [[nodiscard]] bool contains(FaceId id) const { return faces_.contains(id); }

    [[nodiscard]] VertexId corner_vertex(CornerId id) const;
    [[nodiscard]] const std::vector<CornerId>* face_corners(FaceId id) const;

    [[nodiscard]] std::size_t vertex_count() const { return vertices_.size(); }

    [[nodiscard]] std::size_t corner_count() const { return corners_.size(); }

    [[nodiscard]] std::size_t face_count() const { return faces_.size(); }

    // Every live face, in slot order. Iteration is over the dense storage, so
    // removed slots are skipped rather than compacted away.
    [[nodiscard]] std::vector<FaceId> faces() const;

    [[nodiscard]] AttributeSet& attributes() { return attributes_; }

    [[nodiscard]] const AttributeSet& attributes() const { return attributes_; }

    // Convenience for the three channels every mesh has.
    [[nodiscard]] math::Vec3 position(VertexId id) const;
    void set_position(VertexId id, math::Vec3 value);
    void set_normal(CornerId id, math::Vec3 value);
    void set_uv(CornerId id, math::Vec2 value);

    // Creates the joints and weights channels if they are not there yet, which
    // is what makes a mesh skinned as far as the bake is concerned.
    void set_skinning(VertexId id, math::Vec4 joints, math::Vec4 weights);

    [[nodiscard]] bool skinned() const;

    // Reports the first structural problem found, or an empty string. Used by
    // the tests and worth calling after an importer has filled a mesh in.
    [[nodiscard]] std::string validate() const;

private:
    struct Empty {};

    struct Face {
        std::vector<CornerId> corners;
    };

    core::Arena<Empty, tags::Vertex> vertices_;
    core::Arena<VertexId, tags::Corner> corners_;
    core::Arena<Face, tags::Face> faces_;

    AttributeSet attributes_;
};

}  // namespace kinetiqra::geom
