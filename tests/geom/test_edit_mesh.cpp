#include <kinetiqra/geom/Bake.hpp>
#include <kinetiqra/geom/EditMesh.hpp>
#include <kinetiqra/geom/Primitives.hpp>

#include <doctest/doctest.h>

using kinetiqra::geom::bake;
using kinetiqra::geom::CornerId;
using kinetiqra::geom::Domain;
using kinetiqra::geom::EditMesh;
using kinetiqra::geom::FaceId;
using kinetiqra::geom::kNormal;
using kinetiqra::geom::kPosition;
using kinetiqra::geom::kUv;
using kinetiqra::geom::make_box;
using kinetiqra::geom::VertexId;
using kinetiqra::math::Vec2;
using kinetiqra::math::Vec3;
using kinetiqra::math::Vec4;

TEST_CASE("channels grow with the domain they describe") {
    EditMesh mesh;
    auto* mask = mesh.attributes().add<float>("mask", Domain::Vertex, 0.5F);
    REQUIRE(mask != nullptr);
    CHECK(mask->empty());

    mesh.add_vertex(Vec3{0.0F});
    mesh.add_vertex(Vec3{1.0F});

    // The pointer is into the channel, which the mesh resized underneath it.
    const auto* grown = mesh.attributes().find<float>("mask", Domain::Vertex);
    REQUIRE(grown != nullptr);
    CHECK(grown->size() == 2);
    CHECK((*grown)[0] == doctest::Approx(0.5F));
}

TEST_CASE("a channel is rejected when the name is taken by another type") {
    EditMesh mesh;
    REQUIRE(mesh.attributes().add<float>("weight", Domain::Vertex) != nullptr);

    CHECK(mesh.attributes().add<Vec3>("weight", Domain::Vertex) == nullptr);
    CHECK(mesh.attributes().find<Vec3>("weight", Domain::Vertex) == nullptr);
    CHECK(mesh.attributes().find<float>("weight", Domain::Vertex) != nullptr);
}

TEST_CASE("two corners on one vertex can disagree") {
    // The whole reason attributes live on corners: one vertex, two faces, two
    // different UVs. With attributes on vertices this could not be expressed.
    EditMesh mesh;
    const VertexId shared = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId a = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{0.0F, 1.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{-1.0F, 0.0F, 0.0F});

    std::vector<CornerId> first;
    std::vector<CornerId> second;
    mesh.add_face({shared, a, b}, &first);
    mesh.add_face({shared, b, c}, &second);

    mesh.set_uv(first[0], Vec2{0.0F, 0.0F});
    mesh.set_uv(second[0], Vec2{1.0F, 0.0F});

    const auto* uvs = mesh.attributes().find<Vec2>(kUv, Domain::Corner);
    REQUIRE(uvs != nullptr);

    CHECK(mesh.corner_vertex(first[0]) == shared);
    CHECK(mesh.corner_vertex(second[0]) == shared);
    CHECK((*uvs)[first[0].index].x == doctest::Approx(0.0F));
    CHECK((*uvs)[second[0].index].x == doctest::Approx(1.0F));
    CHECK(mesh.vertex_count() == 4);
}

TEST_CASE("validation catches a corner left pointing at a removed vertex") {
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{0.0F, 1.0F, 0.0F});
    mesh.add_face({a, b, c});

    CHECK(mesh.validate().empty());

    REQUIRE(mesh.remove_vertex(b));

    // The face was deliberately left alone, so the mesh is now inconsistent and
    // says so rather than being quietly repaired.
    CHECK_FALSE(mesh.validate().empty());
}

TEST_CASE("a box has eight vertices and twenty-four corners") {
    const EditMesh box = make_box();

    CHECK(box.vertex_count() == 8);
    CHECK(box.corner_count() == 24);
    CHECK(box.face_count() == 6);
    CHECK(box.validate().empty());
}

TEST_CASE("a box is one metre on a side by default") {
    const EditMesh box = make_box();

    float min_x = 1e9F;
    float max_x = -1e9F;
    for (const FaceId face : box.faces()) {
        for (const CornerId corner : *box.face_corners(face)) {
            const float x = box.position(box.corner_vertex(corner)).x;
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
        }
    }

    CHECK(max_x - min_x == doctest::Approx(1.0F));
    CHECK(min_x == doctest::Approx(-0.5F));
}

