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

void Scene::set_mesh(NodeId node_id, MeshId mesh_id) {
    if (Node* target = nodes_.get(node_id); target != nullptr) {
        target->mesh = mesh_id;
    }
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

math::Mat4 Scene::world_transform(NodeId id) const {
    math::Mat4 result{1.0F};

    for (NodeId current = id; nodes_.contains(current);) {
        const Node* node = nodes_.get(current);
        result = node->transform.matrix() * result;
        current = node->parent;
    }

    return result;
}

void Scene::clear() {
    nodes_.clear();
    meshes_.clear();
    roots_.clear();
}

}  // namespace kinetiqra::scene
