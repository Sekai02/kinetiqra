#pragma once

#include <kinetiqra/geom/Raycast.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <optional>

namespace kinetiqra::scene {

// Where a ray met the scene, and which node owns what it met.
struct SceneHit {
    NodeId node;
    geom::FaceId face;
    geom::VertexId vertex;
    math::Vec3 point{0.0F, 0.0F, 0.0F};
    float distance{0.0F};
};

// The nearest mesh the ray meets, with the ray and the answer in world space.
//
// Meshes are tested where they are drawn, not where they are stored, which for
// a skinned mesh is somewhere else entirely: the joints carry the geometry and
// the node's own transform is ignored. `pose` picks against a clip being played
// rather than against the document, and is what the renderer was handed.
//
// Every vertex of every mesh is placed on each call. That is work proportional
// to the scene, done once per click, which is cheap next to being told that a
// click landed somewhere it did not.
[[nodiscard]] std::optional<SceneHit> raycast(const Scene& scene, const math::Ray& ray,
                                              const Pose* pose = nullptr);

}  // namespace kinetiqra::scene
