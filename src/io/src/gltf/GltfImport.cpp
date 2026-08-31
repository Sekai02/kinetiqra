#include <kinetiqra/geom/Tangents.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
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
    std::vector<math::Vec4> tangents;
    std::vector<math::Vec4> joints;
    std::vector<math::Vec4> weights;
    std::vector<std::uint32_t> indices;

    // Which of the file's materials paints this primitive. Every primitive of a
    // mesh melts into one EditMesh, so this is what has to survive as a face
    // attribute or the distinction is lost.
    std::uint32_t material{0};

    [[nodiscard]] bool skinned() const { return !joints.empty() && !weights.empty(); }
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

    // Only TEXCOORD_0. A model can carry several sets, and the juggernaut
    // carries three, but a material asking for the second or third gets the
    // first until there is a reason to keep them all.
    if (const auto* uvs = primitive.findAttribute("TEXCOORD_0");
        uvs != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[uvs->accessorIndex];
        out.uvs.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<math::Vec2>(
            asset, accessor, [&](math::Vec2 value, std::size_t index) { out.uvs[index] = value; });
    }

    // Read when the file has it and computed later when it does not, which is
    // the common case: the tangent is only needed by a normal map and plenty of
    // exporters leave it out.
    if (const auto* tangents = primitive.findAttribute("TANGENT");
        tangents != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[tangents->accessorIndex];
        out.tangents.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<math::Vec4>(
            asset, accessor,
            [&](math::Vec4 value, std::size_t index) { out.tangents[index] = value; });
    }

    // Joint indices arrive as bytes or shorts depending on how many joints the
    // rig has; asking for u32vec4 lets fastgltf widen either one.
    if (const auto* joints = primitive.findAttribute("JOINTS_0");
        joints != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[joints->accessorIndex];
        out.joints.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<glm::u32vec4>(
            asset, accessor, [&](glm::u32vec4 value, std::size_t index) {
                out.joints[index] =
                    math::Vec4{static_cast<float>(value.x), static_cast<float>(value.y),
                               static_cast<float>(value.z), static_cast<float>(value.w)};
            });
    }

    if (const auto* weights = primitive.findAttribute("WEIGHTS_0");
        weights != primitive.attributes.end()) {
        const auto& accessor = asset.accessors[weights->accessorIndex];
        out.weights.resize(accessor.count);
        fastgltf::iterateAccessorWithIndex<math::Vec4>(
            asset, accessor,
            [&](math::Vec4 value, std::size_t index) { out.weights[index] = value; });
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

        // Skinning is per vertex, so it is written once, when the vertex is
        // created. Vertices that weld together necessarily agree on it: they
        // are the same point of the model, and a joint owns a point, not a
        // corner.
        if (data.skinned() && index < data.joints.size() && index < data.weights.size()) {
            const math::Vec4 weights = data.weights[index];

            // The specification allows weights that do not sum to one, and a
            // vertex whose weights sum to something else shrinks or flies off
            // when the skeleton moves.
            const float total = weights.x + weights.y + weights.z + weights.w;
            const math::Vec4 normalised =
                total > 0.0F ? weights / total : math::Vec4{1.0F, 0.0F, 0.0F, 0.0F};

            mesh.set_skinning(id, data.joints[index], normalised);
        }

        return id;
    };

    for (std::size_t triangle = 0; triangle + 2 < data.indices.size(); triangle += 3) {
        const std::uint32_t a = data.indices[triangle];
        const std::uint32_t b = data.indices[triangle + 1];
        const std::uint32_t c = data.indices[triangle + 2];

        std::vector<geom::CornerId> corners;
        const geom::FaceId face =
            mesh.add_face({vertex_for(a), vertex_for(b), vertex_for(c)}, &corners);

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

        mesh.set_material(face, data.material);

        const std::array<std::uint32_t, 3> source{a, b, c};
        for (std::size_t corner = 0; corner < corners.size(); ++corner) {
            const std::uint32_t index = source[corner];

            mesh.set_normal(corners[corner], data.normals.empty() ? flat : data.normals[index]);

            if (!data.uvs.empty()) {
                mesh.set_uv(corners[corner], data.uvs[index]);
            }
            if (!data.tangents.empty()) {
                mesh.set_tangent(corners[corner], data.tangents[index]);
            }
        }
    }
}

