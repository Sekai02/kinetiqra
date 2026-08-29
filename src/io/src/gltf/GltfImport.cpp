#include <kinetiqra/io/gltf/GltfImport.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/geometric.hpp>

#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace kinetiqra::io {

namespace {

// glTF is right-handed, Y-up and in metres, which is what the engine uses, so
// there is nothing to convert today. The seam still belongs here: the moment a
// second format arrives, this is where its conversion goes, and no module
// outside io has to learn about it.

// Bitwise identity, with negative zero folded onto zero so that the two forms
// of the same coordinate weld together.
struct PositionKey {
    math::Vec3 value;

    explicit PositionKey(math::Vec3 position)
        : value(position.x + 0.0F, position.y + 0.0F, position.z + 0.0F) {}

    bool operator==(const PositionKey& other) const {
        return std::memcmp(&value, &other.value, sizeof(value)) == 0;
    }
};

struct PositionHash {
    std::size_t operator()(const PositionKey& key) const {
        const auto* bytes = reinterpret_cast<const unsigned char*>(&key.value);
        std::size_t hash = 1469598103934665603ULL;
        for (std::size_t i = 0; i < sizeof(key.value); ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

struct PrimitiveData {
    std::vector<math::Vec3> positions;
    std::vector<math::Vec3> normals;
    std::vector<math::Vec2> uvs;
    std::vector<std::uint32_t> indices;
};

bool read_primitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
                    PrimitiveData& out, std::string& error) {
    const auto* position_attribute = primitive.findAttribute("POSITION");
    if (position_attribute == primitive.attributes.end()) {
        error = "a primitive has no POSITION attribute";
        return false;
    }

    const auto& position_accessor = asset.accessors[position_attribute->accessorIndex];
    out.positions.resize(position_accessor.count);
    fastgltf::iterateAccessorWithIndex<math::Vec3>(
        asset, position_accessor,
        [&](math::Vec3 value, std::size_t index) { out.positions[index] = value; });

    if (const auto* normals = primitive.findAttribute("NORMAL");
        normals != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[normals->accessorIndex];
        out.normals.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<math::Vec3>(
            asset, accessor,
            [&](math::Vec3 value, std::size_t index) { out.normals[index] = value; });
    }

    if (const auto* uvs = primitive.findAttribute("TEXCOORD_0");
        uvs != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[uvs->accessorIndex];
        out.uvs.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<math::Vec2>(
            asset, accessor, [&](math::Vec2 value, std::size_t index) { out.uvs[index] = value; });
    }

    if (!primitive.indicesAccessor.has_value()) {
        error = "a primitive has no indices";
        return false;
    }

    const auto& index_accessor = asset.accessors[primitive.indicesAccessor.value()];
    out.indices.resize(index_accessor.count);
    fastgltf::iterateAccessorWithIndex<std::uint32_t>(
        asset, index_accessor,
        [&](std::uint32_t value, std::size_t index) { out.indices[index] = value; });

    if (out.indices.size() % 3 != 0) {
        error = "a primitive's index count is not a multiple of three";
        return false;
    }

    return true;
}

// Turns the file's split vertices back into a mesh the editor can work on.
//
// glTF stores one vertex per corner, because that is what a GPU needs: a cube
// arrives with 24 vertices. Welding the ones that share a position and moving
// their normals and UVs onto the corners recovers the 8 vertices the model
// actually has. Nothing is lost, the data changes domain, and geom::bake
// reverses it exactly. See docs/INVARIANTS.md.
void append_primitive(const PrimitiveData& data, geom::EditMesh& mesh,
                      std::unordered_map<PositionKey, geom::VertexId, PositionHash>& welded) {
    const auto vertex_for = [&](std::uint32_t index) {
        const PositionKey key{data.positions[index]};
        if (const auto found = welded.find(key); found != welded.end()) {
            return found->second;
        }

        const geom::VertexId id = mesh.add_vertex(key.value);
        welded.emplace(key, id);
        return id;
    };

    for (std::size_t triangle = 0; triangle + 2 < data.indices.size(); triangle += 3) {
        const std::uint32_t a = data.indices[triangle];
        const std::uint32_t b = data.indices[triangle + 1];
        const std::uint32_t c = data.indices[triangle + 2];

        std::vector<geom::CornerId> corners;
        const geom::FaceId face =
            mesh.add_face({vertex_for(a), vertex_for(b), vertex_for(c)}, &corners);
        (void)face;

        // A primitive without normals gets flat ones, computed per triangle,
        // which is what the specification asks for.
        math::Vec3 flat{0.0F, 1.0F, 0.0F};
        if (data.normals.empty()) {
            const math::Vec3 edge_one = data.positions[b] - data.positions[a];
            const math::Vec3 edge_two = data.positions[c] - data.positions[a];
            const math::Vec3 cross = glm::cross(edge_one, edge_two);
            if (glm::length(cross) > 0.0F) {
                flat = glm::normalize(cross);
            }
        }

        const std::array<std::uint32_t, 3> source{a, b, c};
        for (std::size_t corner = 0; corner < corners.size(); ++corner) {
            const std::uint32_t index = source[corner];

            mesh.set_normal(corners[corner], data.normals.empty() ? flat : data.normals[index]);

            if (!data.uvs.empty()) {
                mesh.set_uv(corners[corner], data.uvs[index]);
            }
        }
    }
}

scene::NodeId add_node(const fastgltf::Asset& asset, std::size_t node_index, scene::Scene& target,
                       scene::NodeId parent, const std::vector<scene::MeshId>& meshes) {
    const fastgltf::Node& source = asset.nodes[node_index];

    const scene::NodeId id = target.add_node(std::string(source.name), parent);
    scene::Node* node = target.node(id);

    // DecomposeNodeMatrices was requested, so a node that stored a matrix has
    // already been turned into translation, rotation and scale for us.
    if (const auto* trs = std::get_if<fastgltf::TRS>(&source.transform); trs != nullptr) {
        node->transform.translation =
            math::Vec3{trs->translation[0], trs->translation[1], trs->translation[2]};
        node->transform.rotation =
            math::Quat{trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]};
        node->transform.scale = math::Vec3{trs->scale[0], trs->scale[1], trs->scale[2]};
    }

    if (source.meshIndex.has_value() && source.meshIndex.value() < meshes.size()) {
        target.set_mesh(id, meshes[source.meshIndex.value()]);
    }

    for (const std::size_t child : source.children) {
        add_node(asset, child, target, id, meshes);
    }

    return id;
}

}  // namespace

bool import_gltf(const std::filesystem::path& path, scene::Scene& target, std::string& error) {
    target.clear();

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        error = "could not read '" + path.string() +
                "': " + std::string(fastgltf::getErrorMessage(data.error()));
        return false;
    }

