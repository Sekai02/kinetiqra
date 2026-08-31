#include <kinetiqra/render/Texture.hpp>

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kinetiqra::render {

namespace {

GLint as_wrap(TextureWrap wrap) {
    switch (wrap) {
        case TextureWrap::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case TextureWrap::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case TextureWrap::Repeat:
            break;
    }
    return GL_REPEAT;
}

// How many times the image can be halved before there is nothing left.
GLsizei level_count(int width, int height) {
    const int largest = std::max(width, height);
    return static_cast<GLsizei>(std::floor(std::log2(static_cast<float>(largest)))) + 1;
}

}  // namespace

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : texture_(std::exchange(other.texture_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        texture_ = std::exchange(other.texture_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

void Texture::destroy() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

void Texture::upload(const std::uint8_t* pixels, int width, int height, ColourSpace space,
                     TextureWrap wrap_s, TextureWrap wrap_t) {
    destroy();

    if (pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }

    width_ = width;
    height_ = height;

    glCreateTextures(GL_TEXTURE_2D, 1, &texture_);

    // Immutable storage, like the vertex buffers: the size and format of a
    // texture never change, only its contents.
    glTextureStorage2D(texture_, level_count(width, height),
                       space == ColourSpace::Srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, width, height);
    glTextureSubImage2D(texture_, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateTextureMipmap(texture_);

    glTextureParameteri(texture_, GL_TEXTURE_WRAP_S, as_wrap(wrap_s));
    glTextureParameteri(texture_, GL_TEXTURE_WRAP_T, as_wrap(wrap_t));
    glTextureParameteri(texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(texture_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::bind(std::uint32_t unit) const {
    glBindTextureUnit(unit, texture_);
}

}  // namespace kinetiqra::render