scene::Wrap as_wrap(fastgltf::Wrap source) {
    switch (source) {
        case fastgltf::Wrap::ClampToEdge:
            return scene::Wrap::ClampToEdge;
        case fastgltf::Wrap::MirroredRepeat:
            return scene::Wrap::MirroredRepeat;
        case fastgltf::Wrap::Repeat:
            break;
    }
    return scene::Wrap::Repeat;
}

// The bytes of an image, wherever the file happens to keep them.
//
// Three shapes, and a model in the wild will use any of them: inside a buffer
// view, which is what a self-contained .glb does; already loaded, which is what
// fastgltf hands back for a data URI or for an external file it was asked to
// read; and a path it has not been asked to read, which is why the parser is
// given LoadExternalImages.
std::vector<std::uint8_t> image_bytes(const fastgltf::Asset& asset, const fastgltf::Image& image) {
    if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data);
        view != nullptr) {
        const fastgltf::BufferView& buffer_view = asset.bufferViews[view->bufferViewIndex];
        const fastgltf::Buffer& buffer = asset.buffers[buffer_view.bufferIndex];

        const auto copy = [&](const std::byte* bytes) {
            const std::byte* start = bytes + buffer_view.byteOffset;
            return std::vector<std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(start),
                reinterpret_cast<const std::uint8_t*>(start + buffer_view.byteLength));
        };

        if (const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data);
            array != nullptr) {
            return copy(array->bytes.data());
        }
        if (const auto* vector = std::get_if<fastgltf::sources::Vector>(&buffer.data);
            vector != nullptr) {
            return copy(vector->bytes.data());
        }
        if (const auto* bytes = std::get_if<fastgltf::sources::ByteView>(&buffer.data);
            bytes != nullptr) {
            return copy(bytes->bytes.data());
        }
        return {};
    }

    if (const auto* array = std::get_if<fastgltf::sources::Array>(&image.data); array != nullptr) {
        return std::vector<std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(array->bytes.data()),
            reinterpret_cast<const std::uint8_t*>(array->bytes.data() + array->bytes.size()));
    }

    if (const auto* vector = std::get_if<fastgltf::sources::Vector>(&image.data);
        vector != nullptr) {
        return std::vector<std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(vector->bytes.data()),
            reinterpret_cast<const std::uint8_t*>(vector->bytes.data() + vector->bytes.size()));
    }

    return {};
}

std::string mime_type_of(const fastgltf::Asset& asset, const fastgltf::Image& image) {
    fastgltf::MimeType mime = fastgltf::MimeType::None;

    if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data);
        view != nullptr) {
        mime = view->mimeType;
    } else if (const auto* array = std::get_if<fastgltf::sources::Array>(&image.data);
               array != nullptr) {
        mime = array->mimeType;
    } else if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data); uri != nullptr) {
        mime = uri->mimeType;
    }

    (void)asset;

    switch (mime) {
        case fastgltf::MimeType::JPEG:
            return "image/jpeg";
        case fastgltf::MimeType::PNG:
            return "image/png";
        default:
            break;
    }

    // PNG when the file did not say. Every decoder sniffs the header anyway,
    // and this is only carried so that an export can declare something.
    return "image/png";
}

