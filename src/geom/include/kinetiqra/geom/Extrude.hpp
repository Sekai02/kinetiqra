#pragma once

#include <kinetiqra/geom/EditMesh.hpp>

#include <vector>

namespace kinetiqra::geom {

// What an extrusion created, which is what the editor selects afterwards.
struct ExtrudeResult {
    // The new faces standing where the originals were. These are what the
    // editor selects afterwards, since they are what the user then moves.
    std::vector<FaceId> caps;

    // The sides raised along the boundary of the selection. Kept apart from the
    // caps because selecting them too would drag the new shape flat again.
    std::vector<FaceId> walls;

    // The duplicated vertices the caps are built from.
    std::vector<VertexId> vertices;
};

// Raises the selected faces off the surface, leaving walls behind them.
//
// Every vertex the selection uses is duplicated, the new faces are built from
// the duplicates with the original winding, and a wall goes up along each
// boundary edge. An edge shared by two selected faces is interior and gets no
// wall, which is what makes extruding a patch behave as one surface rather than
// as a row of separate boxes. The original faces are removed, or the result
// would have a wall inside it.
//
// The new faces are created **in place**, with no offset. The caller moves them
// afterwards, which is how a modelling tool behaves and avoids inventing a
// distance nobody asked for.
//
// Walls are four sided. This is the first thing in the engine that makes a
// quad, and quads are exactly what glTF cannot store, so a mesh that has been
// extruded no longer round trips through it unchanged.
ExtrudeResult extrude(EditMesh& mesh, const std::vector<FaceId>& faces);

}  // namespace kinetiqra::geom
