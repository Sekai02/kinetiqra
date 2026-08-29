#pragma once

#include <kinetiqra/core/Command.hpp>
#include <kinetiqra/scene/Scene.hpp>

namespace kinetiqra::app {

// Rotating one joint.
//
// The first thing in the editor that changes the scene, and therefore the first
// thing that has to be reversible. It holds the rotation before and after
// rather than a delta, so undo restores exactly what was there instead of
// applying an inverse and accumulating drift.
class RotateJoint final : public core::Command {
public:
    RotateJoint(scene::Scene& scene, scene::NodeId node, math::Quat before, math::Quat after)
        : scene_(scene), node_(node), before_(before), after_(after) {}

    void apply() override { set(after_); }

    void revert() override { set(before_); }

    [[nodiscard]] std::string_view name() const override { return "rotate joint"; }

private:
    void set(math::Quat rotation) {
        // The node may have gone if the scene was replaced, which is why this
        // asks rather than keeping a pointer.
        if (scene::Node* node = scene_.node(node_); node != nullptr) {
            node->transform.rotation = rotation;
        }
    }

    scene::Scene& scene_;
    scene::NodeId node_;
    math::Quat before_;
    math::Quat after_;
};

}  // namespace kinetiqra::app
