#include <kinetiqra/geom/Extrude.hpp>
#include <kinetiqra/geom/Primitives.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <vector>

namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;

namespace {

// One quad in the y = 0 plane, wound so that it faces up.
geom::EditMesh floor_quad() {
    geom::EditMesh mesh;

    const geom::VertexId a = mesh.add_vertex({0.0F, 0.0F, 0.0F});
    const geom::VertexId b = mesh.add_vertex({0.0F, 0.0F, 1.0F});
    const geom::VertexId c = mesh.add_vertex({1.0F, 0.0F, 1.0F});
    const geom::VertexId d = mesh.add_vertex({1.0F, 0.0F, 0.0F});

    std::vector<geom::CornerId> corners;
    mesh.add_face({a, b, c, d}, &corners);
    for (const geom::CornerId corner : corners) {
        mesh.set_normal(corner, {0.0F, 1.0F, 0.0F});
    }

    return mesh;
}

// Two quads side by side sharing the edge along x = 1.
geom::EditMesh two_quads() {
    geom::EditMesh mesh;

    const geom::VertexId a = mesh.add_vertex({0.0F, 0.0F, 0.0F});
    const geom::VertexId b = mesh.add_vertex({0.0F, 0.0F, 1.0F});
    const geom::VertexId c = mesh.add_vertex({1.0F, 0.0F, 1.0F});
    const geom::VertexId d = mesh.add_vertex({1.0F, 0.0F, 0.0F});
    const geom::VertexId e = mesh.add_vertex({2.0F, 0.0F, 1.0F});
    const geom::VertexId f = mesh.add_vertex({2.0F, 0.0F, 0.0F});

    mesh.add_face({a, b, c, d});
    mesh.add_face({d, c, e, f});
    return mesh;
}

}  // namespace

TEST_CASE("extruding one quad leaves a cap and four walls") {
    geom::EditMesh mesh = floor_quad();
    const geom::FaceId original = mesh.faces().front();

    const geom::ExtrudeResult result = geom::extrude(mesh, {original});

    CHECK(result.caps.size() == 1);
    CHECK(result.walls.size() == 4);
    CHECK(result.vertices.size() == 4);
    CHECK(mesh.face_count() == 5);

    // The original is gone, or the result would have a wall inside it.
    CHECK_FALSE(mesh.contains(original));

    // The four it had, and the four it grew.
    CHECK(mesh.vertex_count() == 8);
    CHECK(mesh.validate().empty());
}

TEST_CASE("the new geometry stands exactly where the old did") {
    geom::EditMesh mesh = floor_quad();

    const geom::ExtrudeResult result = geom::extrude(mesh, {mesh.faces().front()});

    // Created in place, with no offset: moving it is the caller's next step,
    // which is what avoids inventing a distance.
    for (const geom::VertexId vertex : result.vertices) {
        CHECK(mesh.position(vertex).y == doctest::Approx(0.0F));
    }
}

TEST_CASE("the cap keeps the direction the face was pointing") {
    geom::EditMesh mesh = floor_quad();

    const geom::ExtrudeResult result = geom::extrude(mesh, {mesh.faces().front()});
    REQUIRE_FALSE(result.caps.empty());

    const auto* normals = mesh.attributes().find<math::Vec3>(geom::kNormal, geom::Domain::Corner);
    REQUIRE(normals != nullptr);

    const std::vector<geom::CornerId>* corners = mesh.face_corners(result.caps.front());
    REQUIRE(corners != nullptr);

    for (const geom::CornerId corner : *corners) {
        CHECK((*normals)[corner.index].y == doctest::Approx(1.0F));
    }
}

