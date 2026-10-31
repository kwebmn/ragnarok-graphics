#include "pad/core/logic/mouse_gate.hpp"
#include "test.hpp"

using namespace uapad;

namespace {

constexpr uint8_t kLmb = 1;
constexpr uint8_t kRmb = 2;

using Ev = MouseKeys::KeyEvent;

bool same(const std::vector<Ev>& a, const std::vector<Ev>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].key != b[i].key || a[i].down != b[i].down) return false;
    return true;
}

}  // namespace

TEST(gate_wheel_only_over_the_game) {
    MouseGate g;
    CHECK(!g.filter(0, false, false, 0).wheel);
    CHECK(g.filter(0, true, false, 0).wheel);
}

TEST(gate_press_outside_is_hidden_until_let_go) {
    MouseGate g;
    CHECK_EQ(g.filter(kLmb, false, false, 0).buttons, 0);  // a click on the title bar or another window
    CHECK_EQ(g.filter(kLmb, true, false, 10).buttons, 0);  // still the same press, now over the game
    CHECK_EQ(g.filter(0, true, false, 20).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 30).buttons, kLmb);  // a new press over the game
}

TEST(gate_drag_started_in_the_game_goes_on_outside) {
    MouseGate g;
    CHECK_EQ(g.filter(kRmb, true, false, 0).buttons, kRmb);
    CHECK_EQ(g.filter(kRmb, false, false, 10).buttons, kRmb);
    CHECK_EQ(g.filter(kRmb | kLmb, false, false, 20).buttons, kRmb);  // LMB went down outside
    CHECK_EQ(g.filter(0, false, false, 30).buttons, 0);
}

TEST(gate_the_activating_click_stays_out) {
    MouseGate g;
    g.watch(true, 1000);  // the game window comes to the front
    CHECK_EQ(g.filter(kLmb, true, false, 1000).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 1200).buttons, 0);  // still that press
    CHECK_EQ(g.filter(0, true, false, 1250).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 1500).buttons, kLmb);
    MouseGate passing;
    passing.watch(true, 1000);
    CHECK_EQ(passing.filter(kLmb, true, true, 1000).buttons, kLmb);  // focus_click = 1: as before
}

TEST(gate_activating_click_on_the_first_poll_in_front) {
    MouseGate g;  // the polls stop while the game is behind: a tick saw it go, the next poll sees it back
    g.watch(true, 0);
    CHECK_EQ(g.filter(0, true, false, 500).buttons, 0);
    g.watch(false, 1000);
    g.watch(true, 2000);
    CHECK_EQ(g.filter(kLmb, true, false, 2000).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 2016).buttons, 0);
}

TEST(gate_activation_window_is_short) {
    MouseGate g;
    g.watch(true, 1000);
    g.watch(true, 1100);  // still in front: no new activation
    CHECK_EQ(g.filter(kLmb, true, false, 1400).buttons, kLmb);
}

TEST(gate_going_behind_forgets_held_buttons) {
    MouseGate g;
    g.watch(true, 0);
    CHECK_EQ(g.filter(kLmb, false, false, 1000).buttons, 0);  // went down outside: hidden
    g.watch(false, 1100);
    g.watch(true, 5000);
    CHECK_EQ(g.filter(kLmb, true, false, 5400).buttons, kLmb);  // a new press, not the old hidden one
}

TEST(gate_reset_forgets_held_buttons) {
    MouseGate g;
    g.filter(kLmb, false, false, 0);
    g.reset();
    CHECK_EQ(g.filter(kLmb, true, false, 10).buttons, kLmb);
}

TEST(keys_a_side_button_holds_its_key) {
    MouseKeys k;
    const std::array<int, 3> keys{3, 0, 0};  // middle: F3
    CHECK(same(k.update(1, keys, true, true), {Ev{3, true}}));
    CHECK(k.update(1, keys, true, true).empty());
    CHECK(same(k.update(0, keys, true, true), {Ev{3, false}}));
    CHECK(k.update(2, keys, true, true).empty());  // button 4 has no key
}

