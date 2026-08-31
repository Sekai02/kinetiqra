#pragma once

#include <kinetiqra/core/Handle.hpp>
#include <kinetiqra/math/Types.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace kinetiqra::scene {

namespace tags {
struct Image {};

struct Material {};
}  // namespace tags

using ImageId = core::Handle<tags::Image>;
using MaterialId = core::Handle<tags::Material>;

// A picture, kept exactly as it arrived.
//
// The bytes are the encoded file, PNG or JPEG, rather than decoded pixels. Two
// reasons, and both matter. Export writes them straight back out, so an image
// survives a round trip byte for byte and the engine needs no image encoder at
// all. And decoding is only ever needed on the way to the GPU, which happens
// once per image rather than once per use.
struct Image {
    std::string name;

    // As glTF spells it: "image/png" or "image/jpeg".
    std::string mime_type;

    std::vector<std::uint8_t> bytes;

    [[nodiscard]] bool empty() const { return bytes.empty(); }
};

// How a texture behaves past the edge of its image.
enum class Wrap {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

// One of a material's pictures.
//
// glTF puts a sampler between the material and the image so that several
// textures can share one. Ours would carry the same defaults for every model
// there is, so the two settings that actually differ between files ride here
// and the rest are decided once, in the renderer. A sampler of its own arrives
// when something needs one.
struct Texture {
    ImageId image;
    Wrap wrap_s{Wrap::Repeat};
    Wrap wrap_t{Wrap::Repeat};

    [[nodiscard]] bool valid() const { return image.valid(); }
};

// What to do with a base colour that is not fully opaque.
enum class AlphaMode {
    Opaque,
    Mask,
    Blend,
};

// glTF's metallic-roughness material, which is the one every exporter writes.
//
// The factors multiply their textures rather than replacing them, so a material
// with no texture is the factor alone and a material with one is the texture
// tinted. That is the specification's rule and it is what lets the renderer
// treat both the same way.
struct Material {
    std::string name;

    math::Vec4 base_colour{1.0F, 1.0F, 1.0F, 1.0F};
    float metallic{1.0F};
    float roughness{1.0F};
    math::Vec3 emissive{0.0F, 0.0F, 0.0F};

    // How far the normal map is allowed to bend the surface, and how much of
    // the baked shadow in the occlusion map is applied.
    float normal_scale{1.0F};
    float occlusion_strength{1.0F};

    AlphaMode alpha_mode{AlphaMode::Opaque};
    float alpha_cutoff{0.5F};
    bool double_sided{false};

    Texture base_colour_texture;

    // Metalness in blue and roughness in green, packed into one image, which is
    // how glTF stores it. Occlusion is often the red channel of this very same
    // picture, which is why the two are so often the same image.
    Texture metallic_roughness_texture;

    Texture normal_texture;
    Texture occlusion_texture;
    Texture emissive_texture;
};

}  // namespace kinetiqra::scene
