#include <vector>

#include "core/item_pick.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.155 (the owner: several things of the storage picked with Ctrl and dragged at once, not one
// by one): the things picked by their places - Ctrl+click one on or off, Shift+click all between the last and
// this one (in the order shown), Ctrl+A all shown; those gone from the window dropped
TEST(item_pick_ctrl_shift_all_and_gone) {
    itempick::Picked p;
    const std::vector<int> shown = {10, 11, 12, 13, 14};
    p.toggle(11);
    p.toggle(13);
    CHECK(p.has(11) && p.has(13) && !p.has(12));
    p.toggle(13);
    CHECK(!p.has(13));
    p.range(shown, 14);  // from the last one clicked (13, let go) to 14
    CHECK(p.has(11) && p.has(13) && p.has(14) && !p.has(12) && !p.has(10));
    CHECK_EQ(p.count(), size_t(3));
    p.toggle(10);
    p.range(shown, 12);  // 10 .. 12
    CHECK(p.has(10) && p.has(11) && p.has(12));
    p.keepOnly({10, 11, 12});  // 13 and 14 taken
    CHECK_EQ(p.count(), size_t(3));
    p.all(shown);
    CHECK_EQ(p.count(), size_t(5));
    p.clear();
    CHECK_EQ(p.count(), size_t(0));
}

// 3.0.22-beta.7 (a player: Alt+right click moves a thing without dragging; the owner: «делай» - a stack whole, no
// «how many»): a batch moves one thing a step; the game's «how many» a stack's move brings is answered «all of it» as
// soon as it is up - the last one's too (before, the last one's was left to the player)
TEST(item_pick_pace_answers_the_last_ones_count) {
    itempick::Pace p;
    std::vector<int> queue = {7, 8}, moved;
    bool countUp = false;  // the game's «how many» shown
    int answered = 0;
    const auto answer = [&] {
        if (!countUp) return false;
        countUp = false;
        ++answered;
        return true;
    };
    const auto move = [&] {  // a stack: its move brings a «how many»
        moved.push_back(queue.front());
        queue.erase(queue.begin());
        countUp = true;
    };
    uint64_t t = 1000;
    CHECK(itempick::step(p, t, queue.size(), answer, move));  // 7 moved
    CHECK(moved == std::vector<int>{7});
    CHECK(itempick::step(p, t += 16, queue.size(), answer, move));  // its «how many» answered the next frame
    CHECK_EQ(answered, 1);
    CHECK(itempick::step(p, t += 16, queue.size(), answer, move));  // the next thing a step later
    CHECK_EQ(moved.size(), size_t(1));
    CHECK(itempick::step(p, t += itempick::kStepMs, queue.size(), answer, move));  // 8 moved - the last
    CHECK(moved == (std::vector<int>{7, 8}));
    CHECK(!itempick::step(p, t += 16, queue.size(), answer, move));  // the last one's answered too: over
    CHECK_EQ(answered, 2);
    countUp = true;  // a «how many» of the player's own after the batch: left to him
    CHECK(!itempick::step(p, t += 16, queue.size(), answer, move));
    CHECK_EQ(answered, 2);
}

// A last thing that is not a stack brings no «how many»: awaited kLastWaitMs at most; no batch asks nothing
TEST(item_pick_pace_waits_for_the_last_ones_count_a_while) {
    itempick::Pace p;
    size_t left = 1;
    int asked = 0;
    const auto answer = [&] {
        ++asked;
        return false;
    };
    const auto move = [&] { --left; };
    CHECK(itempick::step(p, 5000, left, answer, move));
    CHECK_EQ(left, size_t(0));
    CHECK(itempick::step(p, 5000 + itempick::kLastWaitMs - 1, left, answer, move));  // still awaited
    CHECK(!itempick::step(p, 5000 + itempick::kLastWaitMs, left, answer, move));     // no longer
    const int before = asked;
    CHECK(!itempick::step(p, 9000, left, answer, move));
    CHECK_EQ(asked, before);
    itempick::Pace idle;
    CHECK(!itempick::step(idle, 100, 0, answer, move));
    CHECK_EQ(asked, before);
}
