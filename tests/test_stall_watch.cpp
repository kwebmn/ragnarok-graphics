// 3.0.11-beta.94 (R-3S33MK «чтото лагает игра»: the game's thread stood still 15 s and 5 s - Windows put its «not
// responding» ghost over the window - and the log did not say where): the game's thread watched by its message loop's
// beats (core/stall_watch) - a second without one begins a stall, the next beat ends it
#include "core/stall_watch.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(stall_watch_a_second_still_begins_a_stall_and_a_beat_ends_it) {
    stallwatch::Watch w;
    CHECK(w.look(5, 0) == stallwatch::Event::None);
    CHECK(w.look(5, 999) == stallwatch::Event::None);
    CHECK(w.look(5, 1000) == stallwatch::Event::Began);
    CHECK(w.still());
    CHECK(w.look(5, 9000) == stallwatch::Event::None);  // still: begun once
    CHECK(w.look(6, 9250) == stallwatch::Event::Ended);
    CHECK_EQ(w.stillMs(), uint64_t(9250));  // from the last beat seen before it
    CHECK(!w.still());
    CHECK(w.look(7, 9500) == stallwatch::Event::None);
}

TEST(stall_watch_beats_keep_it_quiet) {
    stallwatch::Watch w;
    for (uint32_t i = 0; i < 100; ++i) CHECK(w.look(i, uint64_t(i) * 900) == stallwatch::Event::None);
    CHECK(!w.still());
}

TEST(stall_watch_counts_from_the_first_look) {
    stallwatch::Watch w;
    CHECK(w.look(40, 50000) == stallwatch::Event::None);  // the first look: no stall before it
    CHECK(w.look(40, 50999) == stallwatch::Event::None);
    CHECK(w.look(40, 51000) == stallwatch::Event::Began);
}
