#include <kinetiqra/app/Selection.hpp>
#include <kinetiqra/geom/Primitives.hpp>

#include <doctest/doctest.h>

#include <algorithm>

namespace app = kinetiqra::app;
namespace geom = kinetiqra::geom;
namespace scene = kinetiqra::scene;

using kinetiqra::app::EditMode;
using kinetiqra::app::ElementKind;
using kinetiqra::app::Selection;

TEST_CASE("a new selection is on whole objects and holds nothing") {
    const Selection selection;

    CHECK(selection.mode() == EditMode::Object);
    CHECK(selection.kind() == ElementKind::Vertex);
    CHECK_FALSE(selection.node().valid());
    CHECK(selection.empty());
}

TEST_CASE("clicking adds and shift clicking takes away") {
    const geom::EditMesh mesh = geom::make_box();
    const geom::FaceId first = mesh.faces()[0];
    const geom::FaceId second = mesh.faces()[1];

    Selection selection;
    selection.select_only(first);
    CHECK(selection.faces().size() == 1);
    CHECK(selection.contains(first));

    selection.toggle(second);
    CHECK(selection.faces().size() == 2);

    selection.toggle(first);
    CHECK(selection.faces().size() == 1);
    CHECK_FALSE(selection.contains(first));
    CHECK(selection.contains(second));
}

TEST_CASE("selecting only one thing replaces what was there") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.toggle(mesh.faces()[0]);
    selection.toggle(mesh.faces()[1]);
    selection.select_only(mesh.faces()[2]);

    CHECK(selection.faces().size() == 1);
    CHECK(selection.contains(mesh.faces()[2]));
}

TEST_CASE("moving to another node drops the elements of the one being left") {
    scene::Scene target;
    const scene::NodeId first = target.add_node("first");
    const scene::NodeId second = target.add_node("second");

    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_node(first);
    selection.select_only(mesh.faces().front());
    REQUIRE_FALSE(selection.empty());

    // They belonged to the other node's mesh, and keeping them would leave the
    // selection pointing into a mesh nobody is looking at.
    selection.set_node(second);
    CHECK(selection.empty());
}

TEST_CASE("selecting the same node again leaves the elements alone") {
    scene::Scene target;
    const scene::NodeId node = target.add_node("node");
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_node(node);
    selection.select_only(mesh.faces().front());

    selection.set_node(node);
    CHECK(selection.faces().size() == 1);
}

TEST_CASE("changing mode or kind clears the elements") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_mode(EditMode::Mesh);
    selection.select_only(mesh.faces().front());

    // Vertices and faces are not translations of each other, so turning one
    // selection into the other would be a guess at what was meant.
    selection.set_kind(ElementKind::Face);
    CHECK(selection.empty());

    selection.set_kind(ElementKind::Vertex);
    selection.select_only(mesh.vertices().front());
    REQUIRE_FALSE(selection.empty());

    selection.set_mode(EditMode::Object);
    CHECK(selection.empty());
}

TEST_CASE("setting the mode it is already in changes nothing") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_mode(EditMode::Mesh);
    selection.select_only(mesh.vertices().front());

    selection.set_mode(EditMode::Mesh);
    CHECK(selection.vertices().size() == 1);
}

TEST_CASE("in vertex mode the vertices that move are the ones selected") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_kind(ElementKind::Vertex);
    selection.select_only(mesh.vertices()[0]);
    selection.toggle(mesh.vertices()[1]);

    CHECK(selection.moving_vertices(mesh).size() == 2);
}

TEST_CASE("in face mode the vertices that move are the ones the faces are built from") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_kind(ElementKind::Face);
    selection.select_only(mesh.faces().front());

    // A quad, so four of the box's eight.
    CHECK(selection.moving_vertices(mesh).size() == 4);
}

TEST_CASE("a vertex shared by two selected faces moves once, not twice") {
    const geom::EditMesh mesh = geom::make_box();

    Selection selection;
    selection.set_kind(ElementKind::Face);
    selection.select_only(mesh.faces()[0]);
    selection.toggle(mesh.faces()[2]);

    const std::vector<geom::VertexId> moving = selection.moving_vertices(mesh);

    // Two adjacent faces of a box share an edge, so six corners rather than
    // eight. Counting a shared vertex twice would move it twice as far.
    CHECK(moving.size() == 6);

    std::vector<geom::VertexId> unique = moving;
    std::sort(unique.begin(), unique.end(),
              [](geom::VertexId a, geom::VertexId b) { return a.index < b.index; });
    CHECK(std::adjacent_find(unique.begin(), unique.end()) == unique.end());
}

TEST_CASE("elements that have stopped existing are not moved") {
    geom::EditMesh mesh = geom::make_box();
    const geom::VertexId vertex = mesh.vertices().front();

    Selection selection;
    selection.set_kind(ElementKind::Vertex);
    selection.select_only(vertex);

    REQUIRE(mesh.remove_vertex(vertex));

    // The selection still names it, and the mesh is the one asked whether it
    // is still there.
    CHECK(selection.moving_vertices(mesh).empty());
}
