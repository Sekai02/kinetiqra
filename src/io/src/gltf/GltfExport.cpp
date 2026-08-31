#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/io/gltf/GltfExport.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kinetiqra::io {

namespace {

// glTF is right-handed, Y-up and in metres, which is what the engine uses, so
// there is nothing to convert on the way out either. The seam still belongs
// here, on the same side of the wall as the importer's.

constexpr std::size_t kAbsent = static_cast<std::size_t>(-1);

// Joint indices are written as unsigned shorts, so a rig would have to reach
// this many joints before an index stopped fitting. The renderer gives up at
// 256, and no humanoid comes close to either number.
constexpr float kMaxJointIndex = 65535.0F;

bool all_zero(const std::vector<float>& values) {
    return std::all_of(values.begin(), values.end(), [](float value) { return value == 0.0F; });
}

// The asset being built, and the one buffer everything is written into.
//
// Each attribute gets its own view rather than being interleaved. glTF allows
// either, and separate views read far better in the JSON, which is what the
// `.gltf` container is for.
class Writer {
public:
    fastgltf::Asset asset;

    std::size_t add_view(const void* data, std::size_t byte_length,
                         fastgltf::Optional<fastgltf::BufferTarget> target) {
        // Views start on a four byte boundary, which covers the alignment every
        // component type written here asks for and lets accessors sit at
        // offset zero within their view.
        while (binary_.size() % 4 != 0) {
            binary_.push_back(std::byte{0});
        }

        const std::size_t offset = binary_.size();
        const auto* bytes = static_cast<const std::byte*>(data);
        binary_.insert(binary_.end(), bytes, bytes + byte_length);

        fastgltf::BufferView view;
        view.bufferIndex = 0;
        view.byteOffset = offset;
        view.byteLength = byte_length;
        view.target = target;

        asset.bufferViews.emplace_back(std::move(view));
        return asset.bufferViews.size() - 1;
    }

    // `bounds` writes the per-component minimum and maximum the specification
    // requires on positions and on animation keyframe times.
    std::size_t add_floats(const std::vector<float>& values, std::size_t components,
                           fastgltf::AccessorType type, bool bounds,
                           fastgltf::Optional<fastgltf::BufferTarget> target = {}) {
        fastgltf::Accessor accessor;
        accessor.bufferViewIndex = add_view(values.data(), values.size() * sizeof(float), target);
        accessor.count = values.size() / components;
        accessor.type = type;
        accessor.componentType = fastgltf::ComponentType::Float;

        if (bounds && accessor.count > 0) {
            auto minimum = fastgltf::AccessorBoundsArray::ForType<double>(components);
            auto maximum = fastgltf::AccessorBoundsArray::ForType<double>(components);

            for (std::size_t component = 0; component < components; ++component) {
                minimum.set<double>(component, static_cast<double>(values[component]));
                maximum.set<double>(component, static_cast<double>(values[component]));
            }

            for (std::size_t element = 1; element < accessor.count; ++element) {
                for (std::size_t component = 0; component < components; ++component) {
                    const auto value =
                        static_cast<double>(values[(element * components) + component]);
                    minimum.set<double>(component, std::min(minimum.get<double>(component), value));
                    maximum.set<double>(component, std::max(maximum.get<double>(component), value));
                }
            }

            accessor.min = std::move(minimum);
            accessor.max = std::move(maximum);
        }

        asset.accessors.emplace_back(std::move(accessor));
        return asset.accessors.size() - 1;
    }

    std::size_t add_joints(const std::vector<std::uint16_t>& values) {
        fastgltf::Accessor accessor;
        accessor.bufferViewIndex = add_view(values.data(), values.size() * sizeof(std::uint16_t),
                                            fastgltf::BufferTarget::ArrayBuffer);
        accessor.count = values.size() / 4;
        accessor.type = fastgltf::AccessorType::Vec4;
        accessor.componentType = fastgltf::ComponentType::UnsignedShort;

        asset.accessors.emplace_back(std::move(accessor));
        return asset.accessors.size() - 1;
    }

