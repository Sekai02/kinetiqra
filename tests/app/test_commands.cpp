#include <kinetiqra/app/Commands.hpp>
#include <kinetiqra/core/Command.hpp>
#include <kinetiqra/geom/Extrude.hpp>
#include <kinetiqra/geom/Primitives.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

namespace app = kinetiqra::app;
namespace geom = kinetiqra::geom;
namespace math = kinetiqra::math;
namespace scene = kinetiqra::scene;

using kinetiqra::core::CommandStack;
using kinetiqra::scene::Scene;

TEST_CASE("transforming a node can be undone and done again") {
    Scene target;
    const scene::NodeId node = target.add_node("node");

    scene::Transform before = target.node(node)->transform;
    scene::Transform after = before;
    after.translation = math::Vec3{1.0F, 2.0F, 3.0F};
    after.scale = math::Vec3{2.0F, 2.0F, 2.0F};

    CommandStack stack;
    stack.execute(std::make_unique<app::TransformNode>(target, node, before, after));

    CHECK(target.node(node)->transform.translation.y == doctest::Approx(2.0F));
    CHECK(target.node(node)->transform.scale.x == doctest::Approx(2.0F));

    REQUIRE(stack.undo());
    CHECK(target.node(node)->transform.translation.y == doctest::Approx(0.0F));
    CHECK(target.node(node)->transform.scale.x == doctest::Approx(1.0F));

    REQUIRE(stack.redo());
    CHECK(target.node(node)->transform.translation.y == doctest::Approx(2.0F));
}

TEST_CASE("a command whose node has gone does nothing rather than something wrong") {
    Scene target;
    const scene::NodeId node = target.add_node("node");

    scene::Transform after;
    after.translation = math::Vec3{1.0F, 0.0F, 0.0F};

    CommandStack stack;
    stack.execute(std::make_unique<app::TransformNode>(target, node, scene::Transform{}, after));

    // The scene is replaced underneath the history, which is what loading
    // another model does.
    target.clear();
    const scene::NodeId replacement = target.add_node("different");

    CHECK(stack.undo());
    CHECK(target.node(replacement)->transform.translation.x == doctest::Approx(0.0F));
}

TEST_CASE("moving vertices can be undone as one step") {
    Scene target;
    const scene::MeshId mesh = target.add_mesh(geom::make_box());

    const geom::EditMesh* editable = target.mesh(mesh);
    const std::vector<geom::CornerId>* corners = editable->face_corners(editable->faces().front());

    std::vector<app::MoveVertices::Moved> moved;
    for (const geom::CornerId corner : *corners) {
        const geom::VertexId vertex = editable->corner_vertex(corner);
        moved.push_back({vertex, editable->position(vertex),
                         editable->position(vertex) + math::Vec3{0.0F, 5.0F, 0.0F}});
    }
    REQUIRE(moved.size() == 4);

    CommandStack stack;
    stack.execute(std::make_unique<app::MoveVertices>(target, mesh, moved));

    CHECK(target.mesh(mesh)->position(moved[0].vertex).y ==
          doctest::Approx(moved[0].before.y + 5.0F));

    // One press, not one per vertex and not one per frame of the drag.
    REQUIRE(stack.undo());
    CHECK(stack.depth() == 0);
    for (const app::MoveVertices::Moved& one : moved) {
        CHECK(target.mesh(mesh)->position(one.vertex).y == doctest::Approx(one.before.y));
    }

    REQUIRE(stack.redo());
    CHECK(target.mesh(mesh)->position(moved[0].vertex).y ==
          doctest::Approx(moved[0].before.y + 5.0F));
}

TEST_CASE("moving vertices skips the ones that are no longer there") {
    Scene target;
    const scene::MeshId mesh = target.add_mesh(geom::make_box());

    const geom::VertexId vertex = target.mesh(mesh)->corner_vertex(
        target.mesh(mesh)->face_corners(target.mesh(mesh)->faces().front())->front());

    std::vector<app::MoveVertices::Moved> moved{
        {vertex, math::Vec3{0.0F, 0.0F, 0.0F}, math::Vec3{9.0F, 0.0F, 0.0F}}};

    REQUIRE(target.mesh(mesh)->remove_vertex(vertex));

    CommandStack stack;
    stack.execute(std::make_unique<app::MoveVertices>(target, mesh, moved));

    CHECK(stack.undo());
}

TEST_CASE("an extrusion is undone by putting the mesh back") {
    Scene target;
    const scene::MeshId mesh = target.add_mesh(geom::make_box());

    const std::size_t faces_before = target.mesh(mesh)->face_count();
    const std::size_t vertices_before = target.mesh(mesh)->vertex_count();

    geom::EditMesh before = target.mesh(mesh)->clone();
    geom::EditMesh after = before.clone();
    geom::extrude(after, {after.faces().front()});

    CommandStack stack;
    stack.execute(std::make_unique<app::ReplaceMesh>(target, mesh, std::move(before),
                                                     std::move(after), "extrude"));

    CHECK(target.mesh(mesh)->face_count() == faces_before + 4);
    CHECK(stack.undo_name() == "extrude");

    // Elements were created and destroyed, so there is no single value to
    // restore; the whole mesh goes back.
    REQUIRE(stack.undo());
    CHECK(target.mesh(mesh)->face_count() == faces_before);
    CHECK(target.mesh(mesh)->vertex_count() == vertices_before);
    CHECK(target.mesh(mesh)->validate().empty());

    REQUIRE(stack.redo());
    CHECK(target.mesh(mesh)->face_count() == faces_before + 4);
    CHECK(target.mesh(mesh)->validate().empty());
}

TEST_CASE("the history walks back through a whole session in order") {
    Scene target;
    const scene::NodeId node = target.add_node("node");
    const scene::MeshId mesh = target.add_mesh(geom::make_box());
    target.set_mesh(node, mesh);

    const geom::VertexId vertex = target.mesh(mesh)->corner_vertex(
        target.mesh(mesh)->face_corners(target.mesh(mesh)->faces().front())->front());
    const math::Vec3 position = target.mesh(mesh)->position(vertex);

    CommandStack stack;

    scene::Transform moved;
    moved.translation = math::Vec3{0.0F, 1.0F, 0.0F};
    stack.execute(std::make_unique<app::TransformNode>(target, node, scene::Transform{}, moved));

    stack.execute(std::make_unique<app::MoveVertices>(
        target, mesh,
        std::vector<app::MoveVertices::Moved>{
            {vertex, position, position + math::Vec3{1.0F, 0.0F, 0.0F}}}));

    geom::EditMesh before = target.mesh(mesh)->clone();
    geom::EditMesh after = before.clone();
    geom::extrude(after, {after.faces().front()});
    stack.execute(std::make_unique<app::ReplaceMesh>(target, mesh, std::move(before),
                                                     std::move(after), "extrude"));

    CHECK(stack.depth() == 3);

    while (stack.can_undo()) {
        REQUIRE(stack.undo());
    }

    // Back to exactly where the session started.
    CHECK(target.node(node)->transform.translation.y == doctest::Approx(0.0F));
    CHECK(target.mesh(mesh)->position(vertex).x == doctest::Approx(position.x));
    CHECK(target.mesh(mesh)->face_count() == 6);
    CHECK(target.mesh(mesh)->vertex_count() == 8);
}