// Turns a glTF texture reference into ours, collapsing the sampler into the two
// settings that differ between files.
scene::Texture as_texture(const fastgltf::Asset& asset, const fastgltf::TextureInfo& info,
                          const std::vector<scene::ImageId>& images) {
    scene::Texture texture;

    if (info.textureIndex >= asset.textures.size()) {
        return texture;
    }

    const fastgltf::Texture& source = asset.textures[info.textureIndex];
    if (source.imageIndex.has_value() && source.imageIndex.value() < images.size()) {
        texture.image = images[source.imageIndex.value()];
    }

    if (source.samplerIndex.has_value() && source.samplerIndex.value() < asset.samplers.size()) {
        const fastgltf::Sampler& sampler = asset.samplers[source.samplerIndex.value()];
        texture.wrap_s = as_wrap(sampler.wrapS);
        texture.wrap_t = as_wrap(sampler.wrapT);
    }

    return texture;
}

// Reads the materials and the pictures they use.
//
// Images are kept exactly as they arrived rather than decoded, so that an
// export can write the same bytes back and the engine needs no image encoder.
std::vector<scene::MaterialId> add_materials(const fastgltf::Asset& asset, scene::Scene& target) {
    std::vector<scene::ImageId> images;
    images.reserve(asset.images.size());

    for (const fastgltf::Image& source : asset.images) {
        scene::Image image;
        image.name = std::string(source.name);
        image.mime_type = mime_type_of(asset, source);
        image.bytes = image_bytes(asset, source);

        images.push_back(target.add_image(std::move(image)));
    }

    std::vector<scene::MaterialId> materials;
    materials.reserve(asset.materials.size());

    for (const fastgltf::Material& source : asset.materials) {
        scene::Material material;
        material.name = std::string(source.name);

        const fastgltf::PBRData& pbr = source.pbrData;
        material.base_colour = math::Vec4{pbr.baseColorFactor[0], pbr.baseColorFactor[1],
                                          pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
        material.metallic = pbr.metallicFactor;
        material.roughness = pbr.roughnessFactor;
        material.emissive = math::Vec3{source.emissiveFactor[0], source.emissiveFactor[1],
                                       source.emissiveFactor[2]};

        material.double_sided = source.doubleSided;
        material.alpha_cutoff = source.alphaCutoff;

        switch (source.alphaMode) {
            case fastgltf::AlphaMode::Mask:
                material.alpha_mode = scene::AlphaMode::Mask;
                break;
            case fastgltf::AlphaMode::Blend:
                material.alpha_mode = scene::AlphaMode::Blend;
                break;
            case fastgltf::AlphaMode::Opaque:
                break;
        }

        if (pbr.baseColorTexture.has_value()) {
            material.base_colour_texture = as_texture(asset, pbr.baseColorTexture.value(), images);
        }
        if (pbr.metallicRoughnessTexture.has_value()) {
            material.metallic_roughness_texture =
                as_texture(asset, pbr.metallicRoughnessTexture.value(), images);
        }
        if (source.normalTexture.has_value()) {
            material.normal_texture = as_texture(asset, source.normalTexture.value(), images);
            material.normal_scale = source.normalTexture.value().scale;
        }
        if (source.occlusionTexture.has_value()) {
            material.occlusion_texture = as_texture(asset, source.occlusionTexture.value(), images);
            material.occlusion_strength = source.occlusionTexture.value().strength;
        }
        if (source.emissiveTexture.has_value()) {
            material.emissive_texture = as_texture(asset, source.emissiveTexture.value(), images);
        }

        materials.push_back(target.add_material(std::move(material)));
    }

    return materials;
}

scene::NodeId add_node(const fastgltf::Asset& asset, std::size_t node_index, scene::Scene& target,
                       scene::NodeId parent, const std::vector<scene::MeshId>& meshes,
                       std::vector<scene::NodeId>& by_index) {
    const fastgltf::Node& source = asset.nodes[node_index];

    const scene::NodeId id = target.add_node(std::string(source.name), parent);
    scene::Node* node = target.node(id);

    // Skins refer to joints by glTF node index, so the mapping has to survive
    // past this walk.
    if (node_index < by_index.size()) {
        by_index[node_index] = id;
    }

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
        add_node(asset, child, target, id, meshes, by_index);
    }

    return id;
}

