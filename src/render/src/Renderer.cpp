#include <kinetiqra/render/Renderer.hpp>

#include <glad/glad.h>

#include <array>

namespace kinetiqra::render {

namespace {

// After the five a material can carry, which the fragment shader declares
// bindings for in that order.
constexpr std::uint32_t kIrradianceUnit = 5;
constexpr std::uint32_t kReflectionUnit = 6;

const char* as_text(const GLubyte* value) {
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unknown";
}

}  // namespace

bool Renderer::initialise(GlLoader loader, const std::string& shader_directory,
                          std::string& error) {
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loader)) == 0) {
        error = "could not load the OpenGL entry points";
        return false;
    }

    if (GLAD_GL_VERSION_4_5 == 0) {
        error = "OpenGL 4.5 core is required, but the driver reports " +
                std::string(as_text(glGetString(GL_VERSION)));
        return false;
    }

    if (!grid_shader_.load(shader_directory + "/grid.vert", shader_directory + "/grid.frag",
                           error)) {
        return false;
    }

    if (!mesh_shader_.load(shader_directory + "/mesh.vert", shader_directory + "/mesh.frag",
                           error)) {
        return false;
    }

    // The fragment stage is shared: skinning changes where a vertex lands, not
    // how the surface is lit.
    if (!skinned_shader_.load(shader_directory + "/skinned.vert", shader_directory + "/mesh.frag",
                              error)) {
        return false;
    }

    if (!overlay_shader_.load(shader_directory + "/overlay.vert",
                              shader_directory + "/overlay.frag", error)) {
        return false;
    }

    // After the shaders and before anything is drawn: it runs its own passes,
    // and it needs the context set up but nothing else.
    if (!environment_.build(shader_directory, error)) {
        return false;
    }

    glCreateBuffers(1, &joint_buffer_);
    glNamedBufferData(joint_buffer_, static_cast<GLsizeiptr>(kMaxJoints * sizeof(math::Mat4)),
                      nullptr, GL_DYNAMIC_DRAW);

    glCreateVertexArrays(1, &empty_vertex_array_);

    glCreateBuffers(1, &overlay_buffer_);
    glCreateVertexArrays(1, &overlay_vertex_array_);
    glEnableVertexArrayAttrib(overlay_vertex_array_, 0);
    glVertexArrayAttribFormat(overlay_vertex_array_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(overlay_vertex_array_, 0, 0);
    glVertexArrayVertexBuffer(overlay_vertex_array_, 0, overlay_buffer_, 0,
                              static_cast<GLsizei>(sizeof(math::Vec3)));

    // So that the overlay's vertex shader can choose how large a vertex marker
    // is, rather than every point being one pixel.
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Linear, not sRGB: it stands in for a missing texture and gets multiplied
    // by a factor, so it has to be exactly one.
    constexpr std::array<std::uint8_t, 4> kWhite{255, 255, 255, 255};
    white_.upload(kWhite.data(), 1, 1, ColourSpace::Linear);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initialised_ = true;
    return true;
}

