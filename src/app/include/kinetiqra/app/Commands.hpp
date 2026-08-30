#pragma once

#include <kinetiqra/core/Command.hpp>
#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <string>
#include <utility>
#include <vector>

namespace kinetiqra::app {

// Every one of these holds handles rather than pointers and resolves them on
// each apply and revert. The scene can be replaced while a command sits in the
// history, and a command that kept a pointer would write into whatever moved
// into the space its target used to occupy. See docs/INVARIANTS.md.

// Moving, turning or scaling one node.
//
// It holds whole transforms on each side rather than a delta, so undo restores
// exactly what was there instead of applying an inverse and accumulating drift
// over a long history. One command covers all three operations because that is
// what a gizmo produces: a drag can change more than one of them at once.
class TransformNode final : public core::Command {
public:
    TransformNode(scene::Scene& scene, scene::NodeId node, const scene::Transform& before,
                  const scene::Transform& after)
        : scene_(scene), node_(node), before_(before), after_(after) {}

    void apply() override { set(after_); }

    void revert() override { set(before_); }

    [[nodiscard]] std::string_view name() const override { return "transform node"; }

private:
    void set(const scene::Transform& transform) {
        if (scene::Node* node = scene_.node(node_); node != nullptr) {
            node->transform = transform;
        }
    }

    scene::Scene& scene_;
    scene::NodeId node_;
    scene::Transform before_;
    scene::Transform after_;
};

// Moving a set of vertices, as one step.
//
// A drag touches the same vertices every frame, so this is pushed once when the
// gesture ends. One command per frame would leave a history that takes hundreds
// of presses to walk back through, which is the same as having no undo.
class MoveVertices final : public core::Command {
public:
    struct Moved {
        geom::VertexId vertex;
        math::Vec3 before;
        math::Vec3 after;
    };

    MoveVertices(scene::Scene& scene, scene::MeshId mesh, std::vector<Moved> moved)
        : scene_(scene), mesh_(mesh), moved_(std::move(moved)) {}

    void apply() override { set(false); }

    void revert() override { set(true); }

    [[nodiscard]] std::string_view name() const override { return "move vertices"; }

    [[nodiscard]] bool empty() const { return moved_.empty(); }

private:
    void set(bool backwards) {
        geom::EditMesh* mesh = scene_.mesh(mesh_);
        if (mesh == nullptr) {
            return;
        }

        for (const Moved& one : moved_) {
            if (mesh->contains(one.vertex)) {
                mesh->set_position(one.vertex, backwards ? one.before : one.after);
            }
        }
    }

    scene::Scene& scene_;
    scene::MeshId mesh_;
    std::vector<Moved> moved_;
};

// An operation that added and removed elements, remembered by keeping the mesh
// from either side of it.
//
// Extruding creates vertices, corners and faces and destroys others, so there
// is no single value to put back. Snapshots are the honest answer for now: the
// alternative is undoing by deleting exactly what was created, which needs the
// operation to be perfectly invertible and is fragile in a way this is not. The
// cost is two copies of the mesh sitting in the history, which is worth naming
// rather than hiding, and is what a later milestone replaces with reversible
// operators.
class ReplaceMesh final : public core::Command {
public:
    ReplaceMesh(scene::Scene& scene, scene::MeshId mesh, geom::EditMesh before,
                geom::EditMesh after, std::string label)
        : scene_(scene),
          mesh_(mesh),
          before_(std::move(before)),
          after_(std::move(after)),
          label_(std::move(label)) {}

    void apply() override { set(after_); }

    void revert() override { set(before_); }

    [[nodiscard]] std::string_view name() const override { return label_; }

private:
    void set(const geom::EditMesh& source) {
        if (geom::EditMesh* mesh = scene_.mesh(mesh_); mesh != nullptr) {
            *mesh = source.clone();
        }
    }

    scene::Scene& scene_;
    scene::MeshId mesh_;
    geom::EditMesh before_;
    geom::EditMesh after_;
    std::string label_;
};

}  // namespace kinetiqra::app
