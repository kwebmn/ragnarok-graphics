#include "core/windows_apart.hpp"

#include "test.hpp"

using namespace uagfx;
namespace wa = uagfx::windowsapart;

// 3.0.24-beta.1 (the owner: old and new windows «не конфликтовали, особенно наложение»): the game's window that opens
// over a panel goes to the nearest free place; free already, or nowhere free - as it is
TEST(windows_apart_moves_a_game_window_off_the_panels) {
    const std::vector<wa::Box> panels = {{0, 0, 300, 180}, {0, 600, 400, 168}};  // the character block, the chat
    bool moved = false;
    const wa::Box inventory{0, 150, 280, 170};
    const wa::Box to = wa::apart(inventory, panels, 1024, 768, moved);
    CHECK(moved);
    for (const wa::Box& p : panels) CHECK(!wa::overlaps(to, p));
    CHECK(to.x + to.w <= 1024 && to.y + to.h <= 768);
    CHECK(to.y == 184 && to.x == 0);  // the nearest: just under the block
    const wa::Box clear{500, 200, 280, 170};
    CHECK(wa::apart(clear, panels, 1024, 768, moved).x == 500 && !moved);
    const wa::Box huge{0, 0, 1024, 700};  // no free place: as it is
    const wa::Box same = wa::apart(huge, panels, 1024, 768, moved);
    CHECK(!moved && same.y == 0);
}