    std::size_t add_indices(const std::vector<std::uint32_t>& values) {
        fastgltf::Accessor accessor;
        accessor.bufferViewIndex = add_view(values.data(), values.size() * sizeof(std::uint32_t),
                                            fastgltf::BufferTarget::ElementArrayBuffer);
        accessor.count = values.size();
        accessor.type = fastgltf::AccessorType::Scalar;
        accessor.componentType = fastgltf::ComponentType::UnsignedInt;

        asset.accessors.emplace_back(std::move(accessor));
        return asset.accessors.size() - 1;
    }

    // Hands the accumulated bytes over as the asset's single buffer. An empty
    // scene writes no buffer at all, since a buffer of nothing is not a valid
    // one.
    void finish(const std::string& name) {
        if (binary_.empty()) {
            return;
        }

        fastgltf::Buffer buffer;
        buffer.byteLength = binary_.size();
        buffer.name = name.c_str();
        buffer.data = fastgltf::sources::Vector{std::move(binary_)};

        asset.buffers.emplace_back(std::move(buffer));
    }

private:
    std::vector<std::byte> binary_;
};

// Splits the baked, interleaved vertices back into one array per attribute and
// writes each as its own accessor. Returns the mesh's index, or kAbsent when
// there was nothing to draw.
//
// One primitive per material, which undoes what the importer does: it melts
// every primitive of a mesh into one editable mesh, and this puts the seams
// back where the materials say they belong. The vertex arrays are shared
// between them, so only the indices are split.
std::size_t add_mesh(Writer& writer, const geom::EditMesh& source, const std::string& name,
                     const std::vector<std::size_t>& materials) {
    const geom::BakedMesh baked = geom::bake(source);
    if (baked.indices.empty()) {
        // A mesh with no faces has no primitive to write, and glTF has no way
        // to spell an empty one.
        return kAbsent;
    }

    const std::size_t stride = baked.floats_per_vertex();
    const std::size_t count = baked.vertex_count();

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<float> tangents;
    std::vector<float> weights;
    std::vector<std::uint16_t> joints;

    positions.reserve(count * 3);
    normals.reserve(count * 3);
    uvs.reserve(count * 2);
    tangents.reserve(count * 4);

    if (baked.skinned) {
        joints.reserve(count * 4);
        weights.reserve(count * 4);
    }

    for (std::size_t vertex = 0; vertex < count; ++vertex) {
        const float* data = &baked.vertices[vertex * stride];

        positions.insert(positions.end(), data, data + 3);
        normals.insert(normals.end(), data + 3, data + 6);
        uvs.insert(uvs.end(), data + 6, data + 8);
        tangents.insert(tangents.end(), data + 8, data + 12);

        if (baked.skinned) {
            for (std::size_t joint = 0; joint < 4; ++joint) {
                const float index = std::clamp(data[12 + joint], 0.0F, kMaxJointIndex);
                joints.push_back(static_cast<std::uint16_t>(index));
            }
            weights.insert(weights.end(), data + 16, data + 20);
        }
    }

    // Written once and pointed at by every primitive of this mesh, since the
    // split is only ever in the indices.
    //
    // Positions carry bounds because the specification asks for them, and a
    // viewer that culls by bounding box draws nothing without them.
    const std::size_t position_accessor = writer.add_floats(
        positions, 3, fastgltf::AccessorType::Vec3, true, fastgltf::BufferTarget::ArrayBuffer);

    // A channel that was never written holds zeros, and zeros are not a normal,
    // a texture coordinate or a tangent. Leaving the attribute out says so,
    // rather than shipping a mesh that claims to have one.
    const std::size_t normal_accessor =
        all_zero(normals) ? kAbsent
                          : writer.add_floats(normals, 3, fastgltf::AccessorType::Vec3, false,
                                              fastgltf::BufferTarget::ArrayBuffer);

    const std::size_t uv_accessor =
        all_zero(uvs) ? kAbsent
                      : writer.add_floats(uvs, 2, fastgltf::AccessorType::Vec2, false,
                                          fastgltf::BufferTarget::ArrayBuffer);

    const std::size_t tangent_accessor =
        all_zero(tangents) ? kAbsent
                           : writer.add_floats(tangents, 4, fastgltf::AccessorType::Vec4, false,
                                               fastgltf::BufferTarget::ArrayBuffer);

    const std::size_t joint_accessor = baked.skinned ? writer.add_joints(joints) : kAbsent;
    const std::size_t weight_accessor =
        baked.skinned ? writer.add_floats(weights, 4, fastgltf::AccessorType::Vec4, false,
                                          fastgltf::BufferTarget::ArrayBuffer)
                      : kAbsent;

    fastgltf::Mesh mesh;
    mesh.name = name.c_str();

    for (const geom::Section& section : baked.sections) {
        if (section.index_count == 0) {
            continue;
        }

        fastgltf::Primitive primitive;
        primitive.type = fastgltf::PrimitiveType::Triangles;

        primitive.attributes.emplace_back(fastgltf::Attribute{"POSITION", position_accessor});
        if (normal_accessor != kAbsent) {
            primitive.attributes.emplace_back(fastgltf::Attribute{"NORMAL", normal_accessor});
        }
        if (uv_accessor != kAbsent) {
            primitive.attributes.emplace_back(fastgltf::Attribute{"TEXCOORD_0", uv_accessor});
        }
        if (tangent_accessor != kAbsent) {
            primitive.attributes.emplace_back(fastgltf::Attribute{"TANGENT", tangent_accessor});
        }
        if (baked.skinned) {
            primitive.attributes.emplace_back(fastgltf::Attribute{"JOINTS_0", joint_accessor});
            primitive.attributes.emplace_back(fastgltf::Attribute{"WEIGHTS_0", weight_accessor});
        }

        const std::vector<std::uint32_t> indices(
            baked.indices.begin() + static_cast<std::ptrdiff_t>(section.first_index),
            baked.indices.begin() +
                static_cast<std::ptrdiff_t>(section.first_index + section.index_count));

        primitive.indicesAccessor = writer.add_indices(indices);

        if (section.material < materials.size()) {
            primitive.materialIndex = materials[section.material];
        }

        mesh.primitives.emplace_back(std::move(primitive));
    }

    if (mesh.primitives.empty()) {
        return kAbsent;
    }

    writer.asset.meshes.emplace_back(std::move(mesh));
    return writer.asset.meshes.size() - 1;
}

fastgltf::Wrap as_wrap(scene::Wrap source) {
    switch (source) {
        case scene::Wrap::ClampToEdge:
            return fastgltf::Wrap::ClampToEdge;
        case scene::Wrap::MirroredRepeat:
            return fastgltf::Wrap::MirroredRepeat;
        case scene::Wrap::Repeat:
            break;
    }
    return fastgltf::Wrap::Repeat;
}

fastgltf::MimeType as_mime(const std::string& mime_type) {
    return mime_type == "image/jpeg" ? fastgltf::MimeType::JPEG : fastgltf::MimeType::PNG;
}

// Writes the materials and the pictures they use, and reports where each
// material landed so that a primitive can point at it.
//
// Images go out as the very bytes they came in as. Keeping them encoded all the
// way through is what makes that possible, and it means a PNG survives the trip
// untouched and the engine needs no image encoder at all.
std::vector<std::size_t> add_materials(Writer& writer, const scene::Scene& scene) {
    std::unordered_map<std::uint32_t, std::size_t> images;

    const auto image_index = [&](scene::ImageId id) {
        if (const auto found = images.find(id.index); found != images.end()) {
            return found->second;
        }

        const scene::Image* source = scene.image(id);
        if (source == nullptr || source->empty()) {
            return kAbsent;
        }

        fastgltf::Image image;
        image.name = source->name.c_str();
        image.data = fastgltf::sources::BufferView{
            writer.add_view(source->bytes.data(), source->bytes.size(), {}),
            as_mime(source->mime_type)};

        writer.asset.images.emplace_back(std::move(image));

        const std::size_t index = writer.asset.images.size() - 1;
        images.emplace(id.index, index);
        return index;
    };

    // Textures are written per use rather than shared. A material naming the
    // same picture twice writes two of them, which is a handful of bytes of
    // JSON and saves keeping a table keyed by the pair of image and sampler.
    const auto texture_index = [&](const scene::Texture& texture) {
        if (!texture.valid()) {
            return kAbsent;
        }

        const std::size_t image = image_index(texture.image);
        if (image == kAbsent) {
            return kAbsent;
        }

        fastgltf::Sampler sampler;
        sampler.wrapS = as_wrap(texture.wrap_s);
        sampler.wrapT = as_wrap(texture.wrap_t);
        sampler.minFilter = fastgltf::Filter::LinearMipMapLinear;
        sampler.magFilter = fastgltf::Filter::Linear;
        writer.asset.samplers.emplace_back(std::move(sampler));

        fastgltf::Texture out;
        out.imageIndex = image;
        out.samplerIndex = writer.asset.samplers.size() - 1;
        writer.asset.textures.emplace_back(std::move(out));

        return writer.asset.textures.size() - 1;
    };

    const auto texture_info = [&](const scene::Texture& texture) {
        fastgltf::Optional<fastgltf::TextureInfo> info;
        if (const std::size_t index = texture_index(texture); index != kAbsent) {
            fastgltf::TextureInfo used;
            used.textureIndex = index;
            info = std::move(used);
        }
        return info;
    };

    std::vector<std::size_t> written;
    written.reserve(scene.material_count());

    for (const scene::MaterialId id : scene.materials()) {
        const scene::Material* source = scene.material(id);
        if (source == nullptr) {
            written.push_back(kAbsent);
            continue;
        }

        fastgltf::Material material;
        material.name = source->name.c_str();
        material.doubleSided = source->double_sided;
        material.alphaCutoff = source->alpha_cutoff;

        switch (source->alpha_mode) {
            case scene::AlphaMode::Mask:
                material.alphaMode = fastgltf::AlphaMode::Mask;
                break;
            case scene::AlphaMode::Blend:
                material.alphaMode = fastgltf::AlphaMode::Blend;
                break;
            case scene::AlphaMode::Opaque:
                break;
        }

        material.pbrData.baseColorFactor =
            fastgltf::math::nvec4(source->base_colour.x, source->base_colour.y,
                                  source->base_colour.z, source->base_colour.w);
        material.pbrData.metallicFactor = source->metallic;
        material.pbrData.roughnessFactor = source->roughness;
        material.emissiveFactor =
            fastgltf::math::nvec3(source->emissive.x, source->emissive.y, source->emissive.z);

        material.pbrData.baseColorTexture = texture_info(source->base_colour_texture);
        material.pbrData.metallicRoughnessTexture =
            texture_info(source->metallic_roughness_texture);

        if (const std::size_t index = texture_index(source->normal_texture); index != kAbsent) {
            fastgltf::NormalTextureInfo normal;
            normal.textureIndex = index;
            normal.scale = source->normal_scale;
            material.normalTexture = std::move(normal);
        }

        if (const std::size_t index = texture_index(source->occlusion_texture); index != kAbsent) {
            fastgltf::OcclusionTextureInfo occlusion;
            occlusion.textureIndex = index;
            occlusion.strength = source->occlusion_strength;
            material.occlusionTexture = std::move(occlusion);
        }

        material.emissiveTexture = texture_info(source->emissive_texture);

        writer.asset.materials.emplace_back(std::move(material));
        written.push_back(writer.asset.materials.size() - 1);
    }

    return written;
}

fastgltf::AnimationInterpolation as_interpolation(anim::Interpolation source) {
    switch (source) {
        case anim::Interpolation::Step:
            return fastgltf::AnimationInterpolation::Step;
        case anim::Interpolation::CubicSpline:
            return fastgltf::AnimationInterpolation::CubicSpline;
        case anim::Interpolation::Linear:
            break;
    }
    return fastgltf::AnimationInterpolation::Linear;
}

fastgltf::AnimationPath as_path(anim::Path source) {
    switch (source) {
        case anim::Path::Rotation:
            return fastgltf::AnimationPath::Rotation;
        case anim::Path::Scale:
            return fastgltf::AnimationPath::Scale;
        case anim::Path::Translation:
            break;
    }
    return fastgltf::AnimationPath::Translation;
}

// Indices into the file, keyed by the handle's slot, which is what lets a node
// be named by the skin and the channels that point at it.
using IndexMap = std::unordered_map<std::uint32_t, std::size_t>;

std::size_t find(const IndexMap& map, std::uint32_t key) {
    const auto found = map.find(key);
    return found != map.end() ? found->second : kAbsent;
}

void add_animations(Writer& writer, const std::vector<anim::Clip>& clips, const IndexMap& nodes) {
    for (const anim::Clip& clip : clips) {
        fastgltf::Animation animation;
        animation.name = clip.name.c_str();

        // A sampler holds four components whichever path it drives, and glTF
        // wants three for a translation or a scale and four for a rotation.
        // The width therefore belongs to the channel, not to the sampler, and
        // a sampler shared by both would have to be written twice.
        std::unordered_map<std::size_t, std::size_t> written;

        for (const anim::Channel& channel : clip.channels) {
            const std::size_t node = find(nodes, channel.target.index);
            if (node == kAbsent || channel.sampler >= clip.samplers.size()) {
                continue;
            }

            const anim::Sampler& sampler = clip.samplers[channel.sampler];
            if (!sampler.valid()) {
                continue;
            }

            const std::size_t components = channel.path == anim::Path::Rotation ? 4 : 3;
            const std::size_t key = (channel.sampler * 2) + (components == 4 ? 1 : 0);

            std::size_t index = kAbsent;
            if (const auto found = written.find(key); found != written.end()) {
                index = found->second;
            } else {
                std::vector<float> values;
                values.reserve(sampler.values.size() * components);
                for (const math::Vec4& value : sampler.values) {
                    for (std::size_t component = 0; component < components; ++component) {
                        values.push_back(value[static_cast<int>(component)]);
                    }
                }

                fastgltf::AnimationSampler out;
                // Keyframe times carry bounds for the same reason positions do.
                out.inputAccessor =
                    writer.add_floats(sampler.times, 1, fastgltf::AccessorType::Scalar, true);
                out.outputAccessor = writer.add_floats(
                    values, components,
                    components == 4 ? fastgltf::AccessorType::Vec4 : fastgltf::AccessorType::Vec3,
                    false);
                out.interpolation = as_interpolation(sampler.interpolation);

                animation.samplers.emplace_back(out);
                index = animation.samplers.size() - 1;
                written.emplace(key, index);
            }

            fastgltf::AnimationChannel out;
            out.samplerIndex = index;
            out.nodeIndex = node;
            out.path = as_path(channel.path);

            animation.channels.emplace_back(out);
        }

        // A clip whose every channel pointed at a node that is no longer there
        // has nothing to say, and an animation without channels is not a valid
        // one.
        if (!animation.channels.empty()) {
            writer.asset.animations.emplace_back(std::move(animation));
        }
    }
}

void add_skins(Writer& writer, const scene::Scene& scene, const std::vector<scene::NodeId>& ordered,
               const IndexMap& nodes, IndexMap& skins) {
    for (const scene::NodeId id : ordered) {
        const scene::Node* node = scene.node(id);
        if (node == nullptr || !node->skin.valid() || skins.count(node->skin.index) != 0) {
            continue;
        }

        const scene::Skin* source = scene.skin(node->skin);
        if (source == nullptr || source->joints.empty()) {
            continue;
        }

        fastgltf::Skin skin;
        bool complete = true;

        for (const scene::NodeId joint : source->joints) {
            const std::size_t index = find(nodes, joint.index);
            if (index == kAbsent) {
                // A joint outside the tree would leave the skin pointing at
                // nothing. Dropping the skin loses the deformation; writing it
                // anyway would lose the file.
                complete = false;
                break;
            }
            skin.joints.emplace_back(index);
        }

        if (!complete) {
            continue;
        }

        std::vector<float> matrices;
        matrices.reserve(source->inverse_bind.size() * 16);
        for (const math::Mat4& matrix : source->inverse_bind) {
            // Both sides are column major, so the columns go out in order.
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 4; ++row) {
                    matrices.push_back(matrix[column][row]);
                }
            }
        }

