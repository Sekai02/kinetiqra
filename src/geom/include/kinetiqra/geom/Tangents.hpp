#pragma once

#include <kinetiqra/geom/EditMesh.hpp>

namespace kinetiqra::geom {

// Works out which way the texture runs across the surface, and writes it into
// the mesh's tangent channel.
//
// A normal map stores its directions in the space of the texture, so reading
// one needs to know where the texture's U and V axes point in the world at that
// spot. Files often carry that as a TANGENT attribute and just as often do not:
// the juggernaut, for one, has none, so without this its normal maps would have
// nothing to be measured against.
//
// The tangent of each triangle comes from how its positions move against how
// its UVs move. Corners shared between triangles accumulate, so the frame turns
// smoothly across a surface and breaks where the UVs break, which is exactly
// where it should break.
//
// The w component is the handedness: whether the third axis of the frame is the
// cross product of the other two or its opposite. It is what makes a mirrored
// UV island light the same way as the island it was mirrored from.
//
// A face of fewer than three corners is skipped, and a corner whose triangle
// has no UV area keeps whatever it had, since there is no direction to be
// derived from a texture that does not move.
void compute_tangents(EditMesh& mesh);

}  // namespace kinetiqra::geom