TEST_CASE("baking a box splits every corner and keeps the triangles") {
    const EditMesh box = make_box();
    const auto baked = bake(box);

    // Six faces, each with its own normal and its own UV square, so no two
    // corners agree and nothing merges: twenty-four vertices, twelve triangles.
    CHECK(baked.vertex_count() == 24);
    CHECK(baked.triangle_count() == 12);
    CHECK(baked.indices.size() == 36);

    for (const std::uint32_t index : baked.indices) {
        CHECK(index < baked.vertex_count());
    }
}

TEST_CASE("corners that agree collapse instead of duplicating") {
    // Two triangles sharing an edge, with matching normals and UVs everywhere.
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{1.0F, 0.0F, 1.0F});
    const VertexId d = mesh.add_vertex(Vec3{0.0F, 0.0F, 1.0F});

    std::vector<CornerId> first;
    std::vector<CornerId> second;
    mesh.add_face({a, b, c}, &first);
    mesh.add_face({a, c, d}, &second);

    for (const CornerId corner : first) {
        mesh.set_normal(corner, Vec3{0.0F, 1.0F, 0.0F});
        mesh.set_uv(corner, Vec2{0.0F, 0.0F});
    }
    for (const CornerId corner : second) {
        mesh.set_normal(corner, Vec3{0.0F, 1.0F, 0.0F});
        mesh.set_uv(corner, Vec2{0.0F, 0.0F});
    }

    const auto baked = bake(mesh);

    // Six corners over four positions, but every corner agrees, so the shared
    // edge is not duplicated.
    CHECK(mesh.corner_count() == 6);
    CHECK(baked.vertex_count() == 4);
    CHECK(baked.triangle_count() == 2);
}

TEST_CASE("a quad is triangulated as a fan") {
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{1.0F, 0.0F, 1.0F});
    const VertexId d = mesh.add_vertex(Vec3{0.0F, 0.0F, 1.0F});
    mesh.add_face({a, b, c, d});

    const auto baked = bake(mesh);

    CHECK(baked.triangle_count() == 2);
    CHECK(baked.vertex_count() == 4);
}

TEST_CASE("an unskinned mesh bakes to eight floats a vertex") {
    const EditMesh box = make_box();
    const auto baked = bake(box);

    CHECK_FALSE(box.skinned());
    CHECK_FALSE(baked.skinned);
    CHECK(baked.floats_per_vertex() == 8);
    CHECK(baked.vertices.size() == baked.vertex_count() * 8);
}

