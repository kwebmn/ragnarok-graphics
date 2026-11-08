#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::win::companionwin {

// 3.0.16-beta.31 (the owner: «делай» - the homunculus' and the mercenary's skills in the new style): their skills as
// the game keeps them and what its own skills window (UI windows 0x72 / 0x85: class 0x69B9D0 in its kinds 1 and 2,
// FUN_004bdd90) does with them, for the cards (win/homun_card, win/merc_card). uaRO.exe's: the lists (std::lists of
// the server's 0x0235 / 0x029d, FUN_005891e0: at 0x771438 / 0x771444, a node's record at +8 - 1, the id, its kind,
// level, SP, can rise, range - its name at +0x24), the levels they are used at (the session's +0x574 / +0x584, an int
// each by id - 8000 / - 0x2008: FUN_00639860 / FUN_006398b0); the game mode's SendMsg 0x7E uses one (its record, the
// level), 0xCB / 0x10B drag one (id, name, level - the drag's kinds 0xF / 0x11, as 0x51 a character's), 0x12 lets the
// drag go at the game's cursor (onto the hotbar: its drop FUN_0046dae0 takes both kinds). On the game's thread.
enum class Who : uint8_t { Homun, Merc };
struct GameSkill {
    uint32_t node = 0;
    int id = 0, level = 0, sp = 0;
    bool up = false;
};
std::vector<GameSkill> read(Who w);
int storedUse(Who w, int id);  // 0 none kept
bool setUse(Who w, int id, int level);
bool use(Who w, const GameSkill& k, int level);
const char* drop(Who w, const GameSkill& k, int level);  // what came of it, for the log

}  // namespace uagfx::win::companionwin
