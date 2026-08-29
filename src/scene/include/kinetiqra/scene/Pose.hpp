#pragma once

#include <kinetiqra/scene/Scene.hpp>

#include <cstdint>
#include <unordered_map>

namespace kinetiqra::scene {

// Transforms for some nodes, standing in for what the scene holds.
//
// This is how an animation is shown without being applied. Playing a clip fills
// a pose and hands it to the renderer; the scene keeps the transforms the user
// authored, so stopping playback returns the model to their work and the undo
// history keeps meaning what it says. Nothing here is a mutation of the
// document, which is why it does not go through the command stack.
//
// A node the pose does not mention falls through to its own transform, so a
// clip that drives three joints leaves the rest of the skeleton alone.
class Pose {
public:
    void set(NodeId id, const Transform& transform) {
        if (id.valid()) {
            entries_[id.index] = Entry{id.generation, transform};
        }
    }

    // Null when the pose says nothing about this node, and also when the handle
    // is stale, so a pose outliving the node it names is caught rather than
    // moving whatever took its slot.
    [[nodiscard]] const Transform* find(NodeId id) const {
        const auto found = entries_.find(id.index);
        if (found == entries_.end() || found->second.generation != id.generation) {
            return nullptr;
        }
        return &found->second.transform;
    }

    void clear() { entries_.clear(); }

    [[nodiscard]] bool empty() const { return entries_.empty(); }

    [[nodiscard]] std::size_t size() const { return entries_.size(); }

private:
    struct Entry {
        std::uint32_t generation;
        Transform transform;
    };

    std::unordered_map<std::uint32_t, Entry> entries_;
};

}  // namespace kinetiqra::scene
