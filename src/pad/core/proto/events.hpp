#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "pad/core/bytes.hpp"
#include "pad/core/proto/c2s.hpp"

namespace uapad {

struct InvEntry {
    uint16_t index = 0;  // protocol index (inventory slot + 2)
    uint16_t item = 0;
    uint16_t amount = 0;
    bool equip = false;
};

struct SkillInfo {
    uint16_t id = 0;
    uint32_t inf = 0;  // uAthena skill_get_inf bits: 1 enemy, 2 ground, 4 self, 16 support
    uint16_t lv = 0;
    uint16_t range = 0;
    std::string name;  // skill_db name, e.g. "AC_DOUBLE" (0x010f only)
};

struct EvAuthOk { Cell pos; };                       // 0x0073
struct EvMapChange { std::string map; Cell pos; };   // 0x0091, 0x0092
struct EvSelfWalk { Cell from; Cell to; };           // 0x0087
struct EvSetPos { uint32_t id = 0; Cell pos; };      // 0x0088, 0x01ff
struct EvVanish { uint32_t id = 0; uint8_t type = 0; };  // 0x0080 (type 1 = died)
struct EvActor {                                     // 0x0078, 0x007b, 0x007c, 0x022c
    uint32_t id = 0;
    uint16_t cls = 0;
    uint16_t hair = 0;
    uint16_t speed = 0;
    Cell from;
    Cell to;
    uint16_t opt1 = 0;  // the unit's state, as the client shows it (stone, poison, Sight...)
    uint16_t opt2 = 0;
    uint32_t option = 0;
};
struct EvActorOption { uint32_t id = 0; uint16_t opt1 = 0; uint16_t opt2 = 0; uint32_t option = 0; };  // 0x0119, 0x0229
struct EvActorMove { uint32_t id = 0; Cell from; Cell to; };      // 0x0086
struct EvPetInfo { uint8_t type = 0; uint32_t id = 0; };          // 0x01a4 (type 0 = our pet's id)
struct EvAction { uint32_t src = 0; uint32_t dst = 0; uint8_t type = 0; };  // 0x008a (2 sit, 3 stand)
struct EvSkillHit { uint16_t skill = 0; uint32_t src = 0; uint32_t dst = 0; };  // 0x01de, 0x0114, 0x0115
struct EvGroundItem { uint32_t id = 0; uint16_t item = 0; Cell pos; };  // 0x009d, 0x009e
struct EvGroundItemGone { uint32_t id = 0; };                     // 0x00a1
struct EvInventory { std::vector<InvEntry> items; };              // 0x01ee (stackable items)
struct EvItemAdd { InvEntry entry; };                             // 0x00a0
struct EvItemRemove { uint16_t index = 0; uint16_t amount = 0; }; // 0x00af
struct EvItemUsed {                                               // 0x01c8, 0x00a8
    uint16_t index = 0;
    uint16_t remaining = 0;
    uint32_t user = 0;  // 0 when the packet only goes to us (0x00a8)
    bool ok = false;
};
struct EvSkills { std::vector<SkillInfo> skills; };               // 0x010f
struct EvSkillUp { uint16_t id = 0; uint16_t lv = 0; uint16_t range = 0; };  // 0x010e
struct EvHotkeys { std::array<Hotkey, 27> keys{}; };              // 0x02b9
struct EvResurrect { uint32_t id = 0; };                          // 0x0148
struct EvSelfSpeed { uint16_t speed = 0; };                       // 0x00b0 SP_SPEED, ms per cell
struct EvActorName { uint32_t id = 0; std::string name; };        // 0x0095, 0x0195

using Event = std::variant<EvAuthOk, EvMapChange, EvSelfWalk, EvSetPos, EvVanish, EvActor, EvActorMove, EvPetInfo,
                           EvAction, EvSkillHit, EvGroundItem, EvGroundItemGone, EvInventory, EvItemAdd,
                           EvItemRemove, EvItemUsed, EvSkills, EvSkillUp, EvHotkeys, EvResurrect, EvSelfSpeed,
                           EvActorName, EvActorOption>;

}  // namespace uapad