TEST_CASE("skinning a vertex makes the mesh bake to sixteen floats a vertex") {
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{0.0F, 1.0F, 0.0F});
    mesh.add_face({a, b, c});

    CHECK_FALSE(mesh.skinned());

    mesh.set_skinning(a, Vec4{0, 1, 0, 0}, Vec4{0.75F, 0.25F, 0.0F, 0.0F});
    mesh.set_skinning(b, Vec4{1, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    mesh.set_skinning(c, Vec4{0, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});

    CHECK(mesh.skinned());

    const auto baked = bake(mesh);
    CHECK(baked.skinned);
    CHECK(baked.floats_per_vertex() == 16);
    CHECK(baked.vertex_count() == 3);
    CHECK(baked.vertices.size() == 48);
}

TEST_CASE("joints and weights land on the vertex they belong to") {
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{0.0F, 1.0F, 0.0F});
    mesh.add_face({a, b, c});

    mesh.set_skinning(a, Vec4{3, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    mesh.set_skinning(b, Vec4{0, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    mesh.set_skinning(c, Vec4{0, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});

    const auto baked = bake(mesh);

    // The first emitted vertex is the first corner of the only face, which is
    // vertex a, and its joint index sits at offset eight in the layout.
    CHECK(baked.vertices[0] == doctest::Approx(0.0F));
    CHECK(baked.vertices[8] == doctest::Approx(3.0F));
    CHECK(baked.vertices[12] == doctest::Approx(1.0F));
}

TEST_CASE("corners of one skinned vertex still merge when they agree") {
    // Skinning data is per vertex, so it can never be the reason two corners
    // fail to merge. This pins that down.
    EditMesh mesh;
    const VertexId a = mesh.add_vertex(Vec3{0.0F, 0.0F, 0.0F});
    const VertexId b = mesh.add_vertex(Vec3{1.0F, 0.0F, 0.0F});
    const VertexId c = mesh.add_vertex(Vec3{1.0F, 0.0F, 1.0F});
    const VertexId d = mesh.add_vertex(Vec3{0.0F, 0.0F, 1.0F});

    std::vector<CornerId> first;
    std::vector<CornerId> second;
    mesh.add_face({a, b, c}, &first);
    mesh.add_face({a, c, d}, &second);

    for (const VertexId vertex : {a, b, c, d}) {
        mesh.set_skinning(vertex, Vec4{0, 0, 0, 0}, Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    }
    for (const CornerId corner : first) {
        mesh.set_normal(corner, Vec3{0.0F, 1.0F, 0.0F});
    }
    for (const CornerId corner : second) {
        mesh.set_normal(corner, Vec3{0.0F, 1.0F, 0.0F});
    }

    const auto baked = bake(mesh);

    CHECK(mesh.corner_count() == 6);
    CHECK(baked.vertex_count() == 4);
}

TEST_CASE("baking an empty mesh yields nothing rather than failing") {
    const EditMesh mesh;
    const auto baked = bake(mesh);

    CHECK(baked.vertex_count() == 0);
    CHECK(baked.triangle_count() == 0);
}

TEST_CASE("removing a face takes its corners with it") {
    EditMesh mesh = make_box();
    const FaceId face = mesh.faces().front();

    REQUIRE(mesh.remove_face(face));

    CHECK(mesh.face_count() == 5);
    CHECK(mesh.corner_count() == 20);
    CHECK_FALSE(mesh.contains(face));

    // The vertices stay: a vertex outlives the faces that used it, and deciding
    // whether one has been orphaned is not the mesh's call.
    CHECK(mesh.vertex_count() == 8);
    CHECK(mesh.validate().empty());

    // Removing twice is reported rather than corrupting anything.
    CHECK_FALSE(mesh.remove_face(face));
}

TEST_CASE("a clone is a copy, not a second view of the same mesh") {
    EditMesh original = make_box();
    const VertexId vertex =
        original.corner_vertex(original.face_corners(original.faces().front())->front());
    const Vec3 before = original.position(vertex);

    EditMesh copy = original.clone();

    CHECK(copy.vertex_count() == original.vertex_count());
    CHECK(copy.face_count() == original.face_count());
    CHECK(copy.corner_count() == original.corner_count());

    copy.set_position(vertex, Vec3{99.0F, 99.0F, 99.0F});

    // The channels are held behind pointers, so a shallow copy would have moved
    // both of these at once.
    CHECK(copy.position(vertex).x == doctest::Approx(99.0F));
    CHECK(original.position(vertex).x == doctest::Approx(before.x));
}

TEST_CASE("a clone keeps every channel, including the ones added later") {
    EditMesh original = make_box();
    original.attributes().add<float>("thickness", Domain::Vertex, 0.5F);

    const EditMesh copy = original.clone();

    CHECK(copy.attributes().names(Domain::Vertex) == original.attributes().names(Domain::Vertex));

    const auto* thickness = copy.attributes().find<float>("thickness", Domain::Vertex);
    REQUIRE(thickness != nullptr);
    CHECK(thickness->size() == copy.vertex_count());
    CHECK((*thickness)[0] == doctest::Approx(0.5F));
}

TEST_CASE("a cloned mesh grows on its own") {
    EditMesh original = make_box();
    EditMesh copy = original.clone();

    // The fill value travels with the channel, so a vertex added to the copy is
    // filled the way the original would have filled it.
    copy.attributes().add<float>("thickness", Domain::Vertex, 0.25F);
    const VertexId added = copy.add_vertex(Vec3{0.0F, 0.0F, 0.0F});

    CHECK(copy.vertex_count() == original.vertex_count() + 1);
    CHECK(original.vertex_count() == 8);

    const auto* thickness = copy.attributes().find<float>("thickness", Domain::Vertex);
    REQUIRE(thickness != nullptr);
    CHECK((*thickness)[added.index] == doctest::Approx(0.25F));
}
