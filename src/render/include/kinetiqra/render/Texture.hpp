#pragma once

#include <cstdint>

namespace kinetiqra::render {

// How the values in an image are meant to be read.
//
// A photograph is stored bent by a curve that gives dark tones more of the
// available numbers, because that is where an eye notices detail. Lighting maths
// has to happen on the straightened values, and the hardware will straighten
// them for free if it is told the image is sRGB.
//
// A normal map is not a picture. Its numbers are directions, and straightening
// them would bend every direction it holds, so it is uploaded as it is. Metal,
// roughness and occlusion are measurements rather than colours for the same
// reason.
enum class ColourSpace {
    Srgb,
    Linear,
};

// How a texture behaves past the edge of its image.
enum class TextureWrap {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

// An image on the device.
//
// Always four channels and always mipmapped. Uploading three-channel data would
// save a quarter of the memory and cost a row alignment problem on every driver
// that disagrees about padding, and a texture without mipmaps shimmers the
// moment the camera moves away from it.
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // `pixels` is tightly packed RGBA, one byte a channel, top row first.
    // Replaces whatever was there.
    void upload(const std::uint8_t* pixels, int width, int height, ColourSpace space,
                TextureWrap wrap_s = TextureWrap::Repeat, TextureWrap wrap_t = TextureWrap::Repeat);

    void bind(std::uint32_t unit) const;

    void destroy();

    [[nodiscard]] bool valid() const { return texture_ != 0; }

    [[nodiscard]] int width() const { return width_; }

    [[nodiscard]] int height() const { return height_; }

private:
    std::uint32_t texture_{0};
    int width_{0};
    int height_{0};
};

}  // namespace kinetiqra::render