        if (!matrices.empty()) {
            skin.inverseBindMatrices =
                writer.add_floats(matrices, 16, fastgltf::AccessorType::Mat4, false);
        }

        writer.asset.skins.emplace_back(std::move(skin));
        skins.emplace(node->skin.index, writer.asset.skins.size() - 1);
    }
}

bool wants_binary(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".glb";
}

}  // namespace

bool export_gltf(const std::filesystem::path& path, const scene::Scene& scene,
                 const std::vector<anim::Clip>& clips, std::string& error) {
    Writer writer;

    fastgltf::AssetInfo info;
    info.gltfVersion = "2.0";
    info.generator = "kinetiqra";
    writer.asset.assetInfo = std::move(info);

    // Before the meshes, since a primitive names the material it is painted by.
    const std::vector<std::size_t> materials = add_materials(writer, scene);

    IndexMap meshes;
    for (const scene::MeshId id : scene.meshes()) {
        const geom::EditMesh* mesh = scene.mesh(id);
        if (mesh == nullptr) {
            continue;
        }

        const std::size_t index =
            add_mesh(writer, *mesh, "mesh" + std::to_string(id.index), materials);
        if (index != kAbsent) {
            meshes.emplace(id.index, index);
        }
    }

    // Parents before children, so a node's index exists by the time anything
    // refers to it.
    const std::vector<scene::NodeId> ordered = scene.nodes_in_order();

    IndexMap nodes;
    for (const scene::NodeId id : ordered) {
        const scene::Node* node = scene.node(id);
        if (node == nullptr) {
            continue;
        }

        fastgltf::Node out;
        out.name = node->name.c_str();

        const scene::Transform& transform = node->transform;
        fastgltf::TRS trs;
        trs.translation = fastgltf::math::fvec3(transform.translation.x, transform.translation.y,
                                                transform.translation.z);
        trs.rotation = fastgltf::math::fquat(transform.rotation.x, transform.rotation.y,
                                             transform.rotation.z, transform.rotation.w);
        trs.scale = fastgltf::math::fvec3(transform.scale.x, transform.scale.y, transform.scale.z);
        out.transform = trs;

        if (const std::size_t mesh = find(meshes, node->mesh.index); mesh != kAbsent) {
            out.meshIndex = mesh;
        }

        writer.asset.nodes.emplace_back(std::move(out));
        nodes.emplace(id.index, writer.asset.nodes.size() - 1);
    }

    // A second pass, because a child's index is only known once every node has
    // one.
    for (const scene::NodeId id : ordered) {
        const scene::Node* node = scene.node(id);
        const std::size_t index = find(nodes, id.index);
        if (node == nullptr || index == kAbsent) {
            continue;
        }

        for (const scene::NodeId child : node->children) {
            if (const std::size_t found = find(nodes, child.index); found != kAbsent) {
                writer.asset.nodes[index].children.emplace_back(found);
            }
        }
    }

    IndexMap skins;
    add_skins(writer, scene, ordered, nodes, skins);

    for (const scene::NodeId id : ordered) {
        const scene::Node* node = scene.node(id);
        const std::size_t index = find(nodes, id.index);
        if (node == nullptr || index == kAbsent || !node->skin.valid()) {
            continue;
        }

        if (const std::size_t skin = find(skins, node->skin.index); skin != kAbsent) {
            writer.asset.nodes[index].skinIndex = skin;
        }
    }

    fastgltf::Scene root;
    root.name = "scene";
    for (const scene::NodeId id : scene.roots()) {
        if (const std::size_t index = find(nodes, id.index); index != kAbsent) {
            root.nodeIndices.emplace_back(index);
        }
    }

    writer.asset.scenes.emplace_back(std::move(root));
    writer.asset.defaultScene = 0;

    add_animations(writer, clips, nodes);

    writer.finish(path.stem().string());

    // fastgltf writes the buffer relative to the target's directory and refuses
    // an empty one, which is what a bare name such as "model.glb" produces. The
    // importer has the same fallback for the same reason.
    std::filesystem::path target = path;
    if (target.parent_path().empty()) {
        target = std::filesystem::current_path() / target;
    }

    // ValidateAsset checks what was built before any of it reaches the disk, so
    // a mistake here is reported here rather than in whichever engine the file
    // was meant for.
    fastgltf::FileExporter exporter;
    const fastgltf::Error result =
        wants_binary(target)
            ? exporter.writeGltfBinary(writer.asset, target, fastgltf::ExportOptions::ValidateAsset)
            : exporter.writeGltfJson(writer.asset, target,
                                     fastgltf::ExportOptions::ValidateAsset |
                                         fastgltf::ExportOptions::PrettyPrintJson);

    if (result != fastgltf::Error::None) {
        error = "could not write '" + path.string() +
                "': " + std::string(fastgltf::getErrorMessage(result));
        return false;
    }

    return true;
}

}  // namespace kinetiqra::io
