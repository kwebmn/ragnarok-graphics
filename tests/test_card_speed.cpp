// 3.0.4-beta.35, 3.0.11-beta.58 (the autopilot, the owner: «делай»): the card judged by how it draws - the automatic
// set moved over the whole ladder (0 «Без ефектів» .. 3 «Високі») to the highest one it holds at 60 frames
#include <string>

#include "core/card_speed.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// n windows at (fps, the card's ms) with the limit 60: the last one's step
card_speed::Step feed(card_speed::Judge& j, int n, double fps, double gpuMs, int slowTarget = 0) {
    card_speed::Step last = card_speed::Step::None;
    for (int k = 0; k < n; ++k) last = j.window(fps, 60, gpuMs, slowTarget);
    return last;
}

}  // namespace

TEST(card_speed_room_is_half_a_frame) {
    // a Radeon 780M - weak by its 417 MB of memory of its own - drew the weak set in 5.4-7.8 ms of a 60 frames' 16.7
    // ms; an HD 6570 in 10-21 ms
    CHECK(card_speed::hasRoom(60, 7.8));
    CHECK(!card_speed::hasRoom(60, 10.3));
    CHECK(card_speed::hasRoom(30, 15.0));   // a 30 frames' limit: 33 ms a frame
    CHECK(!card_speed::hasRoom(60, -1.0));  // not measured: no room known
}

TEST(card_speed_raises_any_level_below_the_top) {
    for (int level = 0; level < card_speed::kTopLevel; ++level) {
        card_speed::Judge j(level);
        CHECK(feed(j, card_speed::kRaiseWindows - 1, 60.0, 6.5) == card_speed::Step::None);
        CHECK(j.window(60.0, 60, 6.5) == card_speed::Step::Raise);  // a minute with room: the next set on trial
        CHECK_EQ(j.level(), level + 1);
        CHECK(j.trying());
    }
    card_speed::Judge top(card_speed::kTopLevel);
    CHECK(feed(top, 100, 60.0, 2.0) == card_speed::Step::None);  // nothing above «Високі»
    CHECK_EQ(top.level(), card_speed::kTopLevel);
    card_speed::Judge busy(1);  // a window without room starts the minute again
    feed(busy, card_speed::kRaiseWindows - 1, 60.0, 6.5);
    CHECK(busy.window(58.0, 60, 12.0) == card_speed::Step::None);
    CHECK(feed(busy, card_speed::kRaiseWindows - 1, 60.0, 6.5) == card_speed::Step::None);
    CHECK(busy.window(60.0, 60, 6.5) == card_speed::Step::Raise);
}

TEST(card_speed_keeps_a_trial_and_settles_at_the_top) {
    card_speed::Judge j(1);
    feed(j, card_speed::kRaiseWindows, 60.0, 6.5);
    CHECK(feed(j, card_speed::kTrialWindows - 1, 60.0, 11.5) == card_speed::Step::None);
    CHECK(j.window(60.0, 60, 11.5) == card_speed::Step::Keep);
    CHECK_EQ(j.level(), 2);
    CHECK(!j.trying());
    CHECK(!j.settled());  // under the top: a later minute with room tries the next
    CHECK(feed(j, card_speed::kRaiseWindows, 60.0, 6.5) == card_speed::Step::Raise);
    CHECK_EQ(j.level(), 3);
    CHECK(feed(j, card_speed::kTrialWindows, 60.0, 9.0) == card_speed::Step::Keep);
    CHECK_EQ(j.level(), 3);
    CHECK(j.settled());                                          // the top kept: judged
    CHECK(feed(j, 50, 20.0, 30.0) == card_speed::Step::None);  // keep_fps's from here
}

TEST(card_speed_backs_off_a_failed_trial) {
    card_speed::Judge j(1);
    feed(j, card_speed::kRaiseWindows, 60.0, 6.5);
    CHECK(j.window(41.0, 60, 15.9) == card_speed::Step::None);  // the card holding the game back once
    CHECK(j.window(40.0, 60, 16.0) == card_speed::Step::Back);  // twice in a row: the set before
    CHECK_EQ(j.level(), 1);
    CHECK(j.settled());
    CHECK(!j.trying());
    CHECK(feed(j, 50, 60.0, 2.0) == card_speed::Step::None);  // not tried again
    CHECK_EQ(j.level(), 1);
}

