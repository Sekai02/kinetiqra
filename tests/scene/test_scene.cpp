#include <kinetiqra/scene/Pose.hpp>
#include <kinetiqra/scene/Scene.hpp>

#include <doctest/doctest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

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

TEST_CASE("in the bind pose the joint matrices are the identity") {
    Scene scene;
    const NodeId root = scene.add_node("root");
    const NodeId joint = scene.add_node("joint", root);
    scene.node(joint)->transform.translation = Vec3{0.0F, 2.0F, 0.0F};

    // The inverse bind matrix is, by definition, the inverse of the joint's
    // world transform at the moment of binding.
    kinetiqra::scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {glm::inverse(scene.world_transform(joint))};

    const auto id = scene.add_skin(skin);
    REQUIRE(id.valid());

    const auto matrices = scene.joint_matrices(id);
    REQUIRE(matrices.size() == 1);

    // Nothing has moved since binding, so the deformation is nothing at all.
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            CHECK(matrices[0][column][row] ==
                  doctest::Approx(Mat4{1.0F}[column][row]).epsilon(1e-5));
        }
    }
}

TEST_CASE("moving a joint away from bind deforms by exactly that movement") {
    Scene scene;
    const NodeId joint = scene.add_node("joint");

    kinetiqra::scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {glm::inverse(scene.world_transform(joint))};
    const auto id = scene.add_skin(skin);

    scene.node(joint)->transform.translation = Vec3{3.0F, 0.0F, 0.0F};

    const auto matrices = scene.joint_matrices(id);
    const Vec3 moved = apply(matrices[0], Vec3{0.0F});

    CHECK(moved.x == doctest::Approx(3.0F));
}

TEST_CASE("a parent joint carries its child") {
    Scene scene;
    const NodeId shoulder = scene.add_node("shoulder");
    const NodeId elbow = scene.add_node("elbow", shoulder);
    scene.node(elbow)->transform.translation = Vec3{0.0F, -1.0F, 0.0F};

    kinetiqra::scene::Skin skin;
    skin.joints = {shoulder, elbow};
    skin.inverse_bind = {glm::inverse(scene.world_transform(shoulder)),
                         glm::inverse(scene.world_transform(elbow))};
    const auto id = scene.add_skin(skin);

    scene.node(shoulder)->transform.translation = Vec3{0.0F, 5.0F, 0.0F};

    const auto matrices = scene.joint_matrices(id);
    REQUIRE(matrices.size() == 2);

    // Both joints moved, because the elbow hangs off the shoulder.
    CHECK(apply(matrices[0], Vec3{0.0F}).y == doctest::Approx(5.0F));
    CHECK(apply(matrices[1], Vec3{0.0F}).y == doctest::Approx(5.0F));
}

TEST_CASE("a skin whose counts disagree is refused") {
    Scene scene;
    const NodeId joint = scene.add_node("joint");

    kinetiqra::scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {};

    CHECK_FALSE(scene.add_skin(skin).valid());
    CHECK(scene.joint_matrices(kinetiqra::scene::SkinId{}).empty());
}

TEST_CASE("a pose stands in for the nodes it mentions") {
    Scene scene;
    const NodeId node = scene.add_node("node");
    scene.node(node)->transform.translation = Vec3{1.0F, 0.0F, 0.0F};

    kinetiqra::scene::Pose pose;
    kinetiqra::scene::Transform posed;
    posed.translation = Vec3{9.0F, 0.0F, 0.0F};
    pose.set(node, posed);

    CHECK(apply(scene.world_transform(node), Vec3{0.0F}).x == doctest::Approx(1.0F));
    CHECK(apply(scene.world_transform(node, &pose), Vec3{0.0F}).x == doctest::Approx(9.0F));

    // The scene itself was not touched, which is the whole point: playback is a
    // view, not an edit.
    CHECK(scene.node(node)->transform.translation.x == doctest::Approx(1.0F));
}

TEST_CASE("a node the pose says nothing about keeps its own transform") {
    Scene scene;
    const NodeId parent = scene.add_node("parent");
    const NodeId child = scene.add_node("child", parent);
    scene.node(parent)->transform.translation = Vec3{10.0F, 0.0F, 0.0F};
    scene.node(child)->transform.translation = Vec3{0.0F, 3.0F, 0.0F};

    kinetiqra::scene::Pose pose;
    kinetiqra::scene::Transform posed;
    posed.translation = Vec3{0.0F, 0.0F, 0.0F};
    pose.set(parent, posed);

    const Vec3 world = apply(scene.world_transform(child, &pose), Vec3{0.0F});

    // The parent moved because the pose said so, and the child kept the offset
    // it was authored with.
    CHECK(world.x == doctest::Approx(0.0F));
    CHECK(world.y == doctest::Approx(3.0F));
}

TEST_CASE("a pose naming a node that is gone is ignored") {
    Scene scene;
    const NodeId node = scene.add_node("node");

    kinetiqra::scene::Pose pose;
    pose.set(node, kinetiqra::scene::Transform{});

    scene.clear();
    const NodeId replacement = scene.add_node("another");

    // The new node reuses the slot, and the stale entry must not move it.
    CHECK(pose.find(replacement) == nullptr);
}

TEST_CASE("joint matrices follow the pose") {
    Scene scene;
    const NodeId joint = scene.add_node("joint");

    kinetiqra::scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {glm::inverse(scene.world_transform(joint))};
    const auto id = scene.add_skin(skin);

    kinetiqra::scene::Pose pose;
    kinetiqra::scene::Transform posed;
    posed.translation = Vec3{0.0F, 4.0F, 0.0F};
    pose.set(joint, posed);

    CHECK(apply(scene.joint_matrices(id)[0], Vec3{0.0F}).y == doctest::Approx(0.0F));
    CHECK(apply(scene.joint_matrices(id, &pose)[0], Vec3{0.0F}).y == doctest::Approx(4.0F));
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
