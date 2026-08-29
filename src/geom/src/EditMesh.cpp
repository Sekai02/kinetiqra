#include <kinetiqra/geom/EditMesh.hpp>

namespace kinetiqra::geom {

EditMesh::EditMesh() {
    // The three channels every mesh is expected to have. Creating them here
    // means a mesh is never half built, and the bake can rely on them existing.
    attributes_.add<math::Vec3>(kPosition, Domain::Vertex);
    attributes_.add<math::Vec3>(kNormal, Domain::Corner, math::Vec3{0.0F, 1.0F, 0.0F});
    attributes_.add<math::Vec2>(kUv, Domain::Corner);
}

VertexId EditMesh::add_vertex(math::Vec3 position) {
    const VertexId id = vertices_.insert(Empty{});

    // Channels are indexed by slot, so they grow with the arena rather than
    // with the number of live elements. A removed slot leaves a hole, which
    // costs a little memory and saves reindexing every handle in the mesh.
    attributes_.resize(Domain::Vertex, vertices_.slot_count());
    set_position(id, position);
    return id;
}

FaceId EditMesh::add_face(const std::vector<VertexId>& vertices,
                          std::vector<CornerId>* created_corners) {
    Face face;
    face.corners.reserve(vertices.size());

    for (const VertexId vertex : vertices) {
        const CornerId corner = corners_.insert(vertex);
        face.corners.push_back(corner);
    }

    attributes_.resize(Domain::Corner, corners_.slot_count());

    if (created_corners != nullptr) {
        *created_corners = face.corners;
    }

    const FaceId id = faces_.insert(std::move(face));
    attributes_.resize(Domain::Face, faces_.slot_count());
    return id;
}

bool EditMesh::remove_vertex(VertexId id) {
    return vertices_.remove(id);
}

VertexId EditMesh::corner_vertex(CornerId id) const {
    const VertexId* vertex = corners_.get(id);
    return vertex != nullptr ? *vertex : VertexId{};
}

const std::vector<CornerId>* EditMesh::face_corners(FaceId id) const {
    const Face* face = faces_.get(id);
    return face != nullptr ? &face->corners : nullptr;
}

std::vector<FaceId> EditMesh::faces() const {
    std::vector<FaceId> result;
    result.reserve(faces_.size());

    for (std::uint32_t slot = 0; slot < faces_.slot_count(); ++slot) {
        if (faces_.alive(slot)) {
            result.push_back(faces_.id_at(slot));
        }
    }

    return result;
}

math::Vec3 EditMesh::position(VertexId id) const {
    const auto* channel = attributes_.find<math::Vec3>(kPosition, Domain::Vertex);
    if (channel == nullptr || !contains(id) || id.index >= channel->size()) {
        return math::Vec3{0.0F};
    }
    return (*channel)[id.index];
}

void EditMesh::set_position(VertexId id, math::Vec3 value) {
    auto* channel = attributes_.find<math::Vec3>(kPosition, Domain::Vertex);
    if (channel != nullptr && contains(id) && id.index < channel->size()) {
        (*channel)[id.index] = value;
    }
}

void EditMesh::set_normal(CornerId id, math::Vec3 value) {
    auto* channel = attributes_.find<math::Vec3>(kNormal, Domain::Corner);
    if (channel != nullptr && contains(id) && id.index < channel->size()) {
        (*channel)[id.index] = value;
    }
}

void EditMesh::set_uv(CornerId id, math::Vec2 value) {
    auto* channel = attributes_.find<math::Vec2>(kUv, Domain::Corner);
    if (channel != nullptr && contains(id) && id.index < channel->size()) {
        (*channel)[id.index] = value;
    }
}

std::string EditMesh::validate() const {
    if (attributes_.find<math::Vec3>(kPosition, Domain::Vertex) == nullptr) {
        return "the position channel is missing or has the wrong type";
    }

    for (std::uint32_t slot = 0; slot < faces_.slot_count(); ++slot) {
        if (!faces_.alive(slot)) {
            continue;
        }

        const Face* face = faces_.get(faces_.id_at(slot));
        if (face->corners.size() < 3) {
            return "a face has fewer than three corners";
        }

        for (const CornerId corner : face->corners) {
            if (!corners_.contains(corner)) {
                return "a face refers to a corner that no longer exists";
            }
            if (!vertices_.contains(corner_vertex(corner))) {
                return "a corner refers to a vertex that no longer exists";
            }
        }
    }

    return {};
}

}  // namespace kinetiqra::geom