// Reads the skins once the nodes exist, then attaches each one to the node that
// carries the skinned mesh.
void add_skins(const fastgltf::Asset& asset, scene::Scene& target,
               const std::vector<scene::NodeId>& by_index) {
    std::vector<scene::SkinId> skins;
    skins.reserve(asset.skins.size());

    for (const fastgltf::Skin& source : asset.skins) {
        scene::Skin skin;
        skin.joints.reserve(source.joints.size());

        for (const std::size_t joint : source.joints) {
            skin.joints.push_back(joint < by_index.size() ? by_index[joint] : scene::NodeId{});
        }

        if (source.inverseBindMatrices.has_value()) {
            const auto& accessor = asset.accessors[source.inverseBindMatrices.value()];
            skin.inverse_bind.resize(accessor.count);
            fastgltf::iterateAccessorWithIndex<math::Mat4>(
                asset, accessor,
                [&](math::Mat4 value, std::size_t index) { skin.inverse_bind[index] = value; });
        } else {
            // The specification says an absent accessor means every matrix is
            // the identity, which is the case for a mesh already modelled in
            // each joint's own space.
            skin.inverse_bind.assign(skin.joints.size(), math::Mat4{1.0F});
        }

        skins.push_back(target.add_skin(std::move(skin)));
    }

    for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
        const fastgltf::Node& source = asset.nodes[index];
        if (!source.skinIndex.has_value() || index >= by_index.size()) {
            continue;
        }

        const std::size_t skin_index = source.skinIndex.value();
        if (skin_index < skins.size()) {
            target.set_skin(by_index[index], skins[skin_index]);
        }
    }
}

anim::Interpolation as_interpolation(fastgltf::AnimationInterpolation source) {
    switch (source) {
        case fastgltf::AnimationInterpolation::Step:
            return anim::Interpolation::Step;
        case fastgltf::AnimationInterpolation::CubicSpline:
            return anim::Interpolation::CubicSpline;
        case fastgltf::AnimationInterpolation::Linear:
            break;
    }
    return anim::Interpolation::Linear;
}

// Reads the clips once the nodes exist, since a channel names its target by
// node index just as a skin names its joints.
void add_clips(const fastgltf::Asset& asset, const std::vector<scene::NodeId>& by_index,
               std::vector<anim::Clip>& clips) {
    clips.reserve(asset.animations.size());

    for (const fastgltf::Animation& source : asset.animations) {
        anim::Clip clip;
        clip.name = std::string(source.name);
        clip.samplers.reserve(source.samplers.size());

        for (const fastgltf::AnimationSampler& sampler : source.samplers) {
            anim::Sampler out;
            out.interpolation = as_interpolation(sampler.interpolation);

            const auto& times = asset.accessors[sampler.inputAccessor];
            out.times.resize(times.count);
            fastgltf::iterateAccessorWithIndex<float>(
                asset, times, [&](float value, std::size_t index) { out.times[index] = value; });

            // Translations and scales are three components and rotations four,
            // and both are kept as Vec4 so one sampler serves either.
            //
            // Morph target weights are scalars. Their channels are dropped
            // below, but the sampler is still read so that the sampler indices
            // the channels refer to keep lining up with the file. Reading a
            // scalar accessor as a vector trips an assertion inside fastgltf
            // rather than returning something wrong, so the type is checked
            // here instead.
            const auto& values = asset.accessors[sampler.outputAccessor];
            out.values.assign(values.count, math::Vec4{0.0F, 0.0F, 0.0F, 0.0F});

            switch (values.type) {
                case fastgltf::AccessorType::Vec4:
                    fastgltf::iterateAccessorWithIndex<math::Vec4>(
                        asset, values,
                        [&](math::Vec4 value, std::size_t index) { out.values[index] = value; });
                    break;
                case fastgltf::AccessorType::Vec3:
                    fastgltf::iterateAccessorWithIndex<math::Vec3>(
                        asset, values, [&](math::Vec3 value, std::size_t index) {
                            out.values[index] = math::Vec4{value, 0.0F};
                        });
                    break;
                case fastgltf::AccessorType::Scalar:
                    fastgltf::iterateAccessorWithIndex<float>(
                        asset, values, [&](float value, std::size_t index) {
                            out.values[index] = math::Vec4{value, 0.0F, 0.0F, 0.0F};
                        });
                    break;
                default:
                    // Left at zero rather than guessed at. No channel the
                    // importer keeps uses any other shape.
                    break;
            }

            if (!out.times.empty()) {
                clip.duration = std::max(clip.duration, out.times.back());
            }

            clip.samplers.push_back(std::move(out));
        }

        for (const fastgltf::AnimationChannel& channel : source.channels) {
            if (!channel.nodeIndex.has_value()) {
                continue;
            }

            anim::Channel out;
            out.sampler = channel.samplerIndex;

            const std::size_t node = channel.nodeIndex.value();
            out.target = node < by_index.size() ? by_index[node] : scene::NodeId{};

            switch (channel.path) {
                case fastgltf::AnimationPath::Translation:
                    out.path = anim::Path::Translation;
                    break;
                case fastgltf::AnimationPath::Rotation:
                    out.path = anim::Path::Rotation;
                    break;
                case fastgltf::AnimationPath::Scale:
                    out.path = anim::Path::Scale;
                    break;
                default:
                    // Morph target weights, which there is nothing to drive yet.
                    continue;
            }

            clip.channels.push_back(out);
        }

        clips.push_back(std::move(clip));
    }
}

}  // namespace

