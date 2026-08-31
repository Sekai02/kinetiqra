#include <kinetiqra/math/Types.hpp>
#include <kinetiqra/render/Environment.hpp>
#include <kinetiqra/render/Shader.hpp>

#include <glad/glad.h>

#include <array>
#include <cmath>

namespace kinetiqra::render {

namespace {

// Small on purpose. Nothing here is looked at directly: the sky feeds two
// convolutions, and what a surface reflects at these roughnesses carries no
// detail worth more pixels. The prefilter runs 128 samples for every one of
// them, so this is the difference between a startup that is instant and one
// that is noticed.
constexpr int kSkySize = 128;
constexpr int kIrradianceSize = 32;
constexpr int kReflectionSize = 128;
constexpr int kReflectionLevels = 6;

// The six faces of a cubemap, each as the three axes it looks along. The signs
// are the specification's, which follow RenderMan rather than anything the rest
// of the engine does, and getting one wrong shows up as a seam or as a sky that
// is upside down.
struct Face {
    math::Vec3 forward;
    math::Vec3 right;
    math::Vec3 up;
};

constexpr std::array<Face, 6> kFaces{{
    {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, -1.0F, 0.0F}},
    {{-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, -1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
    {{0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
    {{0.0F, 0.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
}};

// A floating point format, not the usual eight bits a channel. A sky is a light
// source and its brightest parts run past one; storing it in a format that
// stops at one would clip the sky before anything had a chance to reflect it.
constexpr GLenum kFormat = GL_R11F_G11F_B10F;

std::uint32_t make_cubemap(int size, int levels) {
    std::uint32_t texture = 0;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texture);
    glTextureStorage2D(texture, levels, kFormat, size, size);

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER,
                        levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return texture;
}

}  // namespace

Environment::~Environment() {
    destroy();
}

void Environment::destroy() {
    for (std::uint32_t* texture : {&sky_, &irradiance_, &reflection_}) {
        if (*texture != 0) {
            glDeleteTextures(1, texture);
            *texture = 0;
        }
    }

    if (framebuffer_ != 0) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }

    if (empty_vertex_array_ != 0) {
        glDeleteVertexArrays(1, &empty_vertex_array_);
        empty_vertex_array_ = 0;
    }
}

float Environment::reflection_levels() const {
    return static_cast<float>(kReflectionLevels);
}

void Environment::render_faces(const Shader& shader, std::uint32_t target, int size,
                               int level) const {
    glViewport(0, 0, size, size);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glBindVertexArray(empty_vertex_array_);

    for (std::size_t face = 0; face < kFaces.size(); ++face) {
        // One layer of the cubemap at a time. Attaching the whole texture and
        // choosing the face in a geometry shader would draw all six at once and
        // cost a shader stage nothing else here needs.
        glNamedFramebufferTextureLayer(framebuffer_, GL_COLOR_ATTACHMENT0, target, level,
                                       static_cast<GLint>(face));

        shader.set_uniform("u_forward", kFaces[face].forward);
        shader.set_uniform("u_right", kFaces[face].right);
        shader.set_uniform("u_up", kFaces[face].up);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Environment::build(const std::string& shader_directory, std::string& error) {
    destroy();

    Shader sky;
    Shader irradiance;
    Shader prefilter;

    const std::string vertex = shader_directory + "/cubemap.vert";

    if (!sky.load(vertex, shader_directory + "/sky.frag", error) ||
        !irradiance.load(vertex, shader_directory + "/irradiance.frag", error) ||
        !prefilter.load(vertex, shader_directory + "/prefilter.frag", error)) {
        return false;
    }

    glCreateFramebuffers(1, &framebuffer_);
    glCreateVertexArrays(1, &empty_vertex_array_);

    sky_ = make_cubemap(kSkySize, 1);
    irradiance_ = make_cubemap(kIrradianceSize, 1);
    reflection_ = make_cubemap(kReflectionSize, kReflectionLevels);

    // Depth testing and blending are on for the world; neither means anything
    // when the whole target is being replaced by one triangle.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    sky.bind();
    render_faces(sky, sky_, kSkySize, 0);

    glBindTextureUnit(0, sky_);

    irradiance.bind();
    render_faces(irradiance, irradiance_, kIrradianceSize, 0);

    prefilter.bind();
    for (int level = 0; level < kReflectionLevels; ++level) {
        // Each level is half the size of the one above and stands for a
        // roughness that much further along, so the blur a surface needs is
        // already sitting at the level its roughness picks.
        const int size = kReflectionSize >> level;
        const float roughness =
            static_cast<float>(level) / static_cast<float>(kReflectionLevels - 1);

        prefilter.set_uniform("u_roughness", roughness);
        render_faces(prefilter, reflection_, size, level);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    return true;
}

void Environment::bind(std::uint32_t irradiance_unit, std::uint32_t reflection_unit) const {
    glBindTextureUnit(irradiance_unit, irradiance_);
    glBindTextureUnit(reflection_unit, reflection_);
}

}  // namespace kinetiqra::render
