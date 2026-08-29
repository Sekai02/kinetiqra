#pragma once

#include <kinetiqra/math/Types.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Shader.hpp>

#include <cstdint>
#include <string>

namespace kinetiqra::render {

// Loads the GL entry points through a function supplied by the caller.
//
// `render` deliberately does not know which windowing library the editor uses,
// so the loader is handed in rather than looked up here.
using GlLoader = void* (*)(const char*);

// Draws the world. For now that means clearing and drawing the ground plane,
// but this is the seam every later pass hangs off.
class Renderer {
public:
    // Returns false and fills `error` if the entry points or the grid shader
    // cannot be loaded. A GL context must already be current.
    bool initialise(GlLoader loader, const std::string& shader_directory, std::string& error);

    void begin_frame(int width, int height) const;

    // `view_projection` and the camera position drive the ground plane; the
    // grid is reconstructed per pixel rather than stored as geometry.
    void draw_grid(const math::Mat4& view_projection, const math::Vec3& camera_position,
                   float far_plane) const;

    // Lit by a single directional light, which is enough to tell a flat face
    // from a smooth one and therefore enough to see whether the corner normals
    // survived the bake.
    void draw_mesh(const RenderMesh& mesh, const math::Mat4& model,
                   const math::Mat4& view_projection, const math::Vec3& camera_position) const;

    void shutdown();

    [[nodiscard]] std::string driver_description() const;

private:
    Shader grid_shader_;
    Shader mesh_shader_;

    // Core profile forbids drawing without a vertex array bound, even when the
    // vertices are generated entirely in the shader.
    std::uint32_t empty_vertex_array_{0};
    bool initialised_{false};
};

}  // namespace kinetiqra::render
