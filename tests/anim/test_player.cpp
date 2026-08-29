#include <kinetiqra/anim/Player.hpp>

#include <doctest/doctest.h>

using kinetiqra::anim::Player;

TEST_CASE("a stopped player does not move") {
    Player player;
    player.set_duration(2.0F);

    player.advance(1.0F);

    CHECK(player.time() == doctest::Approx(0.0F));
    CHECK_FALSE(player.playing());
}

TEST_CASE("playing advances by the elapsed time") {
    Player player;
    player.set_duration(10.0F);
    player.play();

    player.advance(0.25F);
    player.advance(0.25F);

    CHECK(player.time() == doctest::Approx(0.5F));
}

TEST_CASE("speed scales the advance") {
    Player player;
    player.set_duration(10.0F);
    player.set_speed(2.0F);
    player.play();

    player.advance(1.0F);

    CHECK(player.time() == doctest::Approx(2.0F));
}

TEST_CASE("looping wraps back to the start") {
    Player player;
    player.set_duration(2.0F);
    player.set_looping(true);
    player.play();

    player.advance(2.5F);

    CHECK(player.time() == doctest::Approx(0.5F));
    CHECK(player.playing());
}

TEST_CASE("looping backwards reappears at the end") {
    Player player;
    player.set_duration(2.0F);
    player.set_looping(true);
    player.set_speed(-1.0F);
    player.play();

    player.advance(0.5F);

    CHECK(player.time() == doctest::Approx(1.5F));
    CHECK(player.playing());
}

TEST_CASE("without looping it stops at the end") {
    Player player;
    player.set_duration(2.0F);
    player.set_looping(false);
    player.play();

    player.advance(5.0F);

    CHECK(player.time() == doctest::Approx(2.0F));
    CHECK_FALSE(player.playing());
}

TEST_CASE("without looping it stops at the start when running backwards") {
    Player player;
    player.set_duration(2.0F);
    player.set_looping(false);
    player.set_speed(-1.0F);
    player.set_time(1.0F);
    player.play();

    player.advance(5.0F);

    CHECK(player.time() == doctest::Approx(0.0F));
    CHECK_FALSE(player.playing());
}

TEST_CASE("pressing play at the end rewinds") {
    Player player;
    player.set_duration(2.0F);
    player.set_looping(false);
    player.play();
    player.advance(5.0F);
    REQUIRE_FALSE(player.playing());

    player.play();

    // Otherwise the button would appear to do nothing on a finished clip.
    CHECK(player.time() == doctest::Approx(0.0F));
    CHECK(player.playing());
}

TEST_CASE("scrubbing is clamped to the clip") {
    Player player;
    player.set_duration(2.0F);

    player.set_time(-1.0F);
    CHECK(player.time() == doctest::Approx(0.0F));

    player.set_time(99.0F);
    CHECK(player.time() == doctest::Approx(2.0F));
}

TEST_CASE("shortening the clip pulls the time back inside it") {
    Player player;
    player.set_duration(10.0F);
    player.set_time(8.0F);

    // Loading a shorter clip must not leave the playhead past its end.
    player.set_duration(3.0F);

    CHECK(player.time() == doctest::Approx(3.0F));
}

TEST_CASE("a clip with no duration never advances") {
    Player player;
    player.set_duration(0.0F);
    player.play();

    player.advance(1.0F);

    CHECK(player.time() == doctest::Approx(0.0F));
}

TEST_CASE("stopping rewinds and pausing does not") {
    Player player;
    player.set_duration(4.0F);
    player.play();
    player.advance(2.0F);

    player.pause();
    CHECK(player.time() == doctest::Approx(2.0F));
    CHECK_FALSE(player.playing());

    player.play();
    player.stop();
    CHECK(player.time() == doctest::Approx(0.0F));
    CHECK_FALSE(player.playing());
}
