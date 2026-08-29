#include <kinetiqra/render/RenderMesh.hpp>

#include <glad/glad.h>

#include <utility>

namespace kinetiqra::render {

namespace {

constexpr GLuint kBindingIndex = 0;
constexpr GLsizei kStride = static_cast<GLsizei>(geom::BakedMesh::kFloatsPerVertex * sizeof(float));

}  // namespace

RenderMesh::~RenderMesh() {
    destroy();
}

RenderMesh::RenderMesh(RenderMesh&& other) noexcept
    : vertex_array_(std::exchange(other.vertex_array_, 0)),
      vertex_buffer_(std::exchange(other.vertex_buffer_, 0)),
      index_buffer_(std::exchange(other.index_buffer_, 0)),
      vertex_count_(std::exchange(other.vertex_count_, 0)),
      index_count_(std::exchange(other.index_count_, 0)) {}

RenderMesh& RenderMesh::operator=(RenderMesh&& other) noexcept {
    if (this != &other) {
        destroy();
        vertex_array_ = std::exchange(other.vertex_array_, 0);
        vertex_buffer_ = std::exchange(other.vertex_buffer_, 0);
        index_buffer_ = std::exchange(other.index_buffer_, 0);
        vertex_count_ = std::exchange(other.vertex_count_, 0);
        index_count_ = std::exchange(other.index_count_, 0);
    }
    return *this;
}

void RenderMesh::destroy() {
    if (index_buffer_ != 0) {
        glDeleteBuffers(1, &index_buffer_);
        index_buffer_ = 0;
    }
    if (vertex_buffer_ != 0) {
        glDeleteBuffers(1, &vertex_buffer_);
        vertex_buffer_ = 0;
    }
    if (vertex_array_ != 0) {
        glDeleteVertexArrays(1, &vertex_array_);
        vertex_array_ = 0;
    }
    vertex_count_ = 0;
    index_count_ = 0;
}

void RenderMesh::upload(const geom::BakedMesh& baked) {
    destroy();

    if (baked.vertices.empty() || baked.indices.empty()) {
        return;
    }

    vertex_count_ = baked.vertex_count();
    index_count_ = baked.indices.size();

    glCreateBuffers(1, &vertex_buffer_);
    glNamedBufferStorage(vertex_buffer_,
                         static_cast<GLsizeiptr>(baked.vertices.size() * sizeof(float)),
                         baked.vertices.data(), 0);

    glCreateBuffers(1, &index_buffer_);
    glNamedBufferStorage(index_buffer_,
                         static_cast<GLsizeiptr>(baked.indices.size() * sizeof(std::uint32_t)),
                         baked.indices.data(), 0);

    glCreateVertexArrays(1, &vertex_array_);
    glVertexArrayVertexBuffer(vertex_array_, kBindingIndex, vertex_buffer_, 0, kStride);
    glVertexArrayElementBuffer(vertex_array_, index_buffer_);

    // position, normal, uv, in the order geom::bake interleaves them.
    const struct {
        GLuint location;
        GLint components;
        GLuint offset;
    } attributes[] = {
        {0, 3, 0},
        {1, 3, 3 * sizeof(float)},
        {2, 2, 6 * sizeof(float)},
    };

    for (const auto& attribute : attributes) {
        glEnableVertexArrayAttrib(vertex_array_, attribute.location);
        glVertexArrayAttribFormat(vertex_array_, attribute.location, attribute.components, GL_FLOAT,
                                  GL_FALSE, attribute.offset);
        glVertexArrayAttribBinding(vertex_array_, attribute.location, kBindingIndex);
    }
}

void RenderMesh::draw() const {
    if (!valid()) {
        return;
    }

    glBindVertexArray(vertex_array_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace kinetiqra::render
