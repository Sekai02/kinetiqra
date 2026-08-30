#pragma once

#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <vector>

namespace kinetiqra::app {

// What the editor is pointed at: whole nodes, or the elements of one node's
// mesh.
//
// The two are separate modes rather than one list of everything, because the
// question a click answers is different in each. In object mode it is which
// thing, and in mesh mode it is which part of this thing.
enum class EditMode {
    Object,
    Mesh,
};

// Which part. Moving needs vertices and extruding needs faces, so both exist
// and the mode says which one a click is asking about.
enum class ElementKind {
    Vertex,
    Face,
};

// What the gizmo does with a drag. Kept as the editor's own enum so that the
// gizmo library stays inside the one file that talks to it.
enum class GizmoOperation {
    Translate,
    Rotate,
    Scale,
};

// The current selection.
//
// This is deliberately a type of its own rather than a handful of members on
// the application. The rules about what a new selection clears are easy to get
// subtly wrong, and here they can be tested without a window.
class Selection {
public:
    [[nodiscard]] EditMode mode() const { return mode_; }

    [[nodiscard]] ElementKind kind() const { return kind_; }

    [[nodiscard]] scene::NodeId node() const { return node_; }

    [[nodiscard]] const std::vector<geom::VertexId>& vertices() const { return vertices_; }

    [[nodiscard]] const std::vector<geom::FaceId>& faces() const { return faces_; }

    void set_mode(EditMode mode);
    void set_kind(ElementKind kind);

    // Selecting a different node drops the element selection, which belonged to
    // the mesh of the node being left behind.
    void set_node(scene::NodeId node);

    void clear_elements();

    // Adds if absent, removes if present, which is what a shift click does.
    void toggle(geom::VertexId vertex);
    void toggle(geom::FaceId face);

    // Replaces the element selection with this one thing.
    void select_only(geom::VertexId vertex);
    void select_only(geom::FaceId face);

    [[nodiscard]] bool contains(geom::VertexId vertex) const;
    [[nodiscard]] bool contains(geom::FaceId face) const;

    [[nodiscard]] bool empty() const { return vertices_.empty() && faces_.empty(); }

    // The vertices a drag would move: the selected ones in vertex mode, and the
    // ones the selected faces are built from in face mode. Duplicates are
    // removed, so a vertex shared by two selected faces moves once rather than
    // twice as far.
    [[nodiscard]] std::vector<geom::VertexId> moving_vertices(const geom::EditMesh& mesh) const;

private:
    EditMode mode_{EditMode::Object};
    ElementKind kind_{ElementKind::Vertex};
    scene::NodeId node_;
    std::vector<geom::VertexId> vertices_;
    std::vector<geom::FaceId> faces_;
};

}  // namespace kinetiqra::app
