#pragma once

#include <kinetiqra/geom/Bake.hpp>

#include <cstdint>

namespace kinetiqra::render {

// The GPU side of a mesh: a vertex array, a vertex buffer and an index buffer.
//
// It owns nothing but those handles. What to upload was decided by geom::bake,
// which is where the splitting and merging live; this only moves the result
// onto the device.
class RenderMesh {
public:
    RenderMesh() = default;
    ~RenderMesh();

    RenderMesh(const RenderMesh&) = delete;
    RenderMesh& operator=(const RenderMesh&) = delete;
    RenderMesh(RenderMesh&& other) noexcept;
    RenderMesh& operator=(RenderMesh&& other) noexcept;

    // Replaces whatever was there. The buffers are immutable once written, so a
    // changed mesh is uploaded again rather than patched in place.
    void upload(const geom::BakedMesh& baked);

    void draw() const;

    void destroy();

    [[nodiscard]] std::size_t vertex_count() const { return vertex_count_; }

    [[nodiscard]] std::size_t index_count() const { return index_count_; }

    [[nodiscard]] bool valid() const { return vertex_array_ != 0 && index_count_ > 0; }

private:
    std::uint32_t vertex_array_{0};
    std::uint32_t vertex_buffer_{0};
    std::uint32_t index_buffer_{0};
    std::size_t vertex_count_{0};
    std::size_t index_count_{0};
};

}  // namespace kinetiqra::render
