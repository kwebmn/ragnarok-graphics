#pragma once
#include <array>
#include <map>
#include <set>
#include <string>

namespace uagfx::hotbar {

// 3.0.11-beta.78 (the owner: «на панели скилов не те скилы появляются, с других аккаунтов, плюс появляются те которых
// на чаре нету»): uaRO.exe keeps its hotkeys bar in the registry by the character's place on the selection screen
// (SITEM<slot><langtype><page x 3 + index>, FUN_004d0500) - not by the account: every account's first character
// shares one bar. The driver keeps each character's own, by its name (unique on a server), in
// uaro-graphics\hotbar-bars.txt: a line a character - its name and its 27 slots as the game's session has them (hex),
// with their levels.
constexpr int kBarSlots = 27;
struct Bar {
    std::array<std::string, kBarSlots> slots;  // "##AC_DOUBLE" a skill, "501" an item, "" none
    std::array<int, kBarSlots> levels{};
    bool operator==(const Bar& o) const { return slots == o.slots && levels == o.levels; }
    bool operator!=(const Bar& o) const { return !(*this == o); }
};
using Bars = std::map<std::string, Bar>;  // by the character's name (the game's code page)

Bars parseBars(const std::string& text);  // a line it cannot read skipped
std::string barsText(const Bars& bars);
// Its skills the character has not learned (`learned`: their AEGIS names) taken out; how many
int dropUnlearned(Bar& bar, const std::set<std::string>& learned);

}  // namespace uagfx::hotbar
