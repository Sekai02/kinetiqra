#include <kinetiqra/geom/Primitives.hpp>
#include <kinetiqra/scene/Raycast.hpp>

#include <doctest/doctest.h>
#include <glm/gtc/matrix_transform.hpp>

namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;
namespace scene = kinetiqra::scene;

using kinetiqra::math::Ray;
using kinetiqra::math::Vec3;
using kinetiqra::scene::Scene;

namespace {

// A ray coming from far along +Z, pointing at the origin.
Ray from_front(float x = 0.0F, float y = 0.0F) {
    return Ray{Vec3{x, y, 20.0F}, Vec3{0.0F, 0.0F, -1.0F}};
}

scene::NodeId add_box(Scene& target, const std::string& name, Vec3 at) {
    const scene::NodeId node = target.add_node(name);
    target.set_mesh(node, target.add_mesh(geom::make_box({2.0F, 2.0F, 2.0F})));
    target.node(node)->transform.translation = at;
    return node;
}

}  // namespace

TEST_CASE("a ray finds the node it lands on") {
    Scene scene;
    const scene::NodeId box = add_box(scene, "box", Vec3{0.0F, 0.0F, 0.0F});

    const auto hit = scene::raycast(scene, from_front());

    REQUIRE(hit.has_value());
    CHECK(hit->node == box);
    CHECK(hit->point.z == doctest::Approx(1.0F));
    CHECK(hit->distance == doctest::Approx(19.0F));
}

TEST_CASE("the nearest node wins") {
    Scene scene;
    add_box(scene, "far", Vec3{0.0F, 0.0F, 0.0F});
    const scene::NodeId near = add_box(scene, "near", Vec3{0.0F, 0.0F, 5.0F});

    const auto hit = scene::raycast(scene, from_front());

    REQUIRE(hit.has_value());
    CHECK(hit->node == near);
    CHECK(scene.node(hit->node)->name == "near");
}

TEST_CASE("a node is picked where its transform puts it, not where its mesh is") {
    Scene scene;
    add_box(scene, "moved", Vec3{10.0F, 0.0F, 0.0F});

    // Straight down the middle, where the mesh would be if the transform were
    // ignored, and there is nothing there.
    CHECK_FALSE(scene::raycast(scene, from_front()).has_value());

    // And where the transform actually put it.
    const auto hit = scene::raycast(scene, from_front(10.0F));
    REQUIRE(hit.has_value());
    CHECK(scene.node(hit->node)->name == "moved");
}

TEST_CASE("scale is taken into account, and the distance stays in world units") {
    Scene scene;
    const scene::NodeId box = add_box(scene, "big", Vec3{0.0F, 0.0F, 0.0F});
    scene.node(box)->transform.scale = Vec3{3.0F, 3.0F, 3.0F};

    const auto hit = scene::raycast(scene, from_front());

    REQUIRE(hit.has_value());
    // The near face has moved out to three units, and the distance is measured
    // in the world rather than in the mesh's own stretched space.
    CHECK(hit->point.z == doctest::Approx(3.0F));
    CHECK(hit->distance == doctest::Approx(17.0F));
}

TEST_CASE("a child is picked through its parent's transform") {
    Scene scene;
    const scene::NodeId parent = scene.add_node("parent");
    scene.node(parent)->transform.translation = Vec3{4.0F, 0.0F, 0.0F};

    const scene::NodeId child = scene.add_node("child", parent);
    scene.set_mesh(child, scene.add_mesh(geom::make_box({2.0F, 2.0F, 2.0F})));
    scene.node(child)->transform.translation = Vec3{1.0F, 0.0F, 0.0F};

    const auto hit = scene::raycast(scene, from_front(5.0F));

    REQUIRE(hit.has_value());
    CHECK(hit->node == child);
}

TEST_CASE("nodes without a mesh are passed through") {
    Scene scene;
    scene.add_node("empty");

    CHECK_FALSE(scene::raycast(scene, from_front()).has_value());
}

TEST_CASE("an empty scene is missed rather than crashed into") {
    const Scene scene;

    CHECK_FALSE(scene::raycast(scene, from_front()).has_value());
}

TEST_CASE("a miss to the side reports nothing") {
    Scene scene;
    add_box(scene, "box", Vec3{0.0F, 0.0F, 0.0F});

    CHECK_FALSE(scene::raycast(scene, from_front(50.0F)).has_value());
}

