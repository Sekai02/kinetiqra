#include <kinetiqra/geom/Bake.hpp>

#include <cstring>
#include <unordered_map>

namespace kinetiqra::geom {

namespace {

// A candidate GPU vertex: everything that has to agree for two corners to share
// one. Compared by bits rather than by value, so that corners written from the
// same source data merge and corners that differ at all do not.
struct Key {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 uv;

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

    const auto* normals = mesh.attributes().find<math::Vec3>(kNormal, Domain::Corner);
    const auto* uvs = mesh.attributes().find<math::Vec2>(kUv, Domain::Corner);

    std::unordered_map<Key, std::uint32_t, KeyHash> emitted;

    const auto emit = [&](CornerId corner) -> std::uint32_t {
        Key key{};
        key.position = mesh.position(mesh.corner_vertex(corner));
        if (normals != nullptr && corner.index < normals->size()) {
            key.normal = (*normals)[corner.index];
        }
        if (uvs != nullptr && corner.index < uvs->size()) {
            key.uv = (*uvs)[corner.index];
        }

        if (auto found = emitted.find(key); found != emitted.end()) {
            return found->second;
        }

        const auto index = static_cast<std::uint32_t>(baked.vertex_count());
        baked.vertices.insert(baked.vertices.end(),
                              {key.position.x, key.position.y, key.position.z, key.normal.x,
                               key.normal.y, key.normal.z, key.uv.x, key.uv.y});
        emitted.emplace(key, index);
        return index;
    };

    for (const FaceId face : mesh.faces()) {
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

    return baked;
}

}  // namespace kinetiqra::geom
