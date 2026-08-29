#pragma once

#include <kinetiqra/scene/Scene.hpp>

#include <filesystem>
#include <string>

namespace kinetiqra::io {

// Reads a .gltf or .glb into the scene, replacing whatever it held.
//
// Returns false and fills `error` on failure, leaving the scene empty rather
// than half populated, so a bad file cannot be mistaken for a loaded one.
//
// This is the only place in the engine that knows glTF exists. See
// docs/INVARIANTS.md.
bool import_gltf(const std::filesystem::path& path, scene::Scene& scene, std::string& error);

}  // namespace kinetiqra::io
