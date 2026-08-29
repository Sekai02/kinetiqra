#pragma once

#include <kinetiqra/core/Arena.hpp>
#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/math/Types.hpp>

#include <string>
#include <vector>

namespace kinetiqra::scene {

namespace tags {
struct Node {};

struct Mesh {};

struct Skin {};
}  // namespace tags

using NodeId = core::Handle<tags::Node>;
using MeshId = core::Handle<tags::Mesh>;
using SkinId = core::Handle<tags::Skin>;

// The joints a mesh is bound to, and the matrices that undo the pose the mesh
// was modelled in.
//
// An inverse bind matrix takes a vertex from model space into the space of its
// joint as that joint stood when the mesh was bound. Multiplying it by the
// joint's current world transform leaves only the movement since binding, which
// is what deforms the mesh. In the bind pose the two cancel to the identity, so
// the model appears exactly as it was modelled.
struct Skin {
    std::vector<NodeId> joints;
    std::vector<math::Mat4> inverse_bind;
};

// A node's placement, kept as translation, rotation and scale rather than as a
// matrix.
//
// glTF stores it this way, animation channels target the three independently,
// and recovering them from a matrix is lossy: a matrix cannot say whether a
// rotation went the long way round, and shear has nowhere to go. Keeping them
// apart means `anim` can drive one without disturbing the others.
struct Transform {
    math::Vec3 translation{0.0F, 0.0F, 0.0F};
    math::Quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    math::Vec3 scale{1.0F, 1.0F, 1.0F};

    [[nodiscard]] math::Mat4 matrix() const;
};

struct Node {
    std::string name;
    Transform transform;
    NodeId parent;
    std::vector<NodeId> children;

    // Nodes without a mesh are common: glTF uses them for grouping and for
    // joints, and a joint is just a node that a skin points at.
    MeshId mesh;

    // Set only on the node that carries a skinned mesh.
    SkinId skin;
};

// A tree of nodes and the meshes they refer to.
//
// Nodes and meshes are addressed by handles for the same reason mesh elements
// are: a node can be removed while something still holds a reference to it, and
// that has to be detectable rather than silently pointing at a stranger.
class Scene {
public:
    NodeId add_node(std::string name, NodeId parent = NodeId{});

    MeshId add_mesh(geom::EditMesh mesh);

    // Returns an invalid handle if the joint and matrix counts disagree, which
    // is a broken skin rather than something to deform badly with.
    SkinId add_skin(Skin skin);

    void set_mesh(NodeId node, MeshId mesh);
    void set_skin(NodeId node, SkinId skin);

    [[nodiscard]] Node* node(NodeId id) { return nodes_.get(id); }

    [[nodiscard]] const Node* node(NodeId id) const { return nodes_.get(id); }

    [[nodiscard]] geom::EditMesh* mesh(MeshId id) { return meshes_.get(id); }

    [[nodiscard]] const geom::EditMesh* mesh(MeshId id) const { return meshes_.get(id); }

    [[nodiscard]] const Skin* skin(SkinId id) const { return skins_.get(id); }

    // One matrix per joint, ready for the vertex shader: where the joint stands
    // now, composed with where it stood when the mesh was bound.
    [[nodiscard]] std::vector<math::Mat4> joint_matrices(SkinId id) const;

    [[nodiscard]] const std::vector<NodeId>& roots() const { return roots_; }

    // Every live node, parents before children, which is the order a renderer
    // and an exporter both want.
    [[nodiscard]] std::vector<NodeId> nodes_in_order() const;

    [[nodiscard]] std::vector<MeshId> meshes() const;

    // Composed from this node up to its root. Walks the parents on each call,
    // which is fine at this depth and avoids a cache that could go stale.
    [[nodiscard]] math::Mat4 world_transform(NodeId id) const;

    [[nodiscard]] std::size_t node_count() const { return nodes_.size(); }

    [[nodiscard]] std::size_t mesh_count() const { return meshes_.size(); }

    void clear();

private:
    core::Arena<Node, tags::Node> nodes_;
    core::Arena<geom::EditMesh, tags::Mesh> meshes_;
    core::Arena<Skin, tags::Skin> skins_;
    std::vector<NodeId> roots_;
};

}  // namespace kinetiqra::scene
