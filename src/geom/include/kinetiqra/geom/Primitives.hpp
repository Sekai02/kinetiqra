#pragma once

#include <kinetiqra/geom/EditMesh.hpp>

namespace kinetiqra::geom {

// A box centred on the origin, in metres.
//
// Eight vertices carry the corners of the box, and twenty-four corners carry a
// normal and a UV each: every face gets its own, which is what makes the edges
// hard and gives each face its own patch of texture space. The same eight
// positions with per-vertex normals would round the box off and leave no way to
// lay out a seam.
[[nodiscard]] EditMesh make_box(math::Vec3 size = math::Vec3{1.0F, 1.0F, 1.0F});

}  // namespace kinetiqra::geom
