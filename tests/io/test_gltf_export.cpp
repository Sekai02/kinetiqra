#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/io/gltf/GltfExport.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace anim = kinetiqra::anim;
namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;
namespace scene = kinetiqra::scene;

using kinetiqra::io::export_gltf;
using kinetiqra::io::import_gltf;
using kinetiqra::scene::Scene;

namespace {

// A directory of its own for each case, so that the `.bin` a JSON export writes
// beside the file goes away with it whatever the exporter decides to call it.
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("kinetiqra-export-" + std::to_string(counter()++));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] std::filesystem::path file(const std::string& name) const { return path_ / name; }

private:
    static int& counter() {
        static int value = 0;
        return value;
    }

    std::filesystem::path path_;
};

const scene::Node* find_node(const scene::Scene& scene, const std::string& name) {
    for (const scene::NodeId id : scene.nodes_in_order()) {
        const scene::Node* node = scene.node(id);
        if (node != nullptr && node->name == name) {
            return node;
        }
    }
    return nullptr;
}

// A triangle bound to two joints, which is the smallest thing that exercises
// every part of a skin.
geom::EditMesh skinned_triangle() {
    geom::EditMesh mesh;

    const geom::VertexId a = mesh.add_vertex({0.0F, 0.0F, 0.0F});
    const geom::VertexId b = mesh.add_vertex({1.0F, 0.0F, 0.0F});
    const geom::VertexId c = mesh.add_vertex({0.0F, 1.0F, 0.0F});

    std::vector<geom::CornerId> corners;
    mesh.add_face({a, b, c}, &corners);
    for (const geom::CornerId corner : corners) {
        mesh.set_normal(corner, {0.0F, 0.0F, 1.0F});
    }

    mesh.set_skinning(a, {0.0F, 0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 0.0F});
    mesh.set_skinning(b, {1.0F, 0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 0.0F});
    mesh.set_skinning(c, {1.0F, 0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, 0.0F});

    return mesh;
}

}  // namespace

TEST_CASE("the nodes, their names and their hierarchy come back") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId root = written.add_node("root");
    const scene::NodeId child = written.add_node("child", root);
    written.node(child)->transform.translation = {1.0F, 2.0F, 3.0F};
    written.node(child)->transform.scale = {2.0F, 2.0F, 2.0F};

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("scene.gltf"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("scene.gltf"), read, error), error);

    CHECK(read.node_count() == 2);
    REQUIRE(read.roots().size() == 1);

    const scene::Node* imported_root = read.node(read.roots()[0]);
    REQUIRE(imported_root != nullptr);
    CHECK(imported_root->name == "root");
    REQUIRE(imported_root->children.size() == 1);

    const scene::Node* imported_child = read.node(imported_root->children[0]);
    REQUIRE(imported_child != nullptr);
    CHECK(imported_child->name == "child");
    CHECK(imported_child->transform.translation.x == doctest::Approx(1.0F));
    CHECK(imported_child->transform.translation.y == doctest::Approx(2.0F));
    CHECK(imported_child->transform.translation.z == doctest::Approx(3.0F));
    CHECK(imported_child->transform.scale.x == doctest::Approx(2.0F));
}

TEST_CASE("a rotation survives as a rotation rather than as a matrix") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId node = written.add_node("turned");
    // A quarter turn about Y, which is where a decomposition would show up as
    // drift if the transform went out as a matrix.
    written.node(node)->transform.rotation = math::Quat{0.70710678F, 0.0F, 0.70710678F, 0.0F};

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("scene.glb"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("scene.glb"), read, error), error);

    const scene::Node* imported = find_node(read, "turned");
    REQUIRE(imported != nullptr);
    CHECK(imported->transform.rotation.w == doctest::Approx(0.70710678F));
    CHECK(imported->transform.rotation.y == doctest::Approx(0.70710678F));
    CHECK(imported->transform.rotation.x == doctest::Approx(0.0F));
    CHECK(imported->transform.rotation.z == doctest::Approx(0.0F));
}

TEST_CASE("a box comes back as the same geometry, triangulated") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::MeshId mesh = written.add_mesh(geom::make_box());
    const scene::NodeId node = written.add_node("box");
    written.set_mesh(node, mesh);

    const geom::BakedMesh before = geom::bake(*written.mesh(mesh));

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("box.glb"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("box.glb"), read, error), error);

    const scene::Node* imported = find_node(read, "box");
    REQUIRE(imported != nullptr);

    const geom::EditMesh* editable = read.mesh(imported->mesh);
    REQUIRE(editable != nullptr);

    // The vertices weld back to the eight the box has, and the faces do not:
    // glTF has no quads, so the six the box was built with arrive as twelve
    // triangles. That is the loss this direction takes, and it is the reason a
    // native format will eventually exist.
    CHECK(editable->vertex_count() == 8);
    CHECK(editable->face_count() == 12);
    CHECK(editable->corner_count() == 36);
    CHECK(editable->validate().empty());

    // What the GPU is handed is unchanged, which is the part that has to be
    // exact: the same split vertices and the same triangles.
    const geom::BakedMesh after = geom::bake(*editable);
    CHECK(after.vertex_count() == before.vertex_count());
    CHECK(after.indices.size() == before.indices.size());
}

