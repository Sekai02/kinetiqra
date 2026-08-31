#include <kinetiqra/geom/Bake.hpp>

#include <cstring>
#include <map>
#include <unordered_map>

namespace kinetiqra::geom {

namespace {

// A candidate GPU vertex: everything that has to agree for two corners to share
// one. Compared by bits rather than by value, so that corners written from the
// same source data merge and corners that differ at all do not.
//
// Joints and weights are part of the key even though they are vertex
// attributes. They are constant across the corners of a vertex, so they never
// cause a split on their own, but leaving them out would let two corners on
// different vertices merge when their positions happen to coincide.
struct Key {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;
    math::Vec4 tangent;
    math::Vec4 joints;
    math::Vec4 weights;

    bool operator==(const Key& other) const { return std::memcmp(this, &other, sizeof(Key)) == 0; }
};

struct KeyHash {
    std::size_t operator()(const Key& key) const {
        // FNV-1a over the raw bytes. The struct is a flat block of floats with
        // no padding, which memcmp already relies on.
        const auto* bytes = reinterpret_cast<const unsigned char*>(&key);
        std::size_t hash = 1469598103934665603ULL;
        for (std::size_t i = 0; i < sizeof(Key); ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

}  // namespace

BakedMesh bake(const EditMesh& mesh) {
    BakedMesh baked;
    baked.skinned = mesh.skinned();

    const auto* normals = mesh.attributes().find<math::Vec3>(kNormal, Domain::Corner);
    const auto* uvs = mesh.attributes().find<math::Vec2>(kUv, Domain::Corner);
    const auto* tangents = mesh.attributes().find<math::Vec4>(kTangent, Domain::Corner);
    const auto* joints = mesh.attributes().find<math::Vec4>(kJoints, Domain::Vertex);
    const auto* weights = mesh.attributes().find<math::Vec4>(kWeights, Domain::Vertex);

    std::unordered_map<Key, std::uint32_t, KeyHash> emitted;

    const auto emit = [&](CornerId corner) -> std::uint32_t {
        const VertexId vertex = mesh.corner_vertex(corner);

        Key key{};
        key.position = mesh.position(vertex);
        if (normals != nullptr && corner.index < normals->size()) {
            key.normal = (*normals)[corner.index];
        }
        if (uvs != nullptr && corner.index < uvs->size()) {
            key.uv = (*uvs)[corner.index];
        }
        if (tangents != nullptr && corner.index < tangents->size()) {
            key.tangent = (*tangents)[corner.index];
        }
        if (joints != nullptr && vertex.index < joints->size()) {
            key.joints = (*joints)[vertex.index];
        }
        if (weights != nullptr && vertex.index < weights->size()) {
            key.weights = (*weights)[vertex.index];
        }

        if (auto found = emitted.find(key); found != emitted.end()) {
            return found->second;
        }

        const auto index = static_cast<std::uint32_t>(baked.vertex_count());

        baked.vertices.insert(baked.vertices.end(),
                              {key.position.x, key.position.y, key.position.z, key.normal.x,
                               key.normal.y, key.normal.z, key.uv.x, key.uv.y, key.tangent.x,
                               key.tangent.y, key.tangent.z, key.tangent.w});

        if (baked.skinned) {
            baked.vertices.insert(baked.vertices.end(),
                                  {key.joints.x, key.joints.y, key.joints.z, key.joints.w,
                                   key.weights.x, key.weights.y, key.weights.z, key.weights.w});
        }

        emitted.emplace(key, index);
        return index;
    };

    // Grouped by material so that each one owns an unbroken run of indices.
    // Sorted rather than left in slot order, because a section is a range and a
    // range cannot have holes in it.
    std::map<std::uint32_t, std::vector<FaceId>> by_material;
    for (const FaceId face : mesh.faces()) {
        by_material[mesh.material(face)].push_back(face);
    }

    for (const auto& [material, faces] : by_material) {
        const std::size_t started_at = baked.indices.size();

        for (const FaceId face : faces) {
            const std::vector<CornerId>* corners = mesh.face_corners(face);
            if (corners == nullptr || corners->size() < 3) {
                continue;
            }

            const std::uint32_t first = emit((*corners)[0]);
            std::uint32_t previous = emit((*corners)[1]);

            for (std::size_t i = 2; i < corners->size(); ++i) {
                const std::uint32_t current = emit((*corners)[i]);
                baked.indices.push_back(first);
                baked.indices.push_back(previous);
                baked.indices.push_back(current);
                previous = current;
            }
        }

        if (baked.indices.size() > started_at) {
            baked.sections.push_back(
                Section{material, started_at, baked.indices.size() - started_at});
        }
    }

    return baked;
}

}  // namespace kinetiqra::geom
