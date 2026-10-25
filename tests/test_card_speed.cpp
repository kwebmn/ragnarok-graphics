#include <string>

#include "core/card_speed.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(card_speed_a_card_with_room_is_one_whose_work_takes_half_a_frame) {
    // 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»): a Radeon 780M - weak by its 417 MB
    // of memory of its own - drew the weak set in 5.4-7.8 ms of a 60 frames' 16.7 ms; an HD 6570 in 10-21 ms
    CHECK(card_speed::hasRoom(60, 7.8));
    CHECK(!card_speed::hasRoom(60, 10.3));
    CHECK(card_speed::hasRoom(30, 15.0));   // a 30 frames' limit: 33 ms a frame
    CHECK(!card_speed::hasRoom(60, -1.0));  // not measured: no room known
}

TEST(card_speed_raises_a_weak_card_that_keeps_room_for_a_minute) {
    card_speed::Judge j(1);
    for (int k = 0; k + 1 < card_speed::kRaiseWindows; ++k) CHECK(j.window(60.0, 60, 6.5) == card_speed::Step::None);
    CHECK(j.window(60.0, 60, 6.5) == card_speed::Step::Raise);
    CHECK_EQ(j.level(), 2);
    card_speed::Judge busy(1);  // a window without room starts the minute again
    for (int k = 0; k < card_speed::kRaiseWindows - 1; ++k) busy.window(60.0, 60, 6.5);
    CHECK(busy.window(48.0, 60, 12.0) == card_speed::Step::None);
    for (int k = 0; k + 1 < card_speed::kRaiseWindows; ++k) CHECK(busy.window(60.0, 60, 6.5) == card_speed::Step::None);
    CHECK(busy.window(60.0, 60, 6.5) == card_speed::Step::Raise);
}

TEST(card_speed_keeps_the_medium_set_unless_the_card_holds_the_game_back) {
    card_speed::Judge kept(1);
    for (int k = 0; k < card_speed::kRaiseWindows; ++k) kept.window(60.0, 60, 6.5);
    for (int k = 0; k + 1 < card_speed::kTrialWindows; ++k) {
        CHECK(kept.window(k % 2 ? 44.0 : 60.0, 60, 11.5) == card_speed::Step::None);  // slow for the processor: fine
    }
    CHECK(kept.window(60.0, 60, 11.5) == card_speed::Step::Keep);
    CHECK_EQ(kept.level(), 2);
    CHECK(kept.window(20.0, 60, 30.0) == card_speed::Step::None);  // decided: no more steps
    card_speed::Judge back(1);
    for (int k = 0; k < card_speed::kRaiseWindows; ++k) back.window(60.0, 60, 6.5);
    CHECK(back.window(41.0, 60, 15.9) == card_speed::Step::None);  // the card's: once
    CHECK(back.window(40.0, 60, 16.0) == card_speed::Step::Lower);  // twice in a row: back to the weak set
    CHECK_EQ(back.level(), 1);
    CHECK(back.window(60.0, 60, 2.0) == card_speed::Step::None);  // not tried again
}

TEST(card_speed_leaves_a_card_weak_or_strong_by_memory_alone) {
    card_speed::Judge medium(2), settled(1, true);
    for (int k = 0; k < 100; ++k) {
        CHECK(medium.window(60.0, 60, 2.0) == card_speed::Step::None);
        CHECK(settled.window(60.0, 60, 2.0) == card_speed::Step::None);
    }
}

TEST(card_speed_remembers_its_verdict_for_the_card) {
    // uaro-graphics\card-level.txt: the level and the card's name - read at the start by the driver and the app
    const std::string text = card_speed::format({2, "AMD Radeon 780M Graphics"});
    card_speed::Record r;
    CHECK(card_speed::parse(text, r));
    CHECK_EQ(r.level, 2);
    CHECK(r.adapter == "AMD Radeon 780M Graphics");
    CHECK_EQ(card_speed::levelAt(1, "AMD Radeon 780M Graphics", text), 2);
    CHECK(card_speed::decided(1, "AMD Radeon 780M Graphics", text));
    CHECK_EQ(card_speed::levelAt(1, "AMD Radeon HD 6570", text), 1);  // another card: its memory's level
    CHECK(!card_speed::decided(1, "AMD Radeon HD 6570", text));
    CHECK_EQ(card_speed::levelAt(3, "AMD Radeon 780M Graphics", text), 3);  // only a weak card's level moves
    const std::string tried = card_speed::format({1, "Intel(R) UHD Graphics 620"});
    CHECK_EQ(card_speed::levelAt(1, "Intel(R) UHD Graphics 620", tried), 1);
    CHECK(card_speed::decided(1, "Intel(R) UHD Graphics 620", tried));  // tried and too slow: not again
    CHECK(!card_speed::parse("", r));
    CHECK(!card_speed::parse("7 x", r));
    CHECK_EQ(card_speed::levelAt(1, "x", "junk"), 1);
}

TEST(card_speed_tries_the_medium_set_by_the_players_own_frame_rate) {
    // the trial's bar keep_fps's when the player set it (45 of R-EQDYNE's): 44 frames with the card 16 ms is the
    // card holding back a 60's limit, not a 45's
    card_speed::Judge j(1);
    for (int k = 0; k < card_speed::kRaiseWindows; ++k) j.window(60.0, 60, 6.5, 45);
    CHECK(j.window(44.0, 60, 16.0, 45) == card_speed::Step::None);
    CHECK(j.window(44.0, 60, 16.0, 45) == card_speed::Step::None);
    CHECK_EQ(j.level(), 2);
    CHECK(j.window(30.0, 60, 30.0, 45) == card_speed::Step::None);
    CHECK(j.window(30.0, 60, 30.0, 45) == card_speed::Step::Lower);  // under 45 with the card 30 ms: back
}
