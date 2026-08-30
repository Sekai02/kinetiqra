#include <kinetiqra/app/Selection.hpp>

#include <algorithm>
#include <unordered_set>

namespace kinetiqra::app {

namespace {

template <typename Id>
bool holds(const std::vector<Id>& list, Id id) {
    return std::find(list.begin(), list.end(), id) != list.end();
}

template <typename Id>
void toggle_in(std::vector<Id>& list, Id id) {
    if (const auto found = std::find(list.begin(), list.end(), id); found != list.end()) {
        list.erase(found);
        return;
    }
    list.push_back(id);
}

}  // namespace

void Selection::set_mode(EditMode mode) {
    if (mode_ == mode) {
        return;
    }

    mode_ = mode;

    // Leaving mesh mode drops what was selected inside the mesh. Keeping it
    // would mean coming back later to a selection made against a mesh that may
    // have been edited, or replaced, in between.
    clear_elements();
}

void Selection::set_kind(ElementKind kind) {
    if (kind_ == kind) {
        return;
    }

    kind_ = kind;

    // Vertices and faces are not translations of each other. Turning a face
    // selection into the vertices around it, or the reverse, would be a guess
    // at what was meant.
    clear_elements();
}

void Selection::set_node(scene::NodeId node) {
    if (node_ == node) {
        return;
    }

    node_ = node;
    clear_elements();
}

void Selection::clear_elements() {
    vertices_.clear();
    faces_.clear();
}

void Selection::toggle(geom::VertexId vertex) {
    toggle_in(vertices_, vertex);
}

void Selection::toggle(geom::FaceId face) {
    toggle_in(faces_, face);
}

void Selection::select_only(geom::VertexId vertex) {
    clear_elements();
    vertices_.push_back(vertex);
}

void Selection::select_only(geom::FaceId face) {
    clear_elements();
    faces_.push_back(face);
}

bool Selection::contains(geom::VertexId vertex) const {
    return holds(vertices_, vertex);
}

bool Selection::contains(geom::FaceId face) const {
    return holds(faces_, face);
}

std::vector<geom::VertexId> Selection::moving_vertices(const geom::EditMesh& mesh) const {
    std::vector<geom::VertexId> moving;
    std::unordered_set<std::uint32_t> seen;

    const auto add = [&](geom::VertexId vertex) {
        if (!mesh.contains(vertex)) {
            return;
        }
        if (seen.insert(vertex.index).second) {
            moving.push_back(vertex);
        }
    };

    if (kind_ == ElementKind::Vertex) {
        for (const geom::VertexId vertex : vertices_) {
            add(vertex);
        }
        return moving;
    }

    for (const geom::FaceId face : faces_) {
        const std::vector<geom::CornerId>* corners = mesh.face_corners(face);
        if (corners == nullptr) {
            continue;
        }
        for (const geom::CornerId corner : *corners) {
            add(mesh.corner_vertex(corner));
        }
    }

    return moving;
}

}  // namespace kinetiqra::app
