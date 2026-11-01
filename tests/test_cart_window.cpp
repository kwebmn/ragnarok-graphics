// 3.0.11-beta.154 (task 4 of the remaining windows, the merchant's: the cart): «Візок» - its title with the things
// carried of the most (the server's 0x0121), the cart's game facts the window works by
#include <string>

#include "core/cart_window.hpp"
#include "test.hpp"

using namespace uagfx;

// «Візок · 12/100» - the things in it of the most; nothing known yet (0 of 0): the name alone
TEST(cart_window_title_with_its_count) {
    CHECK(cartwindow::title("Візок", 12, 100) == "Візок \xC2\xB7 12/100");
    CHECK(cartwindow::title("Cart", 0, 100) == "Cart \xC2\xB7 0/100");
    CHECK(cartwindow::title("Візок", 0, 0) == "Візок");
}

// The game's: its cart window (0x28) at the manager's + 0x140, its press begins the game mode's drag 0x59 (a cart
// thing, kind 9 - the bag's drop takes it), the session's cart list + 0xF4C (its count + 0xF50)
TEST(cart_window_game_facts) {
    CHECK(cartwindow::kGameWindow == 0x28 && cartwindow::kManagerOffset == 0x140);
    CHECK(cartwindow::kDragMsg == 0x59 && cartwindow::kDragKind == 9);
    CHECK(cartwindow::kSessionList == 0xF4C && cartwindow::kSessionCount == 0xF50);
}
