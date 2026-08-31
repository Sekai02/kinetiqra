#pragma once

#include <kinetiqra/geom/Bake.hpp>

#include <cstdint>
#include <vector>

namespace kinetiqra::render {

using geom::Section;

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

    // One material's worth of it. The indices of a baked mesh are grouped so
    // that each material owns an unbroken run, which is what makes this a range
    // rather than a second buffer.
    void draw(std::size_t first_index, std::size_t index_count) const;

    void destroy();

    [[nodiscard]] std::size_t vertex_count() const { return vertex_count_; }

    [[nodiscard]] std::size_t index_count() const { return index_count_; }

    // Which vertex layout was uploaded, and therefore which shader can draw it.
    [[nodiscard]] bool skinned() const { return skinned_; }

    // The material runs of what was uploaded, kept here so that drawing does
    // not have to bake the mesh again to find out where they are.
    [[nodiscard]] const std::vector<Section>& sections() const { return sections_; }

    [[nodiscard]] bool valid() const { return vertex_array_ != 0 && index_count_ > 0; }

private:
    std::vector<Section> sections_;
    std::uint32_t vertex_array_{0};
    std::uint32_t vertex_buffer_{0};
    std::uint32_t index_buffer_{0};
    std::size_t vertex_count_{0};
    std::size_t index_count_{0};
    bool skinned_{false};
};

}  // namespace kinetiqra::render
