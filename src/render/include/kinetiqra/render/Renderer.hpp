#pragma once

#include <kinetiqra/math/Types.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Shader.hpp>

#include <cstdint>
#include <string>
#include <vector>

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

    // Deforms the mesh by its joints on the GPU.
    //
    // There is no model matrix: glTF specifies that the transform of the node
    // carrying a skinned mesh is ignored, because the joints already place it,
    // and applying both would move it twice.
    //
    // A skin with more joints than the shader's block holds is skipped rather
    // than drawn wrong; `kMaxJoints` says where that limit is.
    void draw_skinned_mesh(const RenderMesh& mesh, const std::vector<math::Mat4>& joints,
                           const math::Mat4& view_projection,
                           const math::Vec3& camera_position) const;

    static constexpr std::size_t kMaxJoints = 256;

    // The selection, drawn on top of the world in a flat colour.
    //
    // Points are drawn **without depth testing**, so the whole shape of the
    // mesh is visible at once rather than only the half facing the camera.
    //
    // Seeing one is not the same as reaching it: picking keeps the nearest face
    // along the ray and takes its nearest corner, so a vertex behind the
    // surface is shown but is selected only by orbiting around to it.
    void draw_points(const std::vector<math::Vec3>& points, const math::Mat4& model,
                     const math::Mat4& view_projection, const math::Vec4& colour, float size) const;

    // Triangles drawn over the surface they belong to, pulled very slightly
    // towards the camera so that they do not fight the mesh for the same
    // depth and flicker.
    void draw_overlay(const std::vector<math::Vec3>& triangles, const math::Mat4& model,
                      const math::Mat4& view_projection, const math::Vec4& colour) const;

    void shutdown();

    [[nodiscard]] std::string driver_description() const;

private:
    void upload_overlay(const std::vector<math::Vec3>& points) const;

    Shader grid_shader_;
    Shader mesh_shader_;
    Shader skinned_shader_;
    Shader overlay_shader_;

    // Holds the joint matrices for the draw in flight.
    std::uint32_t joint_buffer_{0};

    // Core profile forbids drawing without a vertex array bound, even when the
    // vertices are generated entirely in the shader.
    std::uint32_t empty_vertex_array_{0};

    // Rewritten every frame, which is why it is not a RenderMesh: those use
    // immutable storage on purpose, and the selection changes as fast as the
    // pointer moves.
    std::uint32_t overlay_buffer_{0};
    std::uint32_t overlay_vertex_array_{0};

    bool initialised_{false};
};

}  // namespace kinetiqra::render
