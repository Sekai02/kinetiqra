#include <kinetiqra/scene/Scene.hpp>

#include <doctest/doctest.h>
#include <glm/gtc/matrix_transform.hpp>

using kinetiqra::math::Mat4;
using kinetiqra::math::Vec3;
using kinetiqra::math::Vec4;
using kinetiqra::scene::MeshId;
using kinetiqra::scene::NodeId;
using kinetiqra::scene::Scene;

namespace {

Vec3 apply(const Mat4& matrix, Vec3 point) {
    const Vec4 result = matrix * Vec4{point, 1.0F};
    return Vec3{result};
}

}  // namespace

TEST_CASE("a node without a parent is a root") {
    Scene scene;
    const NodeId root = scene.add_node("root");

    REQUIRE(scene.roots().size() == 1);
    CHECK(scene.roots()[0] == root);
    CHECK(scene.node_count() == 1);
}

TEST_CASE("a child is registered with its parent and not as a root") {
    Scene scene;
    const NodeId parent = scene.add_node("parent");
    const NodeId child = scene.add_node("child", parent);

    CHECK(scene.roots().size() == 1);
    REQUIRE(scene.node(parent) != nullptr);
    REQUIRE(scene.node(parent)->children.size() == 1);
    CHECK(scene.node(parent)->children[0] == child);
    CHECK(scene.node(child)->parent == parent);
}

TEST_CASE("world transforms compose through parents") {
    Scene scene;
    const NodeId parent = scene.add_node("parent");
    const NodeId child = scene.add_node("child", parent);

    scene.node(parent)->transform.translation = Vec3{10.0F, 0.0F, 0.0F};
    scene.node(child)->transform.translation = Vec3{0.0F, 2.0F, 0.0F};

    const Vec3 origin_of_child = apply(scene.world_transform(child), Vec3{0.0F});

    CHECK(origin_of_child.x == doctest::Approx(10.0F));
    CHECK(origin_of_child.y == doctest::Approx(2.0F));
}

TEST_CASE("a child keeps its local transform when the parent moves") {
    Scene scene;
    const NodeId parent = scene.add_node("parent");
    const NodeId child = scene.add_node("child", parent);
    scene.node(child)->transform.translation = Vec3{0.0F, 1.0F, 0.0F};

    const Vec3 before = apply(scene.world_transform(child), Vec3{0.0F});
    scene.node(parent)->transform.translation = Vec3{5.0F, 0.0F, 0.0F};
    const Vec3 after = apply(scene.world_transform(child), Vec3{0.0F});

    // The child moved with the parent, and its own offset is unchanged.
    CHECK(after.x - before.x == doctest::Approx(5.0F));
    CHECK(after.y == doctest::Approx(1.0F));

    const Vec3 local = scene.node(child)->transform.translation;
    CHECK(local.y == doctest::Approx(1.0F));
}

TEST_CASE("a parent's scale reaches its child's offset") {
    Scene scene;
    const NodeId parent = scene.add_node("parent");
    const NodeId child = scene.add_node("child", parent);

    scene.node(parent)->transform.scale = Vec3{2.0F, 2.0F, 2.0F};
    scene.node(child)->transform.translation = Vec3{3.0F, 0.0F, 0.0F};

    const Vec3 origin_of_child = apply(scene.world_transform(child), Vec3{0.0F});
    CHECK(origin_of_child.x == doctest::Approx(6.0F));
}

TEST_CASE("nodes come out parents before children") {
    Scene scene;
    const NodeId a = scene.add_node("a");
    const NodeId b = scene.add_node("b", a);
    const NodeId c = scene.add_node("c", b);
    const NodeId d = scene.add_node("d");

    const auto ordered = scene.nodes_in_order();

    REQUIRE(ordered.size() == 4);
    CHECK(ordered[0] == a);
    CHECK(ordered[1] == b);
    CHECK(ordered[2] == c);
    CHECK(ordered[3] == d);
}

TEST_CASE("a node without a mesh has an invalid mesh handle") {
    Scene scene;
    const NodeId node = scene.add_node("empty");

    CHECK_FALSE(scene.node(node)->mesh.valid());
    CHECK(scene.mesh(scene.node(node)->mesh) == nullptr);
}

TEST_CASE("a mesh is reachable through the node that refers to it") {
    Scene scene;
    const NodeId node = scene.add_node("with a mesh");

    kinetiqra::geom::EditMesh mesh;
    mesh.add_vertex(Vec3{0.0F});
    const MeshId id = scene.add_mesh(std::move(mesh));
    scene.set_mesh(node, id);

    REQUIRE(scene.mesh(scene.node(node)->mesh) != nullptr);
    CHECK(scene.mesh(id)->vertex_count() == 1);
    CHECK(scene.mesh_count() == 1);
}

TEST_CASE("clearing empties the scene") {
    Scene scene;
    const NodeId root = scene.add_node("root");
    scene.add_node("child", root);

    scene.clear();

    CHECK(scene.node_count() == 0);
    CHECK(scene.roots().empty());
    CHECK(scene.node(root) == nullptr);
}
