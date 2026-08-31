#pragma once

#include <cstdint>
#include <string>

namespace kinetiqra::render {

class Shader;

// The world a surface reflects, precomputed once.
//
// A material's metalness says how much of what it shows is reflection rather
// than its own colour, so a metal with nothing around it has nothing to show
// and comes out black. This is what it shows.
//
// Three cubemaps, in the order they are built, each made from the one before:
//
//   sky          the environment itself, drawn by a shader
//   irradiance   the sky gathered over a hemisphere, which is the light a
//                rough surface receives and what replaces a constant ambient
//   reflection   the sky blurred more at each mip, so a roughness can pick a
//                level and get the blur it would have gathered
//
// All of it is computed at startup and never again, because the sky does not
// change. Swapping in a loaded panorama later means replacing the first pass
// and leaving the other two alone.
class Environment {
public:
    Environment() = default;
    ~Environment();

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    Environment(Environment&&) = delete;
    Environment& operator=(Environment&&) = delete;

    // Loads the three shaders and runs the three passes. A GL context must be
    // current. Returns false and fills `error` if a shader will not compile.
    bool build(const std::string& shader_directory, std::string& error);

    void bind(std::uint32_t irradiance_unit, std::uint32_t reflection_unit) const;

    void destroy();

    [[nodiscard]] bool valid() const { return irradiance_ != 0 && reflection_ != 0; }

    // How many mips the reflection map has, which is the range a roughness is
    // mapped onto. The shader needs it, so it is not a private detail.
    [[nodiscard]] float reflection_levels() const;

private:
    // Draws one full screen triangle into each face of a cubemap, with the
    // shader already bound and its own uniforms already set.
    void render_faces(const Shader& shader, std::uint32_t target, int size, int level) const;

    std::uint32_t sky_{0};
    std::uint32_t irradiance_{0};
    std::uint32_t reflection_{0};

    std::uint32_t framebuffer_{0};

    // Core profile forbids drawing with no vertex array bound, even when the
    // vertices come from the shader.
    std::uint32_t empty_vertex_array_{0};
};

}  // namespace kinetiqra::render
