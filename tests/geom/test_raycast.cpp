#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/geom/Raycast.hpp>

#include <doctest/doctest.h>

#include <vector>

namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;

using kinetiqra::math::Ray;
using kinetiqra::math::Vec3;

namespace {

// A single quad in the z = 0 plane, two units across and centred on the origin.
geom::EditMesh quad(float z = 0.0F) {
    geom::EditMesh mesh;

    const geom::VertexId a = mesh.add_vertex({-1.0F, -1.0F, z});
    const geom::VertexId b = mesh.add_vertex({1.0F, -1.0F, z});
    const geom::VertexId c = mesh.add_vertex({1.0F, 1.0F, z});
    const geom::VertexId d = mesh.add_vertex({-1.0F, 1.0F, z});

    mesh.add_face({a, b, c, d});
    return mesh;
}

}  // namespace

TEST_CASE("a ray through a quad reports the face and where it landed") {
    const geom::EditMesh mesh = quad();
    const Ray ray{Vec3{0.25F, 0.25F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}};

    const auto hit = geom::raycast(mesh, ray);

    REQUIRE(hit.has_value());
    CHECK(mesh.contains(hit->face));
    CHECK(hit->distance == doctest::Approx(5.0F));
    CHECK(hit->point.x == doctest::Approx(0.25F));
    CHECK(hit->point.z == doctest::Approx(0.0F));
}

TEST_CASE("a quad is hit on both of its triangles") {
    const geom::EditMesh mesh = quad();

    // A fan splits the quad along one diagonal, so a point on each side of that
    // diagonal exercises a different triangle.
    for (const Vec3 origin : {Vec3{0.6F, -0.6F, 5.0F}, Vec3{-0.6F, 0.6F, 5.0F}}) {
        const auto hit = geom::raycast(mesh, Ray{origin, Vec3{0.0F, 0.0F, -1.0F}});
        CHECK(hit.has_value());
    }
}

TEST_CASE("a ray that misses reports nothing") {
    const geom::EditMesh mesh = quad();
    const Ray ray{Vec3{5.0F, 5.0F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}};

    CHECK_FALSE(geom::raycast(mesh, ray).has_value());
}

TEST_CASE("the nearest face wins") {
    geom::EditMesh mesh = quad(0.0F);

    // A second quad in front of the first, so a ray from +Z meets it first.
    const geom::VertexId a = mesh.add_vertex({-1.0F, -1.0F, 2.0F});
    const geom::VertexId b = mesh.add_vertex({1.0F, -1.0F, 2.0F});
    const geom::VertexId c = mesh.add_vertex({1.0F, 1.0F, 2.0F});
    const geom::VertexId d = mesh.add_vertex({-1.0F, 1.0F, 2.0F});
    const geom::FaceId front = mesh.add_face({a, b, c, d});

    const auto hit = geom::raycast(mesh, Ray{Vec3{0.0F, 0.0F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}});

    REQUIRE(hit.has_value());
    CHECK(hit->face == front);
    CHECK(hit->distance == doctest::Approx(3.0F));
}

TEST_CASE("the vertex reported is the corner nearest the hit") {
    const geom::EditMesh mesh = quad();

    // Close to the corner at (1, 1), which is on the far side of the diagonal
    // from the first triangle of the fan.
    const auto hit = geom::raycast(mesh, Ray{Vec3{0.9F, 0.9F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}});

    REQUIRE(hit.has_value());
    CHECK(mesh.position(hit->vertex).x == doctest::Approx(1.0F));
    CHECK(mesh.position(hit->vertex).y == doctest::Approx(1.0F));
}

TEST_CASE("a box is hit on the side facing the ray") {
    const geom::EditMesh mesh = geom::make_box({2.0F, 2.0F, 2.0F});

    const auto hit = geom::raycast(mesh, Ray{Vec3{0.0F, 0.0F, 10.0F}, Vec3{0.0F, 0.0F, -1.0F}});

    REQUIRE(hit.has_value());
    // The near face of a two unit box centred on the origin, not the far one.
    CHECK(hit->point.z == doctest::Approx(1.0F));
    CHECK(hit->distance == doctest::Approx(9.0F));
}

TEST_CASE("an empty mesh is missed rather than crashed into") {
    const geom::EditMesh mesh;

    CHECK_FALSE(
        geom::raycast(mesh, Ray{Vec3{0.0F, 0.0F, 5.0F}, Vec3{0.0F, 0.0F, -1.0F}}).has_value());
}
