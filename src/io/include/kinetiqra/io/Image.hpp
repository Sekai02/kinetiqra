#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kinetiqra::io {

// A picture turned into pixels.
//
// Always four channels, whatever the file had, so that a caller uploading this
// to a device never has to think about row padding or about which of three
// channel orders it was handed.
struct DecodedImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;  // RGBA, top row first

    [[nodiscard]] bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

// Turns the bytes of a PNG or a JPEG into pixels.
//
// Decoding lives here for the same reason parsing glTF does: it is a file
// format, and no module outside io should know which library reads it. The
// scene keeps images encoded, so this runs once on the way to the GPU rather
// than on the way in.
//
// Returns an empty result and fills `error` on failure.
[[nodiscard]] DecodedImage decode_image(const std::vector<std::uint8_t>& bytes, std::string& error);

}  // namespace kinetiqra::io
