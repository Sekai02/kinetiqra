#pragma once

#include <kinetiqra/anim/Clip.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace kinetiqra::io {

// Reads a .gltf or .glb into the scene and its clips, replacing whatever they
// held.
//
// Returns false and fills `error` on failure, leaving both empty rather than
// half populated, so a bad file cannot be mistaken for a loaded one.
//
// `clips` is optional: a caller that only wants geometry can pass nullptr and
// the animations are skipped rather than read and thrown away.
//
// This is the only place in the engine that knows glTF exists. See
// docs/INVARIANTS.md.
bool import_gltf(const std::filesystem::path& path, scene::Scene& scene, std::string& error,
                 std::vector<anim::Clip>* clips = nullptr);

}  // namespace kinetiqra::io
