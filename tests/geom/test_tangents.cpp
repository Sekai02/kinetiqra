#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/geom/Tangents.hpp>

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <vector>

namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;

namespace {

// A quad in the y = 0 plane facing up, with U running along +X and V along +Z.
// Everything below is measured against that.
geom::EditMesh unwrapped_quad(bool mirrored = false) {
    geom::EditMesh mesh;

    const geom::VertexId a = mesh.add_vertex({0.0F, 0.0F, 0.0F});
    const geom::VertexId b = mesh.add_vertex({1.0F, 0.0F, 0.0F});
    const geom::VertexId c = mesh.add_vertex({1.0F, 0.0F, 1.0F});
    const geom::VertexId d = mesh.add_vertex({0.0F, 0.0F, 1.0F});

    std::vector<geom::CornerId> corners;
    mesh.add_face({a, b, c, d}, &corners);

    const std::vector<math::Vec2> uvs{
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {1.0F, 1.0F},
        {0.0F, 1.0F},
    };

    for (std::size_t i = 0; i < corners.size(); ++i) {
        mesh.set_normal(corners[i], {0.0F, 1.0F, 0.0F});

        // Mirroring flips the U axis, which is what a mirrored UV island is and
        // what the handedness in w exists to record.
        const math::Vec2 uv = uvs[i];
        mesh.set_uv(corners[i], mirrored ? math::Vec2{1.0F - uv.x, uv.y} : uv);
    }

    return mesh;
}

const std::vector<math::Vec4>& tangents_of(const geom::EditMesh& mesh) {
    const auto* channel = mesh.attributes().find<math::Vec4>(geom::kTangent, geom::Domain::Corner);
    REQUIRE(channel != nullptr);
    return *channel;
}

}  // namespace

TEST_CASE("the tangent runs the way the texture does") {
    geom::EditMesh mesh = unwrapped_quad();
    geom::compute_tangents(mesh);

    const std::vector<math::Vec4>& tangents = tangents_of(mesh);

    for (const geom::CornerId corner : *mesh.face_corners(mesh.faces().front())) {
        const math::Vec4 tangent = tangents[corner.index];

        // U runs along +X on this quad, so the tangent has to as well.
        CHECK(tangent.x == doctest::Approx(1.0F));
        CHECK(tangent.y == doctest::Approx(0.0F));
        CHECK(tangent.z == doctest::Approx(0.0F));
    }
}

TEST_CASE("the tangent lies in the surface") {
    geom::EditMesh mesh = unwrapped_quad();
    geom::compute_tangents(mesh);

    const std::vector<math::Vec4>& tangents = tangents_of(mesh);
    const auto* normals = mesh.attributes().find<math::Vec3>(geom::kNormal, geom::Domain::Corner);

    for (const geom::CornerId corner : *mesh.face_corners(mesh.faces().front())) {
        const auto tangent = math::Vec3{tangents[corner.index]};

        // The shader builds a square frame out of these two, so a tangent that
        // leaned away from the surface would tilt every direction read from a
        // normal map.
        CHECK(glm::dot(tangent, (*normals)[corner.index]) == doctest::Approx(0.0F));
        CHECK(glm::length(tangent) == doctest::Approx(1.0F));
    }
}

TEST_CASE("a mirrored island gets the opposite handedness") {
    geom::EditMesh plain = unwrapped_quad(false);
    geom::EditMesh mirrored = unwrapped_quad(true);

    geom::compute_tangents(plain);
    geom::compute_tangents(mirrored);

    const float straight = tangents_of(plain)[0].w;
    const float flipped = tangents_of(mirrored)[0].w;

    // Without this the mirrored half of a model is lit as though the light came
    // from the other side.
    CHECK(straight == doctest::Approx(-flipped));
    CHECK(std::abs(straight) == doctest::Approx(1.0F));
}

TEST_CASE("a face with no texture area is left alone") {
    geom::EditMesh mesh = unwrapped_quad();

    // Every corner on the same spot of the texture, so there is no direction to
    // be recovered from it.
    for (const geom::CornerId corner : *mesh.face_corners(mesh.faces().front())) {
        mesh.set_uv(corner, math::Vec2{0.5F, 0.5F});
    }

    geom::compute_tangents(mesh);

    const std::vector<math::Vec4>& tangents = tangents_of(mesh);
    for (const geom::CornerId corner : *mesh.face_corners(mesh.faces().front())) {
        // The default the mesh is built with, rather than a zero or a NaN.
        CHECK(tangents[corner.index].x == doctest::Approx(1.0F));
    }
}

TEST_CASE("a box comes out with a tangent on every corner") {
    geom::EditMesh box = geom::make_box();

    // make_box gives each face the whole unit square, so every face is its own
    // island and every corner has something to work from.
    geom::compute_tangents(box);

    const std::vector<math::Vec4>& tangents = tangents_of(box);
    for (const geom::FaceId face : box.faces()) {
        for (const geom::CornerId corner : *box.face_corners(face)) {
            CHECK(glm::length(math::Vec3{tangents[corner.index]}) == doctest::Approx(1.0F));
        }
    }
}

TEST_CASE("a mesh with no uvs is not disturbed") {
    geom::EditMesh mesh;
    const geom::VertexId a = mesh.add_vertex({0.0F, 0.0F, 0.0F});
    const geom::VertexId b = mesh.add_vertex({1.0F, 0.0F, 0.0F});
    const geom::VertexId c = mesh.add_vertex({0.0F, 1.0F, 0.0F});
    mesh.add_face({a, b, c});

    geom::compute_tangents(mesh);

    for (const geom::CornerId corner : *mesh.face_corners(mesh.faces().front())) {
        CHECK(tangents_of(mesh)[corner.index].x == doctest::Approx(1.0F));
    }
}