TEST(card_speed_lowers_a_steady_slow_card_once_unsettled) {
    card_speed::Judge j(3);  // an old card of 8 GB, too slow for «Високі»
    CHECK(feed(j, card_speed::kLowerWindows - 1, 40.0, 20.0) == card_speed::Step::None);
    CHECK(j.window(40.0, 60, 20.0) == card_speed::Step::Lower);
    CHECK_EQ(j.level(), 2);
    CHECK(j.settled());
    CHECK(feed(j, 50, 40.0, 20.0) == card_speed::Step::None);  // judged: keep_fps's from here
    CHECK_EQ(j.level(), 2);
    card_speed::Judge none(0);
    CHECK(feed(none, 50, 30.0, 30.0) == card_speed::Step::None);  // nothing under «Без ефектів»
    CHECK_EQ(none.level(), 0);
    card_speed::Judge broken(1);  // a window the card keeps up breaks the run
    feed(broken, card_speed::kLowerWindows - 1, 40.0, 20.0);
    CHECK(broken.window(60.0, 60, 9.0) == card_speed::Step::None);
    CHECK(feed(broken, card_speed::kLowerWindows - 1, 40.0, 20.0) == card_speed::Step::None);
    CHECK(broken.window(40.0, 60, 20.0) == card_speed::Step::Lower);
    CHECK_EQ(broken.level(), 0);
}

TEST(card_speed_a_town_slow_for_the_processor_is_no_step) {
    // 40 frames with the card 5 ms a frame: the processor's crowd - no set lowered, none raised (more effects would
    // add the driver's own processor work there)
    card_speed::Judge j(2);
    CHECK(feed(j, 100, 40.0, 5.0) == card_speed::Step::None);
    CHECK_EQ(j.level(), 2);
    CHECK(!j.settled());
}

TEST(card_speed_trial_by_the_players_keep_fps) {
    // the trial's bar keep_fps's when the player set it (45 of R-EQDYNE's): 44 frames with the card 16 ms is the
    // card holding back a 60's limit, not a 45's
    card_speed::Judge j(1);
    feed(j, card_speed::kRaiseWindows, 60.0, 6.5, 45);
    CHECK(feed(j, 2, 44.0, 16.0, 45) == card_speed::Step::None);
    CHECK_EQ(j.level(), 2);
    CHECK(j.window(30.0, 60, 30.0, 45) == card_speed::Step::None);
    CHECK(j.window(30.0, 60, 30.0, 45) == card_speed::Step::Back);  // under 45 with the card 30 ms: back
}

TEST(card_speed_record_round_trip_and_old_format) {
    // uaro-graphics\card-level.txt: written by the driver at each verdict, read at the start by it and the app
    const std::string text = card_speed::format({2, true, "AMD Radeon 780M Graphics"});
    CHECK(text == "v2 2 1 AMD Radeon 780M Graphics\n");
    card_speed::Record r;
    CHECK(card_speed::parse(text, r));
    CHECK_EQ(r.level, 2);
    CHECK(r.settled);
    CHECK(r.adapter == "AMD Radeon 780M Graphics");
    CHECK(card_speed::parse(card_speed::format({0, false, "Intel(R) UHD Graphics 620"}), r));
    CHECK_EQ(r.level, 0);
    CHECK(!r.settled);
    CHECK(card_speed::parse("2 AMD Radeon HD 6570\r\n", r));  // 3.0.4-beta.35's: its level, judged again
    CHECK_EQ(r.level, 2);
    CHECK(!r.settled);
    CHECK(r.adapter == "AMD Radeon HD 6570");
    CHECK(!card_speed::parse("5 X", r));
    CHECK(!card_speed::parse("v2 9 0 X", r));
    CHECK(!card_speed::parse("v2 2 7 X", r));
    CHECK(!card_speed::parse("v2 2 1 ", r));  // no card
    CHECK(!card_speed::parse("", r));
    CHECK(!card_speed::parse("junk", r));
}

TEST(card_speed_start_of_this_card_only) {
    const std::string text = card_speed::format({3, true, "NVIDIA GeForce GTX 1050 Ti"});
    card_speed::Start s = card_speed::startOf(2, "NVIDIA GeForce GTX 1050 Ti", text);
    CHECK_EQ(s.level, 3);
    CHECK(s.settled);
    s = card_speed::startOf(2, "NVIDIA GeForce RTX 3060", text);  // another card: its memory's level, judged again
    CHECK_EQ(s.level, 2);
    CHECK(!s.settled);
    s = card_speed::startOf(1, "x", "");
    CHECK_EQ(s.level, 1);
    CHECK(!s.settled);
    s = card_speed::startOf(1, "", text);  // the card not known
    CHECK_EQ(s.level, 1);
    CHECK(!s.settled);
}
