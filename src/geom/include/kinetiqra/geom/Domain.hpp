#pragma once

#include <cstddef>

namespace kinetiqra::geom {

// What an attribute is attached to.
//
// Corner is the one that matters. UVs, normals and vertex colours belong to a
// face-corner rather than to a vertex, which is what makes UV seams and hard
// edges representable at all: two faces meeting at one vertex can disagree.
// See docs/INVARIANTS.md.
enum class Domain : std::size_t {
    Vertex = 0,
    Corner = 1,
    Face = 2,
};

inline constexpr std::size_t kDomainCount = 3;

}  // namespace kinetiqra::geom