void Renderer::begin_frame(int width, int height) const {
    glViewport(0, 0, width, height);
    glClearColor(0.13F, 0.14F, 0.16F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::draw_grid(const math::Mat4& view_projection, const math::Vec3& camera_position,
                         float far_plane) const {
    if (!initialised_) {
        return;
    }

    grid_shader_.bind();
    grid_shader_.set_uniform("u_view_projection", view_projection);
    grid_shader_.set_uniform("u_inverse_view_projection", glm::inverse(view_projection));
    grid_shader_.set_uniform("u_camera_position", camera_position);
    grid_shader_.set_uniform("u_far_plane", far_plane);

    glBindVertexArray(empty_vertex_array_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::bind_material(const Shader& shader, const MaterialDraw& material) const {
    shader.set_uniform("u_base_colour", material.base_colour);
    shader.set_uniform("u_metallic", material.metallic);
    shader.set_uniform("u_roughness", material.roughness);
    shader.set_uniform("u_emissive", material.emissive);
    shader.set_uniform("u_normal_scale", material.normal_scale);
    shader.set_uniform("u_occlusion_strength", material.occlusion_strength);
    shader.set_uniform("u_alpha_mode", material.alpha_mode);
    shader.set_uniform("u_alpha_cutoff", material.alpha_cutoff);

    // The order matches the binding points the fragment shader declares.
    const std::array<const Texture*, 5> maps{material.base_colour_map,
                                             material.metallic_roughness_map, material.normal_map,
                                             material.occlusion_map, material.emissive_map};

    for (std::uint32_t unit = 0; unit < maps.size(); ++unit) {
        const Texture* map = maps[unit];
        (map != nullptr && map->valid() ? *map : white_).bind(unit);
    }

    // After the material's five, and the same for every surface: the world does
    // not change from one draw to the next.
    environment_.bind(kIrradianceUnit, kReflectionUnit);
    shader.set_uniform("u_reflection_levels", environment_.reflection_levels());
}

void Renderer::draw_mesh(const RenderMesh& mesh, const math::Mat4& model,
                         const math::Mat4& view_projection, const math::Vec3& camera_position,
                         const MaterialDraw& material, std::size_t first_index,
                         std::size_t index_count) const {
    if (!initialised_ || !mesh.valid()) {
        return;
    }

    mesh_shader_.bind();
    mesh_shader_.set_uniform("u_model", model);
    mesh_shader_.set_uniform("u_view_projection", view_projection);
    mesh_shader_.set_uniform("u_camera_position", camera_position);

    // The normal matrix, so that a non-uniform scale does not shear the normals
    // away from the surface.
    mesh_shader_.set_uniform("u_normal_matrix",
                             math::Mat4(glm::transpose(glm::inverse(math::Mat3(model)))));

    bind_material(mesh_shader_, material);
    mesh.draw(first_index, index_count);
}

void Renderer::draw_skinned_mesh(const RenderMesh& mesh, const std::vector<math::Mat4>& joints,
                                 const math::Mat4& view_projection,
                                 const math::Vec3& camera_position, const MaterialDraw& material,
                                 std::size_t first_index, std::size_t index_count) const {
    if (!initialised_ || !mesh.valid() || joints.empty()) {
        return;
    }

    if (joints.size() > kMaxJoints) {
        // Drawing the first 256 would deform the rest of the model by whatever
        // happened to be in the buffer, which looks like a bug in the mesh
        // rather than a limit being hit.
        return;
    }

    glNamedBufferSubData(joint_buffer_, 0,
                         static_cast<GLsizeiptr>(joints.size() * sizeof(math::Mat4)),
                         joints.data());
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, joint_buffer_);

    skinned_shader_.bind();
    skinned_shader_.set_uniform("u_view_projection", view_projection);
    skinned_shader_.set_uniform("u_camera_position", camera_position);

    bind_material(skinned_shader_, material);
    mesh.draw(first_index, index_count);
}

void Renderer::upload_overlay(const std::vector<math::Vec3>& points) const {
    // Reallocated rather than patched: the size changes with the selection, and
    // orphaning the old store lets the driver hand back memory the previous
    // frame may still be reading.
    glNamedBufferData(overlay_buffer_, static_cast<GLsizeiptr>(points.size() * sizeof(math::Vec3)),
                      points.data(), GL_DYNAMIC_DRAW);
}

void Renderer::draw_points(const std::vector<math::Vec3>& points, const math::Mat4& model,
                           const math::Mat4& view_projection, const math::Vec4& colour,
                           float size) const {
    if (!initialised_ || points.empty()) {
        return;
    }

    upload_overlay(points);

    overlay_shader_.bind();
    overlay_shader_.set_uniform("u_model", model);
    overlay_shader_.set_uniform("u_view_projection", view_projection);
    overlay_shader_.set_uniform("u_colour", colour);
    overlay_shader_.set_uniform("u_point_size", size);

    // Vertices behind the surface stay clickable, which is the difference
    // between editing a mesh and orbiting around it looking for a handle.
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(overlay_vertex_array_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size()));
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::draw_overlay(const std::vector<math::Vec3>& triangles, const math::Mat4& model,
                            const math::Mat4& view_projection, const math::Vec4& colour) const {
    if (!initialised_ || triangles.empty()) {
        return;
    }

    upload_overlay(triangles);

    overlay_shader_.bind();
    overlay_shader_.set_uniform("u_model", model);
    overlay_shader_.set_uniform("u_view_projection", view_projection);
    overlay_shader_.set_uniform("u_colour", colour);
    overlay_shader_.set_uniform("u_point_size", 1.0F);

    // Pulled towards the camera by a fraction of a depth unit. Without this the
    // highlight and the face it covers sit at exactly the same depth and the
    // two flicker against each other as the camera moves.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0F, -1.0F);

    glBindVertexArray(overlay_vertex_array_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangles.size()));
    glBindVertexArray(0);

    glPolygonOffset(0.0F, 0.0F);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void Renderer::shutdown() {
    if (overlay_vertex_array_ != 0) {
        glDeleteVertexArrays(1, &overlay_vertex_array_);
        overlay_vertex_array_ = 0;
    }

    if (overlay_buffer_ != 0) {
        glDeleteBuffers(1, &overlay_buffer_);
        overlay_buffer_ = 0;
    }

    if (empty_vertex_array_ != 0) {
        glDeleteVertexArrays(1, &empty_vertex_array_);
        empty_vertex_array_ = 0;
    }

    if (joint_buffer_ != 0) {
        glDeleteBuffers(1, &joint_buffer_);
        joint_buffer_ = 0;
    }

    grid_shader_ = Shader{};
    mesh_shader_ = Shader{};
    skinned_shader_ = Shader{};
    overlay_shader_ = Shader{};
    white_.destroy();
    environment_.destroy();
    initialised_ = false;
}

std::string Renderer::driver_description() const {
    return std::string(as_text(glGetString(GL_RENDERER))) + ", OpenGL " +
           as_text(glGetString(GL_VERSION));
}

}  // namespace kinetiqra::render
