#include <kinetiqra/scene/Pose.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <utility>

namespace kinetiqra::scene {

math::Mat4 Transform::matrix() const {
    // Scale, then rotate, then translate, which is the order glTF specifies and
    // the only one that keeps a scaled child from shearing under a rotated
    // parent.
    math::Mat4 result = glm::translate(math::Mat4{1.0F}, translation);
    result *= glm::mat4_cast(rotation);
    return glm::scale(result, scale);
}

NodeId Scene::add_node(std::string name, NodeId parent) {
    Node node;
    node.name = std::move(name);
    node.parent = parent;

    const NodeId id = nodes_.insert(std::move(node));

    if (Node* parent_node = nodes_.get(parent); parent_node != nullptr) {
        parent_node->children.push_back(id);
    } else {
        roots_.push_back(id);
    }

    return id;
}

MeshId Scene::add_mesh(geom::EditMesh mesh) {
    return meshes_.insert(std::move(mesh));
}

SkinId Scene::add_skin(Skin skin) {
    // A skin with a matrix count that disagrees with its joint count cannot be
    // used: some joint would have no way back to bind space. Refusing it here
    // means the renderer never has to wonder.
    if (skin.joints.size() != skin.inverse_bind.size() || skin.joints.empty()) {
        return SkinId{};
    }

    return skins_.insert(std::move(skin));
}

void Scene::set_mesh(NodeId node_id, MeshId mesh_id) {
    if (Node* target = nodes_.get(node_id); target != nullptr) {
        target->mesh = mesh_id;
    }
}

void Scene::set_skin(NodeId node_id, SkinId skin_id) {
    if (Node* target = nodes_.get(node_id); target != nullptr) {
        target->skin = skin_id;
    }
}

std::vector<math::Mat4> Scene::joint_matrices(SkinId id, const Pose* pose) const {
    const Skin* skin = skins_.get(id);
    if (skin == nullptr) {
        return {};
    }

    std::vector<math::Mat4> matrices;
    matrices.reserve(skin->joints.size());

    for (std::size_t joint = 0; joint < skin->joints.size(); ++joint) {
        // Where the joint is now, undoing where it was when the mesh was bound.
        // In the bind pose these cancel and the vertex is left where it was
        // modelled, which is the property the tests pin down.
        matrices.push_back(world_transform(skin->joints[joint], pose) * skin->inverse_bind[joint]);
    }

    return matrices;
}

std::vector<math::Mat4> Scene::vertex_matrices(NodeId id, const Pose* pose) const {
    const Node* node = nodes_.get(id);
    if (node == nullptr) {
        return {};
    }

    const geom::EditMesh* mesh = meshes_.get(node->mesh);
    if (mesh == nullptr) {
        return {};
    }

    const auto* joints = mesh->attributes().find<math::Vec4>(geom::kJoints, geom::Domain::Vertex);
    const auto* weights = mesh->attributes().find<math::Vec4>(geom::kWeights, geom::Domain::Vertex);
    const std::size_t count = mesh->attributes().count(geom::Domain::Vertex);

    if (!node->skin.valid() || joints == nullptr || weights == nullptr) {
        return std::vector<math::Mat4>(count, world_transform(id, pose));
    }

    // The node's own transform has no part in this. glTF says the transform of
    // the node carrying a skinned mesh is ignored, because the joints have
    // already placed the mesh, and applying both would move it twice.
    const std::vector<math::Mat4> skin = joint_matrices(node->skin, pose);

    std::vector<math::Mat4> matrices(count, math::Mat4{1.0F});

    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        const math::Vec4 index = (*joints)[vertex];
        const math::Vec4 weight = (*weights)[vertex];

        math::Mat4 blended(0.0F);
        float total = 0.0F;

        for (int slot = 0; slot < 4; ++slot) {
            const auto joint = static_cast<std::size_t>(index[slot]);
            if (joint < skin.size() && weight[slot] != 0.0F) {
                blended += skin[joint] * weight[slot];
                total += weight[slot];
            }
        }

        // A vertex the rig forgot keeps the identity rather than collapsing to
        // the origin, which is easier to see and to fix than a spike.
        matrices[vertex] = total > 0.0F ? blended * (1.0F / total) : math::Mat4{1.0F};
    }

    return matrices;
}

std::vector<math::Vec3> Scene::world_positions(NodeId id, const Pose* pose) const {
    const Node* node = nodes_.get(id);
    if (node == nullptr) {
        return {};
    }

    const geom::EditMesh* mesh = meshes_.get(node->mesh);
    if (mesh == nullptr) {
        return {};
    }

    const auto* positions =
        mesh->attributes().find<math::Vec3>(geom::kPosition, geom::Domain::Vertex);
    if (positions == nullptr) {
        return {};
    }

    const std::vector<math::Mat4> matrices = vertex_matrices(id, pose);

    std::vector<math::Vec3> world(positions->size(), math::Vec3{0.0F, 0.0F, 0.0F});
    for (std::size_t vertex = 0; vertex < positions->size() && vertex < matrices.size(); ++vertex) {
        world[vertex] = math::Vec3{matrices[vertex] * math::Vec4{(*positions)[vertex], 1.0F}};
    }

    return world;
}

std::vector<NodeId> Scene::nodes_in_order() const {
    std::vector<NodeId> ordered;
    ordered.reserve(nodes_.size());

    std::vector<NodeId> pending(roots_.rbegin(), roots_.rend());

    while (!pending.empty()) {
        const NodeId id = pending.back();
        pending.pop_back();

        const Node* current = nodes_.get(id);
        if (current == nullptr) {
            continue;
        }

        ordered.push_back(id);

        // Pushed in reverse so that siblings come out in the order they were
        // added, which matches the order the file listed them.
        for (auto child = current->children.rbegin(); child != current->children.rend(); ++child) {
            pending.push_back(*child);
        }
    }

    return ordered;
}

std::vector<MeshId> Scene::meshes() const {
    std::vector<MeshId> result;
    result.reserve(meshes_.size());

    for (std::uint32_t slot = 0; slot < meshes_.slot_count(); ++slot) {
        if (meshes_.alive(slot)) {
            result.push_back(meshes_.id_at(slot));
        }
    }

    return result;
}

math::Mat4 Scene::world_transform(NodeId id, const Pose* pose) const {
    math::Mat4 result{1.0F};

    for (NodeId current = id; nodes_.contains(current);) {
        const Node* node = nodes_.get(current);

        // The pose speaks for the nodes it mentions and stays quiet about the
        // rest, so a clip driving three joints leaves the others as authored.
        const Transform* posed = pose != nullptr ? pose->find(current) : nullptr;
        result = (posed != nullptr ? posed->matrix() : node->transform.matrix()) * result;

        current = node->parent;
    }

    return result;
}

void Scene::clear() {
    nodes_.clear();
    meshes_.clear();
    skins_.clear();
    roots_.clear();
}

}  // namespace kinetiqra::scene