TEST_CASE("a node with a skin but no weights is placed by its transform") {
    Scene scene;
    const scene::NodeId joint = scene.add_node("joint");

    scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {math::Mat4{1.0F}};

    // A box carries no joints or weights, so there is nothing for the skin to
    // deform. The renderer falls back to drawing it by its transform, and
    // picking has to make the same choice or the two disagree about where it is.
    const scene::NodeId node = add_box(scene, "unweighted", Vec3{10.0F, 0.0F, 0.0F});
    scene.set_skin(node, scene.add_skin(skin));

    CHECK_FALSE(scene::raycast(scene, from_front()).has_value());

    const auto hit = scene::raycast(scene, from_front(10.0F));
    REQUIRE(hit.has_value());
    CHECK(hit->node == node);
}

TEST_CASE("a skinned mesh is picked where its joints put it, not where it is stored") {
    Scene scene;
    const scene::NodeId joint = scene.add_node("joint");

    // The shape a model exported from a tool working in centimetres arrives in:
    // the mesh is stored a hundred times too large and the inverse bind matrix
    // is what brings it back down. Nothing but the joints knows that.
    scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {glm::scale(math::Mat4{1.0F}, Vec3{0.01F, 0.01F, 0.01F})};

    geom::EditMesh mesh = geom::make_box({200.0F, 200.0F, 200.0F});
    for (const geom::VertexId vertex : mesh.vertices()) {
        mesh.set_skinning(vertex, math::Vec4{0.0F, 0.0F, 0.0F, 0.0F},
                          math::Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    }

    const scene::NodeId node = scene.add_node("skinned");
    scene.set_mesh(node, scene.add_mesh(std::move(mesh)));
    scene.set_skin(node, scene.add_skin(skin));

    const auto hit = scene::raycast(scene, from_front());

    REQUIRE(hit.has_value());
    CHECK(hit->node == node);

    // One unit out, where the model is drawn, rather than a hundred, where the
    // mesh happens to be written down.
    CHECK(hit->point.z == doctest::Approx(1.0F));
    CHECK(hit->distance == doctest::Approx(19.0F));
}

TEST_CASE("the world positions of a skinned mesh follow its joints") {
    Scene scene;
    const scene::NodeId joint = scene.add_node("joint");
    scene.node(joint)->transform.translation = Vec3{0.0F, 5.0F, 0.0F};

    scene::Skin skin;
    skin.joints = {joint};
    skin.inverse_bind = {math::Mat4{1.0F}};

    geom::EditMesh mesh = geom::make_box({2.0F, 2.0F, 2.0F});
    for (const geom::VertexId vertex : mesh.vertices()) {
        mesh.set_skinning(vertex, math::Vec4{0.0F, 0.0F, 0.0F, 0.0F},
                          math::Vec4{1.0F, 0.0F, 0.0F, 0.0F});
    }

    const scene::NodeId node = scene.add_node("skinned");
    scene.set_mesh(node, scene.add_mesh(std::move(mesh)));
    scene.set_skin(node, scene.add_skin(skin));

    // Deliberately somewhere else, to show it takes no part.
    scene.node(node)->transform.translation = Vec3{100.0F, 0.0F, 0.0F};

    const std::vector<Vec3> positions = scene.world_positions(node);
    REQUIRE(positions.size() == 8);

    for (const Vec3 position : positions) {
        CHECK(position.y >= 4.0F);
        CHECK(position.x <= 1.0F);
    }
}

TEST_CASE("the world positions of an ordinary mesh follow its node") {
    Scene scene;
    const scene::NodeId node = add_box(scene, "box", Vec3{3.0F, 0.0F, 0.0F});

    const std::vector<Vec3> positions = scene.world_positions(node);
    REQUIRE(positions.size() == 8);

    for (const Vec3 position : positions) {
        CHECK(position.x >= 2.0F);
        CHECK(position.x <= 4.0F);
    }
}

TEST_CASE("a node with no mesh has no positions rather than an empty box") {
    Scene scene;
    const scene::NodeId node = scene.add_node("empty");

    CHECK(scene.world_positions(node).empty());
    CHECK(scene.vertex_matrices(node).empty());
}

TEST_CASE("the reported vertex belongs to the face that was hit") {
    Scene scene;
    add_box(scene, "box", Vec3{0.0F, 0.0F, 0.0F});

    // Near the corner at (1, 1) of the face pointing at the ray.
    const auto hit = scene::raycast(scene, from_front(0.9F, 0.9F));

    REQUIRE(hit.has_value());

    const geom::EditMesh* mesh = scene.mesh(scene.node(hit->node)->mesh);
    REQUIRE(mesh != nullptr);
    CHECK(mesh->contains(hit->vertex));
    CHECK(mesh->position(hit->vertex).x == doctest::Approx(1.0F));
    CHECK(mesh->position(hit->vertex).y == doctest::Approx(1.0F));
}
