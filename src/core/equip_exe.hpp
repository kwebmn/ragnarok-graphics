#pragma once
#include <cstdint>

// 3.0.18-beta.174 (R-ZGN2US, the beta tester's crash at a map's entry - a read at 0x5 in uaRO.exe 0x4ffb7e): the
// game's FUN_00628e50 (thiscall: the session 0x771148, an item's info) puts a worn item on the session's places by the
// bits 0-9 of the info's +0xc - their indexes into a local of three ints, never counted: a fourth wrote over the
// saved EBP, a fifth over the return address, a sixth (index 5) over its argument, then read as the info. With none
// of them (only bits it does not know - a costume's - and not the arrows' 0x8000 alone) the item went to place -1,
// before the places' array. Its two calls: the equipment's list (0x00a4, 0x582542) and a wear's answer (0x00aa,
// 0x628c6e) - win/equip_patch puts a guard between.
namespace uagfx::equip_exe {

constexpr uint32_t kWear = 0x628E50;
constexpr uint32_t kListCall = 0x582542, kAnswerCall = 0x628C6E;
constexpr uint32_t kMaskAt = 0xC, kIndexAt = 0x8, kPlacesAt = 0x4;  // the info's worn mask, its index, its places
constexpr uint32_t kAmmo = 0x8000;

// True: the game can take an item worn at `mask` as it is - the arrows' place alone, or one to three of the ten
bool wearable(uint32_t mask);

}  // namespace uagfx::equip_exe
