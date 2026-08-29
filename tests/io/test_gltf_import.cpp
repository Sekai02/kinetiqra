#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/io/gltf/GltfImport.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using kinetiqra::geom::bake;
using kinetiqra::io::import_gltf;
using kinetiqra::scene::Scene;

namespace {

// A glTF written by hand into a temporary file, with the buffer inline as a
// base64 data URI. Keeping the fixtures in the test rather than committing
// binaries means each case says exactly what geometry it is about, and the
// repository gains no files nobody can read.
class TemporaryFile {
public:
    explicit TemporaryFile(const std::string& contents) {
        path_ = std::filesystem::temp_directory_path() /
                ("kinetiqra-test-" + std::to_string(counter()++) + ".gltf");
        std::ofstream stream(path_, std::ios::binary);
        stream << contents;
    }

    ~TemporaryFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&) = delete;
    TemporaryFile& operator=(TemporaryFile&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    static int& counter() {
        static int value = 0;
        return value;
    }

    std::filesystem::path path_;
};

std::string base64(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t remaining = static_cast<std::uint32_t>(bytes.size() - i);
        std::uint32_t block = static_cast<std::uint32_t>(bytes[i]) << 16;
        if (remaining > 1) {
            block |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
        }
        if (remaining > 2) {
            block |= static_cast<std::uint32_t>(bytes[i + 2]);
        }

        encoded += kAlphabet[(block >> 18) & 0x3F];
        encoded += kAlphabet[(block >> 12) & 0x3F];
        encoded += remaining > 1 ? kAlphabet[(block >> 6) & 0x3F] : '=';
        encoded += remaining > 2 ? kAlphabet[block & 0x3F] : '=';
    }
    return encoded;
}

void append(std::vector<std::uint8_t>& bytes, float value) {
    std::uint8_t raw[sizeof(float)];
    std::memcpy(raw, &value, sizeof(float));
    bytes.insert(bytes.end(), std::begin(raw), std::end(raw));
}

void append(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

// One triangle with an optional normal attribute.
std::string triangle_gltf(bool with_normals) {
    std::vector<std::uint8_t> buffer;

    // Positions, three vec3.
    append(buffer, 0.0F);
    append(buffer, 0.0F);
    append(buffer, 0.0F);
    append(buffer, 1.0F);
    append(buffer, 0.0F);
    append(buffer, 0.0F);
    append(buffer, 0.0F);
    append(buffer, 0.0F);
    append(buffer, 1.0F);

    const std::size_t normals_offset = buffer.size();
    if (with_normals) {
        for (int i = 0; i < 3; ++i) {
            append(buffer, 0.0F);
            append(buffer, 1.0F);
            append(buffer, 0.0F);
        }
    }

    const std::size_t indices_offset = buffer.size();
    append(buffer, static_cast<std::uint16_t>(0));
    append(buffer, static_cast<std::uint16_t>(1));
    append(buffer, static_cast<std::uint16_t>(2));

    std::string attributes = R"("POSITION": 0)";
    std::string extra_accessors;
    std::string extra_views;
    int index_accessor = 1;

    if (with_normals) {
        attributes += R"(, "NORMAL": 1)";
        index_accessor = 2;
        extra_accessors = R"(,
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" })";
        extra_views = R"(,
    { "buffer": 0, "byteOffset": )" +
                      std::to_string(normals_offset) + R"(, "byteLength": 36 })";
    }

    return R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "name": "triangle", "mesh": 0 } ],
  "meshes": [ { "primitives": [ { "attributes": { )" +
           attributes + R"( }, "indices": )" + std::to_string(index_accessor) + R"( } ] } ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [0,0,0], "max": [1,0,1] })" +
           extra_accessors + R"(,
    { "bufferView": )" +
           std::to_string(with_normals ? 2 : 1) +
           R"(, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 })" +
           extra_views + R"(,
    { "buffer": 0, "byteOffset": )" +
           std::to_string(indices_offset) + R"(, "byteLength": 6 }
  ],
  "buffers": [ { "byteLength": )" +
           std::to_string(buffer.size()) + R"(, "uri": "data:application/octet-stream;base64,)" +
           base64(buffer) + R"(" } ]
})";
}

