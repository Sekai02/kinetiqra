#pragma once

#include <kinetiqra/math/Types.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace kinetiqra::render {

// A linked vertex and fragment program.
//
// Compilation failures report the driver's log rather than leaving a black
// screen behind, because a shader that silently fails to compile is one of the
// least pleasant things to debug in graphics work.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Reads both stages from disk and links them. Returns false and fills
    // `error` on failure, leaving the shader unusable but valid to destroy.
    bool load(const std::string& vertex_path, const std::string& fragment_path, std::string& error);

    void bind() const;

    void set_uniform(std::string_view name, const math::Mat4& value) const;
    void set_uniform(std::string_view name, const math::Vec4& value) const;
    void set_uniform(std::string_view name, const math::Vec3& value) const;
    void set_uniform(std::string_view name, float value) const;
    void set_uniform(std::string_view name, int value) const;

    [[nodiscard]] bool valid() const { return program_ != 0; }

private:
    void destroy();

    std::uint32_t program_{0};
};

}  // namespace kinetiqra::render
