#pragma once
#include <cstdint>
#include <string>

namespace uagfx::weightrules {

// 3.0.11-beta.202 (the tester: «бреше в чат на 50%», «тут теж на 70% показує іконку 50»): uAthena on uaRO stops HP and
// SP coming back at 70 % of the most weight (natural_heal_weight_rate 70), not the game's 50 %. The game's own line for
// it (msgstringtable's 0x132) said 50 and came at 50 % (its weight update, FUN_00582700: cmp esi / edi, 0x32 at
// 0x582E97 and 0x582E9C - made 0x46); the status icon of it (SI_WEIGHT50, 35) the server sends at 70 %
constexpr int kLine = 0x132;
constexpr uint32_t kFirstCmp = 0x582E97;  // 83 FE 32 7C 38 83 FF 32 7D 33
constexpr int kPerCent = 70;
// The line's «50» before its per cent sign made «70» in place (the same length); false: none such
bool seventy(std::string& text);

}  // namespace uagfx::weightrules