    fastgltf::Parser parser;
    // DecomposeNodeMatrices keeps transforms as TRS, which is what the scene
    // stores and what animation will need. GenerateMeshIndices covers the
    // primitives that arrive without an index buffer.
    constexpr auto kOptions = fastgltf::Options::LoadExternalBuffers |
                              fastgltf::Options::DecomposeNodeMatrices |
                              fastgltf::Options::GenerateMeshIndices;

    // fastgltf resolves external buffers and images against this directory and
    // rejects an empty one, which is what a bare filename such as "model.glb"
    // produces.
    std::filesystem::path directory = path.parent_path();
    if (directory.empty()) {
        directory = std::filesystem::current_path();
    }

    auto parsed = parser.loadGltf(data.get(), directory, kOptions);
    if (parsed.error() != fastgltf::Error::None) {
        error = "could not parse '" + path.string() +
                "': " + std::string(fastgltf::getErrorMessage(parsed.error()));
        target.clear();
        return false;
    }

    const fastgltf::Asset& asset = parsed.get();

    std::vector<scene::MeshId> meshes;
    meshes.reserve(asset.meshes.size());

    for (const fastgltf::Mesh& source : asset.meshes) {
        geom::EditMesh mesh;

        // Shared across the primitives of one glTF mesh, so that a model split
        // by material still welds along the seams between those parts.
        std::unordered_map<PositionKey, geom::VertexId, PositionHash> welded;

        for (const fastgltf::Primitive& primitive : source.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                // Points and lines have no faces to build; skipping them beats
                // refusing the whole file over a stray primitive.
                continue;
            }

            PrimitiveData primitive_data;
            if (!read_primitive(asset, primitive, primitive_data, error)) {
                target.clear();
                return false;
            }

            append_primitive(primitive_data, mesh, welded);
        }

        meshes.push_back(target.add_mesh(std::move(mesh)));
    }

    if (asset.scenes.empty()) {
        for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
            add_node(asset, index, target, scene::NodeId{}, meshes);
        }
        return true;
    }

    const std::size_t scene_index = asset.defaultScene.value_or(0);
    for (const std::size_t root : asset.scenes[scene_index].nodeIndices) {
        add_node(asset, root, target, scene::NodeId{}, meshes);
    }

    return true;
}

}  // namespace kinetiqra::io
