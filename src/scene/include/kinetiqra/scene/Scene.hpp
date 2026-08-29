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
}  // namespace tags

using NodeId = core::Handle<tags::Node>;
using MeshId = core::Handle<tags::Mesh>;

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

    // Nodes without a mesh are common: glTF uses them for grouping and, later,
    // for joints.
    MeshId mesh;
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

    void set_mesh(NodeId node, MeshId mesh);

    [[nodiscard]] Node* node(NodeId id) { return nodes_.get(id); }

    [[nodiscard]] const Node* node(NodeId id) const { return nodes_.get(id); }

    [[nodiscard]] geom::EditMesh* mesh(MeshId id) { return meshes_.get(id); }

    [[nodiscard]] const geom::EditMesh* mesh(MeshId id) const { return meshes_.get(id); }

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
    std::vector<NodeId> roots_;
};

}  // namespace kinetiqra::scene
