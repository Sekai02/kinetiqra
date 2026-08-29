#include <kinetiqra/core/Arena.hpp>

#include <doctest/doctest.h>

#include <string>

using kinetiqra::core::Arena;
using kinetiqra::core::Handle;

namespace {

struct ThingTag {};

struct OtherTag {};

using ThingId = Handle<ThingTag>;
using OtherId = Handle<OtherTag>;

}  // namespace

TEST_CASE("a default handle is invalid and resolves to nothing") {
    Arena<int, ThingTag> arena;

    CHECK_FALSE(ThingId{}.valid());
    CHECK_FALSE(arena.contains(ThingId{}));
    CHECK(arena.get(ThingId{}) == nullptr);
}

TEST_CASE("an inserted element resolves through its handle") {
    Arena<std::string, ThingTag> arena;

    const ThingId id = arena.insert("mesh");

    REQUIRE(arena.contains(id));
    REQUIRE(arena.get(id) != nullptr);
    CHECK(*arena.get(id) == "mesh");
    CHECK(arena.size() == 1);
}

TEST_CASE("handles survive later insertions") {
    Arena<int, ThingTag> arena;

    const ThingId first = arena.insert(1);
    for (int i = 0; i < 100; ++i) {
        arena.insert(i);
    }

    REQUIRE(arena.get(first) != nullptr);
    CHECK(*arena.get(first) == 1);
}

TEST_CASE("a handle kept across its removal is rejected") {
    Arena<int, ThingTag> arena;

    const ThingId id = arena.insert(7);
    REQUIRE(arena.remove(id));

    CHECK_FALSE(arena.contains(id));
    CHECK(arena.get(id) == nullptr);
    CHECK(arena.size() == 0);

    // Removing twice is reported rather than corrupting the free list.
    CHECK_FALSE(arena.remove(id));
}

TEST_CASE("a reused slot does not resurrect the old handle") {
    Arena<int, ThingTag> arena;

    const ThingId first = arena.insert(1);
    REQUIRE(arena.remove(first));

    const ThingId second = arena.insert(2);

    // The point of the generation counter: the same slot, a different element.
    CHECK(second.index == first.index);
    CHECK(second.generation != first.generation);

    CHECK_FALSE(arena.contains(first));
    REQUIRE(arena.contains(second));
    CHECK(*arena.get(second) == 2);
}

TEST_CASE("removal frees the slot rather than growing the arena") {
    Arena<int, ThingTag> arena;

    const ThingId id = arena.insert(1);
    arena.remove(id);
    arena.insert(2);

    CHECK(arena.slot_count() == 1);
    CHECK(arena.size() == 1);
}

TEST_CASE("live slots can be walked by index") {
    Arena<int, ThingTag> arena;

    const ThingId a = arena.insert(10);
    const ThingId b = arena.insert(20);
    const ThingId c = arena.insert(30);
    arena.remove(b);

    int seen = 0;
    int total = 0;
    for (std::uint32_t i = 0; i < arena.slot_count(); ++i) {
        if (!arena.alive(i)) {
            continue;
        }
        ++seen;
        total += *arena.get(arena.id_at(i));
    }

    CHECK(seen == 2);
    CHECK(total == 40);
    CHECK(arena.id_at(a.index) == a);
    CHECK(arena.id_at(c.index) == c);
}

TEST_CASE("handles of different kinds are different types") {
    // The guarantee is a compile-time one, so what is asserted here is that the
    // types are distinct rather than aliases of each other. Passing an OtherId
    // where a ThingId belongs would not compile, which no runtime check can say.
    CHECK_FALSE(std::is_same_v<ThingId, OtherId>);
}