// A cube stored the way glTF stores one: twenty-four vertices, because every
// face needs its own normal, which is precisely what welding undoes.
std::string cube_gltf() {
    std::vector<std::uint8_t> buffer;

    struct Face {
        float normal[3];
        float corners[4][3];
    };

    const float h = 0.5F;
    const Face faces[6] = {
        {{1, 0, 0}, {{h, -h, -h}, {h, -h, h}, {h, h, h}, {h, h, -h}}},
        {{-1, 0, 0}, {{-h, -h, h}, {-h, -h, -h}, {-h, h, -h}, {-h, h, h}}},
        {{0, 1, 0}, {{-h, h, -h}, {h, h, -h}, {h, h, h}, {-h, h, h}}},
        {{0, -1, 0}, {{-h, -h, h}, {h, -h, h}, {h, -h, -h}, {-h, -h, -h}}},
        {{0, 0, 1}, {{-h, -h, h}, {-h, h, h}, {h, h, h}, {h, -h, h}}},
        {{0, 0, -1}, {{h, -h, -h}, {h, h, -h}, {-h, h, -h}, {-h, -h, -h}}},
    };

    for (const Face& face : faces) {
        for (const auto& corner : face.corners) {
            append(buffer, corner[0]);
            append(buffer, corner[1]);
            append(buffer, corner[2]);
        }
    }

    const std::size_t normals_offset = buffer.size();
    for (const Face& face : faces) {
        for (int corner = 0; corner < 4; ++corner) {
            append(buffer, face.normal[0]);
            append(buffer, face.normal[1]);
            append(buffer, face.normal[2]);
        }
    }

    const std::size_t indices_offset = buffer.size();
    for (std::uint16_t face = 0; face < 6; ++face) {
        const std::uint16_t base = static_cast<std::uint16_t>(face * 4);
        for (const int offset : {0, 1, 2, 0, 2, 3}) {
            append(buffer, static_cast<std::uint16_t>(base + offset));
        }
    }

    return R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0] } ],
  "nodes": [ { "name": "cube", "mesh": 0 } ],
  "meshes": [ { "primitives": [
    { "attributes": { "POSITION": 0, "NORMAL": 1 }, "indices": 2 } ] } ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 24, "type": "VEC3",
      "min": [-0.5,-0.5,-0.5], "max": [0.5,0.5,0.5] },
    { "bufferView": 1, "componentType": 5126, "count": 24, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5123, "count": 36, "type": "SCALAR" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 288 },
    { "buffer": 0, "byteOffset": )" +
           std::to_string(normals_offset) + R"(, "byteLength": 288 },
    { "buffer": 0, "byteOffset": )" +
           std::to_string(indices_offset) + R"(, "byteLength": 72 }
  ],
  "buffers": [ { "byteLength": )" +
           std::to_string(buffer.size()) + R"(, "uri": "data:application/octet-stream;base64,)" +
           base64(buffer) + R"(" } ]
})";
}

}  // namespace

TEST_CASE("a triangle imports with three vertices and one face") {
    const TemporaryFile file(triangle_gltf(true));

    Scene scene;
    std::string error;
    REQUIRE_MESSAGE(import_gltf(file.path(), scene, error), error);

    REQUIRE(scene.mesh_count() == 1);
    const auto* mesh = scene.mesh(scene.meshes()[0]);
    REQUIRE(mesh != nullptr);

    CHECK(mesh->vertex_count() == 3);
    CHECK(mesh->corner_count() == 3);
    CHECK(mesh->face_count() == 1);
    CHECK(mesh->validate().empty());
}

TEST_CASE("the node name and hierarchy come through") {
    const TemporaryFile file(triangle_gltf(true));

    Scene scene;
    std::string error;
    REQUIRE(import_gltf(file.path(), scene, error));

    REQUIRE(scene.roots().size() == 1);
    const auto* node = scene.node(scene.roots()[0]);
    REQUIRE(node != nullptr);
    CHECK(node->name == "triangle");
    CHECK(node->mesh.valid());
}