TEST(keys_need_a_press_while_allowed) {
    MouseKeys k;
    const std::array<int, 3> keys{0, 1, 0};
    CHECK(k.update(2, keys, false, true).empty());  // the menu is open or the cursor is elsewhere
    CHECK(k.update(2, keys, true, true).empty());   // the same press
    CHECK(k.update(0, keys, true, true).empty());
    CHECK(same(k.update(2, keys, true, true), {Ev{1, true}}));
}

TEST(keys_let_go_when_the_game_loses_the_foreground) {
    MouseKeys k;
    const std::array<int, 3> keys{0, 0, 9};
    k.update(4, keys, true, true);
    CHECK(same(k.update(4, keys, false, false), {Ev{9, false}}));
    CHECK(k.update(0, keys, false, false).empty());
}

TEST(keys_two_buttons_on_one_key) {
    MouseKeys k;
    const std::array<int, 3> keys{1, 1, 0};
    CHECK(same(k.update(1, keys, true, true), {Ev{1, true}}));
    CHECK(k.update(3, keys, true, true).empty());
    CHECK(k.update(2, keys, true, true).empty());  // the other one still holds F1
    CHECK(same(k.update(0, keys, true, true), {Ev{1, false}}));
}

TEST(keys_new_mapping_lets_the_old_key_go) {
    MouseKeys k;
    k.update(1, {1, 0, 0}, true, true);
    CHECK(same(k.update(1, {2, 0, 0}, true, true), {Ev{1, false}}));
    CHECK(k.update(1, {2, 0, 0}, true, true).empty());
    CHECK(same(k.releaseAll(), {}));
}

TEST(keys_release_all) {
    MouseKeys k;
    k.update(5, {4, 0, 6}, true, true);
    CHECK(same(k.releaseAll(), {Ev{4, false}, Ev{6, false}}));
    CHECK(k.releaseAll().empty());
}

TEST(background_sleep_only_when_idle_and_behind) {
    CHECK_EQ(backgroundSleepMs(true, false, false, 10), 10);
    CHECK_EQ(backgroundSleepMs(true, true, false, 10), 0);   // in front
    CHECK_EQ(backgroundSleepMs(true, true, true, 10), 10);   // minimized
    CHECK_EQ(backgroundSleepMs(false, false, false, 10), 0);  // messages waiting
    CHECK_EQ(backgroundSleepMs(true, false, false, 0), 0);    // switched off
}

// 3.0.11-beta.24: the driver's parts in the game's picture are the game's windows for its cursor - the world
// under them does not react to it (the owner: «курсор мишки поверх интерфейса
// реагирует еще и на мир»)
TEST(blocked_point_inside_a_part_is_its) {
    Blocked parts[2];
    parts[1] = Blocked{true, false, 100, 50, 300, 150};
    CHECK_EQ(blockedAt(parts, 2, 100, 50), 1);
    CHECK_EQ(blockedAt(parts, 2, 299, 149), 1);
    CHECK_EQ(blockedAt(parts, 2, 300, 100), -1);  // right and bottom outside, as Windows' rectangles
    CHECK_EQ(blockedAt(parts, 2, 200, 150), -1);
    CHECK_EQ(blockedAt(parts, 2, 99, 100), -1);
}

TEST(blocked_part_off_is_nothing) {
    Blocked parts[1];
    parts[0] = Blocked{false, false, 0, 0, 1000, 1000};
    CHECK_EQ(blockedAt(parts, 1, 10, 10), -1);
}

TEST(blocked_all_takes_the_whole_window) {  // the panel dragged, its menu open
    Blocked parts[3];
    parts[2] = Blocked{true, true, 0, 0, 10, 10};
    CHECK_EQ(blockedAt(parts, 3, 900, 700), 2);
}

