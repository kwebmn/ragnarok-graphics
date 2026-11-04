#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::mercpacket {

// 3.0.11-beta.218 (R-S8BERR: the mercenary card «HP 256 / 0», «Викликів 359399424», «Контракт до 01.01 02:03»): the
// uaRO server fills ZC_MER_INIT (0x029b, 72 bytes) in the later layout - HP, its most, SP, its most as ints from 48,
// the contract's end at 64, loyalty at 68, summons at 70 (its int cut by the 72 bytes; kills and range past them) -
// while uaRO.exe (FUN_0059b950) reads the 2007 one: shorts at 48, 50, 52, 54, the end at 56, loyalty 60, summons 62,
// kills 66, range 70. The server's layout told by the contract's end: a Unix time at 64, none at 56. Laid out again in
// place before the game reads it; true when it was.
constexpr uint16_t kInit = 0x029B;
constexpr size_t kInitSize = 72;
bool laidOut(uint8_t* packet, size_t size);

// 3.0.11-beta.218 (R-S8BERR: «блок со скилами наемника пустой»): the game names skills only below 0x3FD (its table
// at 0x74B9AC) and the mercenaries' are 8201-8240 - their list from ZC_MER_SKILLINFO_LIST (0x029d: its length at 2,
// 37 bytes each - the id, its kind, level, SP, range, its Aegis name, upgradable), their names the driver's (the
// server's skill_db names)
constexpr uint16_t kSkills = 0x029D;
struct Skill {
    int id = 0, lv = 0, sp = 0, range = 0;
};
std::vector<Skill> skills(const uint8_t* packet, size_t size);
std::string skillName(int id);  // "" for another id

}  // namespace uagfx::mercpacket
