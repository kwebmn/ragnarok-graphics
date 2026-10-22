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