TEST_CASE("a cube's split vertices weld back into eight") {
    const TemporaryFile file(cube_gltf());

    Scene scene;
    std::string error;
    REQUIRE_MESSAGE(import_gltf(file.path(), scene, error), error);

    const auto* mesh = scene.mesh(scene.meshes()[0]);
    REQUIRE(mesh != nullptr);

    // The file stores 24 vertices because each face needs its own normal.
    // Welding recovers the 8 the cube actually has, and the normals move to
    // the 24 corners, where they belong.
    CHECK(mesh->vertex_count() == 8);
    CHECK(mesh->corner_count() == 36);
    CHECK(mesh->face_count() == 12);
    CHECK(mesh->validate().empty());
}

TEST_CASE("importing and baking a cube reproduces what the file held") {
    const TemporaryFile file(cube_gltf());

    Scene scene;
    std::string error;
    REQUIRE(import_gltf(file.path(), scene, error));

    const auto baked = bake(*scene.mesh(scene.meshes()[0]));

    // The round trip: 24 vertices in the file, welded to 8, split again to 24.
    // Welding is lossless precisely because the attributes moved to corners
    // rather than being averaged or discarded.
    CHECK(baked.vertex_count() == 24);
    CHECK(baked.triangle_count() == 12);
}

TEST_CASE("a primitive without normals gets flat ones") {
    const TemporaryFile file(triangle_gltf(false));

    Scene scene;
    std::string error;
    REQUIRE_MESSAGE(import_gltf(file.path(), scene, error), error);

    const auto* mesh = scene.mesh(scene.meshes()[0]);
    REQUIRE(mesh != nullptr);

    const auto* normals = mesh->attributes().find<kinetiqra::math::Vec3>(
        kinetiqra::geom::kNormal, kinetiqra::geom::Domain::Corner);
    REQUIRE(normals != nullptr);
    REQUIRE(normals->size() >= 3);

    // The triangle lies in the XZ plane wound so that its face points down.
    for (std::size_t corner = 0; corner < 3; ++corner) {
        CHECK(std::fabs((*normals)[corner].y) == doctest::Approx(1.0F));
        CHECK((*normals)[corner].x == doctest::Approx(0.0F));
        CHECK((*normals)[corner].z == doctest::Approx(0.0F));
    }
}

TEST_CASE("a bare filename with no directory still loads") {
    // fastgltf resolves external resources against a directory and refuses an
    // empty one, so a path like "model.glb" used to fail while the same file
    // opened fine by absolute path. Every test above passes an absolute path,
    // which is exactly why this needs its own.
    const std::filesystem::path name = "kinetiqra-relative-test.gltf";
    {
        std::ofstream stream(name, std::ios::binary);
        stream << triangle_gltf(true);
    }

    Scene scene;
    std::string error;
    const bool imported = import_gltf(name, scene, error);

    std::error_code ignored;
    std::filesystem::remove(name, ignored);

    REQUIRE_MESSAGE(imported, error);
    CHECK(scene.mesh_count() == 1);
}

TEST_CASE("a malformed file is reported rather than crashing") {
    const TemporaryFile file("{ this is not glTF");

    Scene scene;
    std::string error;

    CHECK_FALSE(import_gltf(file.path(), scene, error));
    CHECK_FALSE(error.empty());
    CHECK(scene.node_count() == 0);
}

TEST_CASE("a missing file is reported rather than crashing") {
    Scene scene;
    std::string error;

    CHECK_FALSE(import_gltf("does-not-exist.gltf", scene, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("importing again replaces the scene rather than adding to it") {
    const TemporaryFile file(triangle_gltf(true));

    Scene scene;
    std::string error;
    REQUIRE(import_gltf(file.path(), scene, error));
    REQUIRE(import_gltf(file.path(), scene, error));

    CHECK(scene.node_count() == 1);
    CHECK(scene.mesh_count() == 1);
}
