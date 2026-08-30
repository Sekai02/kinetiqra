#include <kinetiqra/render/Shader.hpp>

#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace kinetiqra::render {

namespace {

bool read_file(const std::string& path, std::string& out, std::string& error) {
    std::ifstream stream(path);
    if (!stream) {
        error = "could not open '" + path + "'";
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

std::string shader_log(GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 0) {
        return {};
    }

    std::vector<char> log(static_cast<std::size_t>(length));
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return std::string(log.data(), static_cast<std::size_t>(length - 1));
}

std::string program_log(GLuint program) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 0) {
        return {};
    }

    std::vector<char> log(static_cast<std::size_t>(length));
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return std::string(log.data(), static_cast<std::size_t>(length - 1));
}

GLuint compile(GLenum stage, const std::string& source, const std::string& path,
               std::string& error) {
    const GLuint shader = glCreateShader(stage);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        error = path + ":\n" + shader_log(shader);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

}  // namespace

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept : program_(std::exchange(other.program_, 0)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = std::exchange(other.program_, 0);
    }
    return *this;
}

void Shader::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

bool Shader::load(const std::string& vertex_path, const std::string& fragment_path,
                  std::string& error) {
    std::string vertex_source;
    std::string fragment_source;
    if (!read_file(vertex_path, vertex_source, error) ||
        !read_file(fragment_path, fragment_source, error)) {
        return false;
    }

    const GLuint vertex = compile(GL_VERTEX_SHADER, vertex_source, vertex_path, error);
    if (vertex == 0) {
        return false;
    }

    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragment_source, fragment_path, error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    // The stages are reference counted by the program, so they can go now.
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        error = "link failed:\n" + program_log(program);
        glDeleteProgram(program);
        return false;
    }

    destroy();
    program_ = program;
    return true;
}

void Shader::bind() const {
    glUseProgram(program_);
}

void Shader::set_uniform(std::string_view name, const math::Mat4& value) const {
    const std::string key(name);
    glUniformMatrix4fv(glGetUniformLocation(program_, key.c_str()), 1, GL_FALSE, &value[0][0]);
}

void Shader::set_uniform(std::string_view name, const math::Vec4& value) const {
    const std::string key(name);
    glUniform4fv(glGetUniformLocation(program_, key.c_str()), 1, &value[0]);
}

void Shader::set_uniform(std::string_view name, const math::Vec3& value) const {
    const std::string key(name);
    glUniform3fv(glGetUniformLocation(program_, key.c_str()), 1, &value[0]);
}

void Shader::set_uniform(std::string_view name, float value) const {
    const std::string key(name);
    glUniform1f(glGetUniformLocation(program_, key.c_str()), value);
}

}  // namespace kinetiqra::render