TEST_CASE("both containers write the same scene") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::MeshId mesh = written.add_mesh(geom::make_box());
    written.set_mesh(written.add_node("box"), mesh);

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("box.gltf"), written, {}, error), error);
    REQUIRE_MESSAGE(export_gltf(directory.file("box.glb"), written, {}, error), error);

    Scene json;
    Scene binary;
    REQUIRE_MESSAGE(import_gltf(directory.file("box.gltf"), json, error), error);
    REQUIRE_MESSAGE(import_gltf(directory.file("box.glb"), binary, error), error);

    REQUIRE(json.mesh_count() == 1);
    REQUIRE(binary.mesh_count() == 1);
    CHECK(json.mesh(json.meshes()[0])->vertex_count() ==
          binary.mesh(binary.meshes()[0])->vertex_count());
}

TEST_CASE("a skin comes back with its joints and its bind matrices") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId root = written.add_node("armature");
    const scene::NodeId first = written.add_node("joint0", root);
    const scene::NodeId second = written.add_node("joint1", first);

    scene::Skin skin;
    skin.joints = {first, second};
    skin.inverse_bind = {math::Mat4{1.0F}, math::Mat4{1.0F}};
    // A translation, so that a matrix written by rows instead of columns would
    // come back in the wrong place rather than looking like the identity.
    skin.inverse_bind[1][3] = math::Vec4{0.0F, -1.0F, 0.0F, 1.0F};

    const scene::SkinId skin_id = written.add_skin(skin);
    REQUIRE(skin_id.valid());

    const scene::NodeId body = written.add_node("body", root);
    written.set_mesh(body, written.add_mesh(skinned_triangle()));
    written.set_skin(body, skin_id);

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("rig.glb"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("rig.glb"), read, error), error);

    const scene::Node* imported = find_node(read, "body");
    REQUIRE(imported != nullptr);
    REQUIRE(imported->skin.valid());

    const scene::Skin* imported_skin = read.skin(imported->skin);
    REQUIRE(imported_skin != nullptr);
    REQUIRE(imported_skin->joints.size() == 2);
    CHECK(read.node(imported_skin->joints[0])->name == "joint0");
    CHECK(read.node(imported_skin->joints[1])->name == "joint1");

    REQUIRE(imported_skin->inverse_bind.size() == 2);
    CHECK(imported_skin->inverse_bind[1][3].y == doctest::Approx(-1.0F));
    CHECK(imported_skin->inverse_bind[0][0].x == doctest::Approx(1.0F));
}

TEST_CASE("joint indices survive being written as unsigned shorts") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId root = written.add_node("armature");
    const scene::NodeId first = written.add_node("joint0", root);
    const scene::NodeId second = written.add_node("joint1", first);

    scene::Skin skin;
    skin.joints = {first, second};
    skin.inverse_bind = {math::Mat4{1.0F}, math::Mat4{1.0F}};

    const scene::NodeId body = written.add_node("body", root);
    written.set_mesh(body, written.add_mesh(skinned_triangle()));
    written.set_skin(body, written.add_skin(skin));

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("rig.glb"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("rig.glb"), read, error), error);

    const scene::Node* imported = find_node(read, "body");
    REQUIRE(imported != nullptr);

    const geom::EditMesh* mesh = read.mesh(imported->mesh);
    REQUIRE(mesh != nullptr);
    REQUIRE(mesh->skinned());

    const auto* positions =
        mesh->attributes().find<math::Vec3>(geom::kPosition, geom::Domain::Vertex);
    const auto* joints = mesh->attributes().find<math::Vec4>(geom::kJoints, geom::Domain::Vertex);
    const auto* weights = mesh->attributes().find<math::Vec4>(geom::kWeights, geom::Domain::Vertex);
    REQUIRE(positions != nullptr);
    REQUIRE(joints != nullptr);
    REQUIRE(weights != nullptr);

    // The vertex at the origin was bound to joint 0 and the other two to joint
    // 1, and an index that had been rounded or truncated would land elsewhere.
    bool seen_origin = false;
    for (std::size_t index = 0; index < mesh->vertex_count(); ++index) {
        const math::Vec3 position = (*positions)[index];
        const bool origin = position.x == 0.0F && position.y == 0.0F;

        CHECK((*joints)[index].x == doctest::Approx(origin ? 0.0F : 1.0F));
        CHECK((*weights)[index].x == doctest::Approx(1.0F));
        seen_origin = seen_origin || origin;
    }
    CHECK(seen_origin);
}

