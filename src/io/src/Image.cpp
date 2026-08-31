#include <kinetiqra/io/Image.hpp>

#define STB_IMAGE_IMPLEMENTATION

// stb ships as a header that becomes a translation unit when the macro above is
// defined, which has to happen in exactly one place. This is that place.
#include <stb_image.h>

namespace kinetiqra::io {

DecodedImage decode_image(const std::vector<std::uint8_t>& bytes, std::string& error) {
    DecodedImage decoded;

    if (bytes.empty()) {
        error = "the image has no data";
        return decoded;
    }

    int width = 0;
    int height = 0;
    int channels = 0;

    // Four channels asked for whatever the file holds, so a caller never has to
    // deal with three-channel rows and their padding.
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width,
                                            &height, &channels, 4);

    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        error = std::string("could not decode the image: ") + (reason != nullptr ? reason : "");
        return decoded;
    }

    decoded.width = width;
    decoded.height = height;
    const std::size_t byte_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    decoded.pixels.assign(pixels, pixels + byte_count);

    stbi_image_free(pixels);
    return decoded;
}

}  // namespace kinetiqra::io
