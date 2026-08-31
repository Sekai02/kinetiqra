#pragma once

#include <kinetiqra/math/Types.hpp>
#include <kinetiqra/render/RenderMesh.hpp>
#include <kinetiqra/render/Shader.hpp>
#include <kinetiqra/render/Texture.hpp>

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
// What to paint a surface with, in the form the renderer wants it: the numbers
// of a glTF material, and the textures already on the device.
//
// A null texture means the material has none, and a white one is bound in its
// place so that the shader has no branch and no second program. The factors
// multiply the textures, which is the specification's rule, so a material with
// no texture is its factor alone.
struct MaterialDraw {
    math::Vec4 base_colour{1.0F, 1.0F, 1.0F, 1.0F};
    float metallic{1.0F};
    float roughness{1.0F};
    math::Vec3 emissive{0.0F, 0.0F, 0.0F};
    float normal_scale{1.0F};
    float occlusion_strength{1.0F};

    // 0 opaque, 1 masked, 2 blended, matching what the shader expects.
    int alpha_mode{0};
    float alpha_cutoff{0.5F};
    bool double_sided{false};

    const Texture* base_colour_map{nullptr};
    const Texture* metallic_roughness_map{nullptr};
    const Texture* normal_map{nullptr};
    const Texture* occlusion_map{nullptr};
    const Texture* emissive_map{nullptr};
};

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

    // One section of a mesh, painted by one material.
    //
    // Lit by a single directional light with glTF's metallic-roughness model.
    // Direct lighting only: with nothing in the world to reflect, metal comes
    // out dark rather than as chrome.
    void draw_mesh(const RenderMesh& mesh, const math::Mat4& model,
                   const math::Mat4& view_projection, const math::Vec3& camera_position,
                   const MaterialDraw& material, std::size_t first_index,
                   std::size_t index_count) const;

    // Deforms the mesh by its joints on the GPU.
    //
    // There is no model matrix: glTF specifies that the transform of the node
    // carrying a skinned mesh is ignored, because the joints already place it,
    // and applying both would move it twice.
    //
    // A skin with more joints than the shader's block holds is skipped rather
    // than drawn wrong; `kMaxJoints` says where that limit is.
    void draw_skinned_mesh(const RenderMesh& mesh, const std::vector<math::Mat4>& joints,
                           const math::Mat4& view_projection, const math::Vec3& camera_position,
                           const MaterialDraw& material, std::size_t first_index,
                           std::size_t index_count) const;

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

    // Binds a material's numbers and its five texture units, standing in the
    // white texture wherever there is no map.
    void bind_material(const Shader& shader, const MaterialDraw& material) const;

    Shader grid_shader_;
    Shader mesh_shader_;
    Shader skinned_shader_;
    Shader overlay_shader_;

    // One white pixel, bound wherever a material has no texture. Multiplying by
    // white is the same as not having one, which keeps the shader free of
    // branches on what a material happens to carry.
    Texture white_;

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