TEST_CASE("an unskinned mesh is written without joints") {
    const TemporaryDirectory directory;

    Scene written;
    written.set_mesh(written.add_node("box"), written.add_mesh(geom::make_box()));

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("box.glb"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("box.glb"), read, error), error);

    const scene::Node* imported = find_node(read, "box");
    REQUIRE(imported != nullptr);

    // An empty JOINTS_0 would have made the mesh claim a skin it does not have,
    // and the renderer picks its shader from exactly this.
    CHECK_FALSE(read.mesh(imported->mesh)->skinned());
    CHECK_FALSE(imported->skin.valid());
}

TEST_CASE("a clip's channels and keyframes survive the trip") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId node = written.add_node("mover");

    anim::Clip clip;
    clip.name = "walk";
    clip.duration = 2.0F;

    anim::Sampler translation;
    translation.times = {0.0F, 1.0F, 2.0F};
    translation.values = {
        {0.0F, 0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 0.0F}};
    translation.interpolation = anim::Interpolation::Linear;

    anim::Sampler rotation;
    rotation.times = {0.0F, 2.0F};
    // Keyframe values keep glTF's order, x, y, z, w, on both sides of the trip.
    rotation.values = {{0.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.70710678F, 0.0F, 0.70710678F}};
    rotation.interpolation = anim::Interpolation::Step;

    clip.samplers = {translation, rotation};
    clip.channels = {anim::Channel{node, anim::Path::Translation, 0},
                     anim::Channel{node, anim::Path::Rotation, 1}};

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("clip.glb"), written, {clip}, error), error);

    Scene read;
    std::vector<anim::Clip> clips;
    REQUIRE_MESSAGE(import_gltf(directory.file("clip.glb"), read, error, &clips), error);

    REQUIRE(clips.size() == 1);
    CHECK(clips[0].name == "walk");
    CHECK(clips[0].duration == doctest::Approx(2.0F));
    REQUIRE(clips[0].channels.size() == 2);

    const anim::Channel& first = clips[0].channels[0];
    CHECK(first.path == anim::Path::Translation);
    CHECK(read.node(first.target)->name == "mover");

    const anim::Sampler& imported_translation = clips[0].samplers[first.sampler];
    REQUIRE(imported_translation.times.size() == 3);
    CHECK(imported_translation.times[1] == doctest::Approx(1.0F));
    CHECK(imported_translation.values[1].y == doctest::Approx(1.0F));
    CHECK(imported_translation.interpolation == anim::Interpolation::Linear);

    const anim::Channel& second = clips[0].channels[1];
    CHECK(second.path == anim::Path::Rotation);

    const anim::Sampler& imported_rotation = clips[0].samplers[second.sampler];
    CHECK(imported_rotation.interpolation == anim::Interpolation::Step);
    REQUIRE(imported_rotation.values.size() == 2);
    CHECK(imported_rotation.values[1].y == doctest::Approx(0.70710678F));
    CHECK(imported_rotation.values[1].w == doctest::Approx(0.70710678F));
}

TEST_CASE("a cubic sampler keeps its three values per key") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId node = written.add_node("mover");

    anim::Clip clip;
    clip.name = "cubic";
    clip.duration = 1.0F;

    anim::Sampler sampler;
    sampler.interpolation = anim::Interpolation::CubicSpline;
    sampler.times = {0.0F, 1.0F};
    // In tangent, value, out tangent, for each of the two keys.
    sampler.values = {{0.0F, 0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F, 0.0F},
                      {0.0F, 2.0F, 0.0F, 0.0F}, {0.0F, 3.0F, 0.0F, 0.0F}, {0.0F, 4.0F, 0.0F, 0.0F}};

    clip.samplers = {sampler};
    clip.channels = {anim::Channel{node, anim::Path::Translation, 0}};

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("cubic.glb"), written, {clip}, error), error);

    Scene read;
    std::vector<anim::Clip> clips;
    REQUIRE_MESSAGE(import_gltf(directory.file("cubic.glb"), read, error, &clips), error);

    REQUIRE(clips.size() == 1);
    REQUIRE(clips[0].samplers.size() == 1);

    const anim::Sampler& imported = clips[0].samplers[0];
    CHECK(imported.interpolation == anim::Interpolation::CubicSpline);
    CHECK(imported.valid());
    REQUIRE(imported.values.size() == 6);
    CHECK(imported.values[3].y == doctest::Approx(2.0F));
}