TEST(blocked_first_part_wins) {
    Blocked parts[2];
    parts[0] = Blocked{true, false, 0, 0, 100, 100};
    parts[1] = Blocked{true, false, 50, 50, 200, 200};
    CHECK_EQ(blockedAt(parts, 2, 60, 60), 0);
    CHECK_EQ(blockedAt(parts, 2, 150, 150), 1);
}

TEST(blocked_game_cursor_in_its_own_pixels) {  // the game 800x450 in a client area of 1600x900
    Blocked parts[1];
    parts[0] = Blocked{true, false, 800, 400, 1000, 500};
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 450, 220, 800, 450), 0);   // 900, 440 in the window
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 399, 220, 800, 450), -1);  // 798
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 500, 220, 800, 450), -1);  // 1000: outside
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 450, 249, 800, 450), 0);   // 498
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 450, 250, 800, 450), -1);  // 500
}

TEST(blocked_game_cursor_same_size) {
    Blocked parts[1];
    parts[0] = Blocked{true, false, 10, 10, 20, 20};
    CHECK_EQ(blockedAtGame(parts, 1, 1024, 768, 15, 15, 1024, 768), 0);
    CHECK_EQ(blockedAtGame(parts, 1, 1024, 768, 20, 15, 1024, 768), -1);
}

TEST(blocked_game_cursor_no_sizes_is_nothing) {
    Blocked parts[1];
    parts[0] = Blocked{true, true, 0, 0, 10, 10};
    CHECK_EQ(blockedAtGame(parts, 1, 0, 900, 5, 5, 800, 450), -1);
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 5, 5, 0, 450), -1);
    CHECK_EQ(blockedAtGame(parts, 1, 1600, 900, 5, 5, 800, -1), -1);
}

// 3.0.11-beta.33 (the owner: «при нажатии на кнопку закрыть справочник персонаж идет по локации»): a press a part of
// the driver's took in its own window message (its ✕: the part gone before the game's next poll) stays out until it
// is let go, whatever is under the cursor at the poll
TEST(gate_a_press_a_part_took_stays_out) {
    MouseGate g;
    g.claim(kLmb);  // «Довідник»'s ✕: the window shut on the press, the game polls after it
    CHECK_EQ(g.filter(kLmb, true, false, 0).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 10).buttons, 0);  // held: still out
    CHECK_EQ(g.filter(0, true, false, 20).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 30).buttons, kLmb);  // a new press over the game
    CHECK_EQ(g.filter(0, true, false, 35).buttons, 0);
    g.claim(kLmb);  // a click let go before the poll: nothing left of it
    CHECK_EQ(g.filter(0, true, false, 40).buttons, 0);
    CHECK_EQ(g.filter(kLmb, true, false, 50).buttons, kLmb);
    g.claim(kRmb);  // the other button's own press only
    CHECK_EQ(g.filter(kLmb | kRmb, true, false, 60).buttons, kLmb);
}

// 3.0.11-beta.108: windows over each other - the parts named first (the stack's top first) before the rest: a drop on
// two windows goes to the one on top
TEST(gate_the_top_window_first) {
    Blocked parts[3];
    parts[1] = Blocked{true, false, 100, 100, 300, 200};  // a window under
    parts[2] = Blocked{true, false, 200, 150, 400, 300};  // a window over it, the overlap 200-300 x 150-200
    CHECK_EQ(blockedAt(parts, 3, 250, 175), 1);  // the parts' own order
    CHECK_EQ(blockedAtOrdered(parts, 3, {2, 1}, 250, 175), 2);
    CHECK_EQ(blockedAtOrdered(parts, 3, {2, 1}, 150, 120), 1);
    CHECK_EQ(blockedAtOrdered(parts, 3, {2}, 150, 120), 1);  // the rest after the named
    CHECK_EQ(blockedAtOrdered(parts, 3, {}, 500, 500), -1);
}

