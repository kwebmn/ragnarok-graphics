// 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов, плюс появляются те которых
// на чаре нету»): each character's own bar, kept by its name (core/hotbar_bars)
#include <set>
#include <string>

#include "core/hotbar_bars.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(hotbar_bars_file_round_trip) {
    hotbar::Bars bars;
    hotbar::Bar a;
    a.slots[0] = "##AC_DOUBLE";
    a.levels[0] = 10;
    a.slots[1] = "501";
    a.slots[26] = "##AL_HEAL";
    a.levels[26] = 5;
    bars["\xB1\xE8\xC3\xB6 Kim"] = a;  // a name in the game's code page, a space in it
    hotbar::Bar b;
    b.slots[8] = "607";
    bars["Smith"] = b;
    const hotbar::Bars back = hotbar::parseBars(hotbar::barsText(bars));
    CHECK_EQ(back.size(), size_t(2));
    CHECK(back.count("\xB1\xE8\xC3\xB6 Kim") && back.at("\xB1\xE8\xC3\xB6 Kim") == a);
    CHECK(back.count("Smith") && back.at("Smith") == b);
    CHECK(hotbar::parseBars("garbage\nzz\t1\n").empty());
    // the game's "nothing" is an empty cell
    hotbar::Bar n;
    n.slots[3] = "nothing";
    CHECK(hotbar::parseBars(hotbar::barsText({{"X", n}})).at("X").slots[3].empty());
}

TEST(hotbar_bars_drop_unlearned) {
    hotbar::Bar bar;
    bar.slots[0] = "##AC_DOUBLE";
    bar.levels[0] = 10;
    bar.slots[1] = "##MG_FIREBOLT";  // another character's
    bar.levels[1] = 3;
    bar.slots[2] = "501";            // an item stays
    const std::set<std::string> learned = {"AC_DOUBLE", "NV_BASIC"};
    CHECK_EQ(hotbar::dropUnlearned(bar, learned), 1);
    CHECK(bar.slots[0] == "##AC_DOUBLE" && bar.slots[1].empty() && bar.levels[1] == 0 && bar.slots[2] == "501");
    CHECK_EQ(hotbar::dropUnlearned(bar, learned), 0);
}

// 3.0.11-beta.147 (the owner: a cell's icon dragged to another cell, off the bar to take it away): two slots swapped
// with their levels (an empty one: moved there); a slot emptied; a slot out of the bar changes nothing
TEST(hotbar_bars_swap_and_empty) {
    hotbar::Bar b;
    b.slots[0] = "##MG_FIREWALL";
    b.levels[0] = 10;
    b.slots[5] = "501";
    hotbar::swapSlots(b, 0, 5);
    CHECK(b.slots[0] == "501" && b.levels[0] == 0 && b.slots[5] == "##MG_FIREWALL" && b.levels[5] == 10);
    hotbar::swapSlots(b, 5, 20);
    CHECK(b.slots[5].empty() && b.levels[5] == 0 && b.slots[20] == "##MG_FIREWALL" && b.levels[20] == 10);
    hotbar::emptySlot(b, 20);
    CHECK(b.slots[20].empty() && b.levels[20] == 0);
    const hotbar::Bar same = b;
    hotbar::swapSlots(b, -1, 0);
    hotbar::swapSlots(b, 0, hotbar::kBarSlots);
    hotbar::emptySlot(b, hotbar::kBarSlots);
    CHECK(b == same);
}
