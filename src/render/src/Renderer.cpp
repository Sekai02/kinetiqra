#include <kinetiqra/render/Renderer.hpp>

#include <glad/glad.h>

namespace kinetiqra::render {

namespace {

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

    glCreateVertexArrays(1, &empty_vertex_array_);

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

void Renderer::shutdown() {
    if (empty_vertex_array_ != 0) {
        glDeleteVertexArrays(1, &empty_vertex_array_);
        empty_vertex_array_ = 0;
    }

    grid_shader_ = Shader{};
    initialised_ = false;
}

std::string Renderer::driver_description() const {
    return std::string(as_text(glGetString(GL_RENDERER))) + ", OpenGL " +
           as_text(glGetString(GL_VERSION));
}

}  // namespace kinetiqra::render