bool import_gltf(const std::filesystem::path& path, scene::Scene& target, std::string& error,
                 std::vector<anim::Clip>* clips) {
    target.clear();
    if (clips != nullptr) {
        clips->clear();
    }

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
    constexpr auto kOptions =
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
        fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::GenerateMeshIndices;

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

    // Before the meshes, because a face records its material by index and the
    // indices are the order these are added in.
    add_materials(asset, target);

    std::vector<scene::MeshId> meshes;
    meshes.reserve(asset.meshes.size());

    for (const fastgltf::Mesh& source : asset.meshes) {
        geom::EditMesh mesh;

        // Shared across the primitives of one glTF mesh, so that a model split
        // by material still welds along the seams between those parts.
        std::unordered_map<PositionKey, geom::VertexId, PositionHash> welded;

        bool tangents_needed = false;

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

            // A primitive without a material takes the first one, which is what
            // the specification's default amounts to once a file has any.
            primitive_data.material =
                primitive.materialIndex.has_value()
                    ? static_cast<std::uint32_t>(primitive.materialIndex.value())
                    : 0;

            append_primitive(primitive_data, mesh, welded);
            tangents_needed = tangents_needed || primitive_data.tangents.empty();
        }

        // Only for the meshes that arrived without them. A file that carries
        // its own tangents knows better than we can work out, since it may have
        // been baked against the very normal map it ships with.
        if (tangents_needed) {
            geom::compute_tangents(mesh);
        }

        meshes.push_back(target.add_mesh(std::move(mesh)));
    }

    std::vector<scene::NodeId> by_index(asset.nodes.size());

    if (asset.scenes.empty()) {
        for (std::size_t index = 0; index < asset.nodes.size(); ++index) {
            add_node(asset, index, target, scene::NodeId{}, meshes, by_index);
        }
    } else {
        const std::size_t scene_index = asset.defaultScene.value_or(0);
        for (const std::size_t root : asset.scenes[scene_index].nodeIndices) {
            add_node(asset, root, target, scene::NodeId{}, meshes, by_index);
        }
    }

    // After the nodes, because a skin names its joints by node index and those
    // handles do not exist until the tree has been walked.
    add_skins(asset, target, by_index);

    if (clips != nullptr) {
        add_clips(asset, by_index, *clips);
    }

    return true;
}

}  // namespace kinetiqra::io
