#pragma once

#include <cstdint>

namespace kinetiqra::core {

// A reference to an element living in an Arena.
//
// The engine refers to mesh elements this way rather than by pointer, because a
// pointer into a mesh is invalidated by every topology change, does not
// serialise, and makes undo unworkable. See docs/INVARIANTS.md.
//
// The tag parameter exists solely to make handles of different kinds distinct
// types, so that passing a FaceId where a VertexId belongs fails to compile
// rather than resolving to whatever happens to sit at that index.
//
// The generation is what turns a stale handle into a detectable error. A slot
// bumps it when the element in it is removed, so a handle held across that
// removal no longer matches, even once the slot has been reused.
template <typename Tag>
struct Handle {
    static constexpr std::uint32_t kInvalidIndex = 0xFFFFFFFFU;

    std::uint32_t index{kInvalidIndex};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const { return index != kInvalidIndex; }

    friend constexpr bool operator==(Handle lhs, Handle rhs) {
        return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }

    friend constexpr bool operator!=(Handle lhs, Handle rhs) { return !(lhs == rhs); }
};

}  // namespace kinetiqra::core
