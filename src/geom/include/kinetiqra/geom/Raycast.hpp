#pragma once

#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/math/Ray.hpp>

#include <optional>

namespace kinetiqra::geom {

// Where a ray met the mesh.
//
// The vertex is the one of that face nearest the point, which is what makes
// clicking near a corner select the corner rather than the middle of the face.
struct Hit {
    FaceId face;
    VertexId vertex;
    math::Vec3 point{0.0F, 0.0F, 0.0F};
    float distance{0.0F};
};

// The nearest face the ray meets.
//
// Faces are triangulated as a fan, exactly as `bake` does, so that what is
// picked is what is drawn. A face of fewer than three corners is skipped rather
// than guessed at.
//
// `positions` replaces the mesh's own position channel, indexed by vertex slot.
// It exists because a skinned mesh is not where it is stored: the joints move
// it, and picking has to agree with what is on screen rather than with what is
// in the file. Pass nullptr to use the mesh as it stands.
[[nodiscard]] std::optional<Hit> raycast(const EditMesh& mesh, const math::Ray& ray,
                                         const std::vector<math::Vec3>* positions = nullptr);

}  // namespace kinetiqra::geom