TEST_CASE("one sampler driving two paths is written once for each width") {
    const TemporaryDirectory directory;

    Scene written;
    const scene::NodeId node = written.add_node("mover");

    anim::Clip clip;
    clip.name = "shared";
    clip.duration = 1.0F;

    anim::Sampler sampler;
    sampler.times = {0.0F, 1.0F};
    sampler.values = {{1.0F, 1.0F, 1.0F, 1.0F}, {2.0F, 2.0F, 2.0F, 1.0F}};

    clip.samplers = {sampler};
    // A translation takes three components and a rotation four, so the same
    // sampler cannot be one accessor in the file.
    clip.channels = {anim::Channel{node, anim::Path::Translation, 0},
                     anim::Channel{node, anim::Path::Scale, 0},
                     anim::Channel{node, anim::Path::Rotation, 0}};

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("shared.glb"), written, {clip}, error), error);

    Scene read;
    std::vector<anim::Clip> clips;
    REQUIRE_MESSAGE(import_gltf(directory.file("shared.glb"), read, error, &clips), error);

    REQUIRE(clips.size() == 1);
    CHECK(clips[0].channels.size() == 3);

    // The two three component channels share one sampler and the rotation gets
    // its own.
    CHECK(clips[0].samplers.size() == 2);
    CHECK(clips[0].channels[0].sampler == clips[0].channels[1].sampler);
    CHECK(clips[0].channels[2].sampler != clips[0].channels[0].sampler);

    const anim::Sampler& rotation = clips[0].samplers[clips[0].channels[2].sampler];
    REQUIRE(rotation.values.size() == 2);
    CHECK(rotation.values[1].w == doctest::Approx(1.0F));
}

TEST_CASE("an empty scene writes a file that reads back empty") {
    const TemporaryDirectory directory;

    const Scene written;
    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("empty.gltf"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("empty.gltf"), read, error), error);
    CHECK(read.node_count() == 0);
    CHECK(read.mesh_count() == 0);
}

TEST_CASE("a mesh with no faces leaves the node without one") {
    const TemporaryDirectory directory;

    Scene written;
    geom::EditMesh mesh;
    mesh.add_vertex({0.0F, 0.0F, 0.0F});
    written.set_mesh(written.add_node("lonely"), written.add_mesh(std::move(mesh)));

    std::string error;
    REQUIRE_MESSAGE(export_gltf(directory.file("lonely.gltf"), written, {}, error), error);

    Scene read;
    REQUIRE_MESSAGE(import_gltf(directory.file("lonely.gltf"), read, error), error);

    const scene::Node* imported = find_node(read, "lonely");
    REQUIRE(imported != nullptr);
    CHECK_FALSE(imported->mesh.valid());
}

TEST_CASE("a missing directory is created rather than refused") {
    const TemporaryDirectory directory;

    Scene written;
    written.set_mesh(written.add_node("box"), written.add_mesh(geom::make_box()));

    const std::filesystem::path nested = directory.file("exports") / "box.glb";

    std::string error;
    REQUIRE_MESSAGE(export_gltf(nested, written, {}, error), error);
    CHECK(std::filesystem::exists(nested));
}

TEST_CASE("a bare filename with no directory still writes") {
    // fastgltf resolves the buffer against the target's directory and refuses
    // an empty one, so "model.glb" failed while the same name with a directory
    // in front of it worked. Every other case here writes to a temporary
    // directory, which is exactly why this needs its own.
    Scene written;
    written.set_mesh(written.add_node("box"), written.add_mesh(geom::make_box()));

    const std::filesystem::path name = "kinetiqra-relative-export.glb";

    std::string error;
    const bool exported = export_gltf(name, written, {}, error);

    Scene read;
    const bool imported = exported && import_gltf(name, read, error);

    std::error_code ignored;
    std::filesystem::remove(name, ignored);

    REQUIRE_MESSAGE(exported, error);
    REQUIRE_MESSAGE(imported, error);
    CHECK(read.mesh_count() == 1);
}

TEST_CASE("a path that cannot be written is reported rather than passing") {
    const TemporaryDirectory directory;

    Scene written;
    written.set_mesh(written.add_node("box"), written.add_mesh(geom::make_box()));

    // A regular file where a directory would have to be, so that nothing along
    // the way can be created and the write has to fail.
    const std::filesystem::path blocker = directory.file("blocker");
    { const std::ofstream stream(blocker); }

    std::string error;
    CHECK_FALSE(export_gltf(blocker / "box.glb", written, {}, error));
    CHECK_FALSE(error.empty());
}