TEST_CASE("the walls face outwards") {
    geom::EditMesh mesh = floor_quad();

    const geom::ExtrudeResult result = geom::extrude(mesh, {mesh.faces().front()});
    REQUIRE(result.walls.size() == 4);

    const auto* normals = mesh.attributes().find<math::Vec3>(geom::kNormal, geom::Domain::Corner);
    REQUIRE(normals != nullptr);

    // The quad spans x and z from zero to one, so its centre is at the middle
    // and every wall should point away from it.
    const math::Vec3 centre{0.5F, 0.0F, 0.5F};

    for (const geom::FaceId wall : result.walls) {
        const std::vector<geom::CornerId>* corners = mesh.face_corners(wall);
        REQUIRE(corners != nullptr);
        CHECK(corners->size() == 4);

        math::Vec3 middle{0.0F, 0.0F, 0.0F};
        for (const geom::CornerId corner : *corners) {
            middle += mesh.position(mesh.corner_vertex(corner));
        }
        middle /= static_cast<float>(corners->size());

        const math::Vec3 normal = (*normals)[(*corners)[0].index];
        CHECK(glm::dot(normal, middle - centre) > 0.0F);
    }
}

TEST_CASE("a wall is a quad, which is the first thing gltf cannot hold") {
    geom::EditMesh mesh = floor_quad();

    const geom::ExtrudeResult result = geom::extrude(mesh, {mesh.faces().front()});

    for (const geom::FaceId wall : result.walls) {
        CHECK(mesh.face_corners(wall)->size() == 4);
    }
}

TEST_CASE("an edge between two selected faces gets no wall") {
    geom::EditMesh mesh = two_quads();
    const std::vector<geom::FaceId> both = mesh.faces();
    REQUIRE(both.size() == 2);

    const geom::ExtrudeResult result = geom::extrude(mesh, both);

    // Six walls around the outside of the pair, not eight: the shared edge is
    // on the inside, and a wall there would cut the patch in two.
    CHECK(result.caps.size() == 2);
    CHECK(result.walls.size() == 6);
    CHECK(mesh.face_count() == 8);

    // Six vertices duplicated once each, however many selected faces use them.
    CHECK(result.vertices.size() == 6);
    CHECK(mesh.vertex_count() == 12);
    CHECK(mesh.validate().empty());
}

TEST_CASE("extruding every face of a box doubles its shell") {
    geom::EditMesh mesh = geom::make_box();
    const std::vector<geom::FaceId> all = mesh.faces();

    const geom::ExtrudeResult result = geom::extrude(mesh, all);

    // Every edge of a closed box is shared by two faces, so the whole surface
    // is interior and not one wall goes up.
    CHECK(result.caps.size() == 6);
    CHECK(result.walls.empty());
    CHECK(mesh.face_count() == 6);
    CHECK(mesh.validate().empty());
}

TEST_CASE("extruding nothing changes nothing") {
    geom::EditMesh mesh = floor_quad();

    const geom::ExtrudeResult result = geom::extrude(mesh, {});

    CHECK(result.caps.empty());
    CHECK(result.walls.empty());
    CHECK(result.vertices.empty());
    CHECK(mesh.face_count() == 1);
}

TEST_CASE("a stale face handle is ignored rather than followed") {
    geom::EditMesh mesh = floor_quad();
    const geom::FaceId face = mesh.faces().front();
    REQUIRE(mesh.remove_face(face));

    const geom::ExtrudeResult result = geom::extrude(mesh, {face});

    CHECK(result.caps.empty());
    CHECK(mesh.face_count() == 0);
}

TEST_CASE("extruding a skinned face carries the skinning onto the new vertices") {
    geom::EditMesh mesh = floor_quad();
    const geom::FaceId face = mesh.faces().front();

    for (const geom::CornerId corner : *mesh.face_corners(face)) {
        mesh.set_skinning(mesh.corner_vertex(corner), {2.0F, 0.0F, 0.0F, 0.0F},
                          {1.0F, 0.0F, 0.0F, 0.0F});
    }

    const geom::ExtrudeResult result = geom::extrude(mesh, {face});

    const auto* joints = mesh.attributes().find<math::Vec4>(geom::kJoints, geom::Domain::Vertex);
    REQUIRE(joints != nullptr);

    // Without this the new geometry would stay behind when the skeleton moved.
    for (const geom::VertexId vertex : result.vertices) {
        CHECK((*joints)[vertex.index].x == doctest::Approx(2.0F));
    }
}
