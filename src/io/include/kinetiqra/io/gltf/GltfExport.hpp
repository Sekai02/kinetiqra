#pragma once

#include <kinetiqra/anim/Clip.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace kinetiqra::io {

// Writes the scene and its clips out, choosing the container by the extension:
// `.glb` is one self-contained binary and anything else is JSON with a `.bin`
// written beside it.
//
// Returns false and fills `error` on failure. The asset is validated before a
// byte is written, so a mistake of ours is reported here rather than turning up
// as a model that will not open in someone else's engine.
//
// What leaves is the scene as authored. A pose being played is a view of the
// document rather than an edit of it, so it has nothing to do with export.
//
// Two things do not survive the trip, because glTF has nowhere to put them:
// faces of more than three sides are triangulated, and attributes are moved off
// the corners onto duplicated vertices. Both are the bake's doing and both are
// what the format is for. See docs/INVARIANTS.md.
bool export_gltf(const std::filesystem::path& path, const scene::Scene& scene,
                 const std::vector<anim::Clip>& clips, std::string& error);

}  // namespace kinetiqra::io
