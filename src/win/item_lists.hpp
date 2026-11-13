#pragma once
#include <map>

namespace uagfx::win::itemlists {

// 3.0.18-beta.242 (the reference's cards: «у сумці N · у візку N · на складі N», the craft's have and missing, a quest's
// «2 з 5»): one of the session's item lists as counts by the item's number - the bag's (+0xF1C, its worn gear left out
// but the arrows), the cart's (+0xF4C), the storage's (+0xF40; the game keeps it while it is open). Each node's
// ITEM_INFO at +8: the place +8, worn +0xC, the count +0x10, the number's words at +0x30 (its length +0x34). On the
// game's thread.
enum class List { Bag, Cart, Storage };
std::map<int, int> counts(List list, bool* known = nullptr);  // known: the game has the list (in the world)

}  // namespace uagfx::win::itemlists
